import os, sys, json, shutil, subprocess, threading, atexit, mimetypes, re
from pathlib import Path
from starlette.applications import Starlette
from starlette.responses import JSONResponse, FileResponse, Response, StreamingResponse
from starlette.routing import Route, Mount
from starlette.staticfiles import StaticFiles
from . import WEB_DIR, DATA_DIR, ROOT_DIR, doctor, store, jobs, piecemodel, passages, takes, spotcheck, edits

threading.Thread(target=doctor.run, daemon=True).start()

# ------------------------------------------------------------------ worker process
_worker = None
def start_worker():
    global _worker
    try:
        from . import seed; seed.run()
    except Exception as e:
        import logging; logging.getLogger("seed").exception("seeding failed: %s", e)
    if os.environ.get("PRACTICE_NO_WORKER"): return
    _worker = subprocess.Popen([sys.executable, "-W", "ignore", "-m", "practice.worker"], cwd=str(Path(__file__).resolve().parent.parent))
    atexit.register(lambda: _worker and _worker.poll() is None and _worker.terminate())

def J(obj, status=200): return JSONResponse(obj, status_code=status)

# ------------------------------------------------------------------ pieces
def piece_summary(p):
    recs = store.q("SELECT id,name,kind,status FROM recordings WHERE piece_id=? ORDER BY sort,id", (p["id"],))
    return dict(p, recordings=len(recs))

async def list_pieces(req):
    return J([piece_summary(p) for p in store.q("SELECT * FROM pieces ORDER BY updated DESC, id DESC")])

async def create_piece(req):
    form = await req.form()
    f = form.get("pdf"); title = (form.get("title") or "").strip(); composer = (form.get("composer") or "").strip()
    if f is None: return J({"error": "no PDF"}, 400)
    pf = form.get("page_from"); pt = form.get("page_to")
    pid = store.ex("INSERT INTO pieces(title,composer,created,updated,status,page_from,page_to) VALUES(?,?,?,?,?,?,?)",
                   (title or Path(f.filename).stem, composer, store.now(), store.now(), "queued",
                    int(pf) if pf else None, int(pt) if pt else None))
    d = store.piece_dir(pid); dst = d / "score.pdf"
    with open(dst, "wb") as out: shutil.copyfileobj(f.file, out)
    store.ex("UPDATE pieces SET pdf=? WHERE id=?", (str(dst), pid))
    jobs.enqueue("score", pid)
    return J({"id": pid})

async def update_piece(req):
    pid = int(req.path_params["pid"]); body = await req.json()
    for k in ("title", "composer", "perf_date"):
        if k in body: store.ex(f"UPDATE pieces SET {k}=?, updated=? WHERE id=?", (body[k], store.now(), pid))
    return J({"ok": True})

async def delete_piece(req):
    store.delete_piece(int(req.path_params["pid"])); return J({"ok": True})

def _alignment_view(pid, r, consensus, labels):
    a = store.read_json(store.rec_dir(r["id"]) / "alignment.json")
    if not a: return None
    c = consensus.get(str(r["id"])) if consensus else None
    occ = []
    near = set()                                     # the user's anchors (and 2 bars either side) are trusted as they are
    for k in a.get("anchored", []): near.update(range(k - 2, k + 3))
    for k, o in enumerate(a["occ"]):
        sp = c["spread"][k] if c and k < len(c["spread"]) else None
        # the cross-checked (median) time, unless the recordings disagree wildly there: then this recording's own
        use = c and k < len(c["times"]) and k not in near and (sp is None or sp < 0.35 * max(0.2, o[3] - o[2]))
        t = c["times"][k] if use else o[2]
        occ.append([o[0], o[1], round(t, 3), round(o[3], 3), None if sp is None else round(sp, 3)])
    for i0, i1, t0, t1 in a["runs"]:                 # keep times increasing inside a run
        for k in range(i0 + 1, i1):
            if occ[k][2] <= occ[k - 1][2] + 0.02: occ[k][2] = a["occ"][k][2] if a["occ"][k][2] > occ[k - 1][2] + 0.02 else occ[k - 1][2] + 0.02
    # the end of each occurrence = start of the next one in the same run
    for i0, i1, t0, t1 in a["runs"]:
        for k in range(i0, i1):
            occ[k][3] = occ[k + 1][2] if k + 1 < i1 else t1
    return dict(occ=occ, runs=a["runs"], forms=a.get("forms"), shift=a.get("shift", 0), gaps=a.get("gaps"), anchored=a.get("anchored", []),
                anchors=len(store.read_json(store.rec_dir(r["id"]) / "anchors.json") or []),
                stale=a.get("structure_version") != (store.read_json(store.piece_dir(pid) / "structure.json") or {}).get("version"))

async def get_piece(req):
    pid = int(req.path_params["pid"]); p = store.q1("SELECT * FROM pieces WHERE id=?", (pid,))
    if not p: return J({"error": "not found"}, 404)
    score, structure = piecemodel.load(pid)
    out = dict(piece=p, score=None, structure=structure, labels={}, recordings=[], vision=bool(store.setting("deepseek_key")))
    if score:
        bars = [dict(id=b["id"], page=b["page"], system=b["system"], idx=b["idx"], box=b["box"], conf=b["conf"]) for b in score["bars"]]
        out["score"] = dict(pages=score["pages"], bars=bars,
                            systems=[dict(page=s["page"], idx=s["idx"], x_left=s["x_left"], y0=s["y0"], y1=s["y1"], barlines=s["barlines"]) for s in score["systems"]])
        if structure:
            out["labels"] = piecemodel.labels(score, structure)
            mv, lists = piecemodel.movement_of(score, structure); out["movement_of"] = mv
            ck = store.read_json(store.piece_dir(pid) / "check.json")
            if ck:                                   # flags recomputed live, so fixing a line clears its flag
                from . import vision
                printed = {(x["page"], x["system"]): x["number"] for x in ck.get("printed", [])}
                flags, lines, reliable = vision.compare(score, structure, printed)
                out["check"] = dict(flags=flags, read=len(printed), lines=len(lines), reliable=reliable, created=ck.get("created"))
    cons = store.read_json(store.piece_dir(pid) / "consensus.json", {}) or {}
    for r in store.q("SELECT * FROM recordings WHERE piece_id=? ORDER BY sort,id", (pid,)):
        rr = dict(r); rr["audio"] = f"/api/recordings/{r['id']}/audio?v={Path(r['path']).suffix.lstrip('.')}" if r["path"] else None
        rr["path"] = None
        rr["alignment"] = _alignment_view(pid, r, cons, out["labels"]) if score else None
        ch = store.read_json(store.rec_dir(r["id"]) / "chapters.json"); rr["chapters"] = ch
        out["recordings"].append(rr)
    return J(out)

async def page_image(req):
    pid = int(req.path_params["pid"]); n = int(req.path_params["n"])
    f = store.piece_dir(pid) / "pages" / f"p{n:03d}.png"
    if not f.exists(): return Response(status_code=404)
    return FileResponse(f, headers={"Cache-Control": "max-age=86400"})

async def save_structure(req):
    """Movements, meters, markup, deleted bars. Any change re-aligns the piece's recordings."""
    pid = int(req.path_params["pid"]); body = await req.json()
    d = store.piece_dir(pid); old = store.read_json(d / "structure.json", {}) or {}
    body["version"] = int(old.get("version", 1)) + 1
    store.write_json(d / "structure.json", body)
    for r in store.q("SELECT id FROM recordings WHERE piece_id=?", (pid,)): jobs.enqueue("align", pid, r["id"])
    return J({"ok": True, "version": body["version"]})

async def save_barlines(req):
    """Edit the barlines of one system: body {page, idx, barlines:[x...]} (normalized x of each bar's right edge)."""
    pid = int(req.path_params["pid"]); body = await req.json()
    d = store.piece_dir(pid); score = store.read_json(d / "score.json"); structure = store.read_json(d / "structure.json")
    for s in score["systems"]:
        if s["page"] == body["page"] and s["idx"] == body["idx"]:
            old_n = len(s["barlines"]); s["barlines"] = sorted(float(x) for x in body["barlines"]); s["edited"] = True
            s["conf"] = [1.0] * len(s["barlines"])
            if len(s["barlines"]) != old_n: s["per_bar"] = s["per_bar"] if (s.get("per_bar") and len(s["per_bar"]) == len(s["barlines"])) else None
    piecemodel.rebuild_bars(score); store.write_json(d / "score.json", score)
    # bar ids of this system may have shifted: drop markup/deletions that no longer exist
    ids = {b["id"] for b in score["bars"]}
    structure["markup"] = {k: v for k, v in structure.get("markup", {}).items() if k in ids}
    structure["deleted"] = [b for b in structure.get("deleted", []) if b in ids]
    for m in structure["movements"]:
        if m["start"] not in ids: m["start"] = score["bars"][0]["id"]
    structure["version"] = int(structure.get("version", 1)) + 1
    store.write_json(d / "structure.json", structure)
    for r in store.q("SELECT id FROM recordings WHERE piece_id=?", (pid,)): jobs.enqueue("align", pid, r["id"])
    return J({"ok": True})

async def check_score(req):
    from . import vision
    pid = int(req.path_params["pid"])
    if not vision.configured(): return J({"error": "Add a DeepSeek API key in Settings first."}, 400)
    jobs.enqueue("check", pid); return J({"ok": True})

async def reread_score(req):
    pid = int(req.path_params["pid"]); d = store.piece_dir(pid)
    for f in ("score.json", "structure.json"): (d / f).unlink(missing_ok=True)
    jobs.enqueue("score", pid); store.ex("UPDATE pieces SET status='queued' WHERE id=?", (pid,))
    return J({"ok": True})

# ------------------------------------------------------------------ recordings
def _next_sort(pid):
    r = store.q1("SELECT MAX(sort) AS m FROM recordings WHERE piece_id=?", (pid,))
    return (r["m"] if r and r["m"] is not None else -1) + 1

async def add_recording(req):
    pid = int(req.path_params["pid"])
    ct = req.headers.get("content-type", "")
    if ct.startswith("application/json"):
        body = await req.json(); url = (body.get("url") or "").strip()
        if not url: return J({"error": "no URL"}, 400)
        rid = store.ex("INSERT INTO recordings(piece_id,name,kind,source,url,created,status,sort) VALUES(?,?,?,?,?,?,?,?)",
                       (pid, body.get("name") or "", body.get("kind") or "reference", "youtube", url, store.now(), "queued", _next_sort(pid)))
        jobs.enqueue("download", pid, rid); return J({"id": rid})
    form = await req.form(); f = form.get("file")
    if f is None: return J({"error": "no file"}, 400)
    source = "take" if form.get("source") == "take" else "file"
    num = lambda k: int(form.get(k)) if (form.get(k) or "").strip().isdigit() else None
    sec_a, sec_b = form.get("sec_a") or None, form.get("sec_b") or None
    if num("passage_id") and not sec_a:                # a passage take covers the passage's bars
        ps = store.q1("SELECT a,b FROM passages WHERE id=?", (num("passage_id"),))
        if ps: sec_a, sec_b = ps["a"], ps["b"]
    rid = store.ex("INSERT INTO recordings(piece_id,name,kind,source,created,status,sort,passage_id,label,piano,technique,rating,sec_a,sec_b) "
                   "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                   (pid, form.get("name") or Path(f.filename).stem, form.get("kind") or "reference", source, store.now(), "queued",
                    _next_sort(pid), num("passage_id"), form.get("label") or None, form.get("piano") or None,
                    form.get("technique") or None, num("rating"), sec_a, sec_b or sec_a))
    dst = store.rec_dir(rid) / ("audio" + Path(f.filename).suffix.lower())
    with open(dst, "wb") as out: shutil.copyfileobj(f.file, out)
    store.ex("UPDATE recordings SET path=? WHERE id=?", (str(dst), rid))
    jobs.enqueue("features", pid, rid); return J({"id": rid})

async def update_recording(req):
    rid = int(req.path_params["rid"]); body = await req.json(); realign = False
    for k in ("name", "kind", "sort", "label", "piano", "technique", "rating", "rerating"):
        if k in body: store.ex(f"UPDATE recordings SET {k}=? WHERE id=?", (body[k], rid))
    if "passage_id" in body: store.ex("UPDATE recordings SET passage_id=? WHERE id=?", (body["passage_id"], rid)); realign = True
    for k in ("range_start", "range_end"):
        if k in body: store.ex(f"UPDATE recordings SET {k}=? WHERE id=?", (body[k], rid)); realign = True
    if realign:
        r = store.q1("SELECT piece_id FROM recordings WHERE id=?", (rid,)); jobs.enqueue("align", r["piece_id"], rid)
    return J({"ok": True})

async def delete_recording(req):
    rid = int(req.path_params["rid"]); r = store.q1("SELECT piece_id FROM recordings WHERE id=?", (rid,))
    store.delete_recording(rid)
    if r: jobs.enqueue("consensus", r["piece_id"])
    return J({"ok": True})

async def move_recording(req):
    """Move a recording up or down the list (its number key follows)."""
    rid = int(req.path_params["rid"]); d = int((await req.json()).get("d") or 0)
    r = store.q1("SELECT piece_id FROM recordings WHERE id=?", (rid,))
    if not r: return J({"error": "not found"}, 404)
    ids = [x["id"] for x in store.q("SELECT id FROM recordings WHERE piece_id=? ORDER BY sort,id", (r["piece_id"],))]
    i = ids.index(rid); j = max(0, min(len(ids) - 1, i + d)); ids.insert(j, ids.pop(i))
    for k, x in enumerate(ids): store.ex("UPDATE recordings SET sort=? WHERE id=?", (k, x))
    return J({"ok": True})

async def realign(req):
    rid = int(req.path_params["rid"]); r = store.q1("SELECT piece_id FROM recordings WHERE id=?", (rid,))
    jobs.enqueue("align", r["piece_id"], rid); return J({"ok": True})

async def audio(req):
    """Audio with HTTP Range support (needed for instant seeking)."""
    rid = int(req.path_params["rid"]); r = store.q1("SELECT path FROM recordings WHERE id=?", (rid,))
    if not r or not r["path"]: return Response(status_code=404)
    path = Path(r["path"]); v = req.query_params.get("v")
    if v and path.suffix.lstrip(".") != v and path.with_suffix("." + v).exists():
        path = path.with_suffix("." + v)             # the file this page started playing (a take converted since)
    if not path.exists(): return Response(status_code=404)
    size = path.stat().st_size
    ctype = {".m4a": "audio/mp4", ".mp4": "audio/mp4", ".webm": "audio/webm", ".mp3": "audio/mpeg", ".ogg": "audio/ogg",
             ".opus": "audio/ogg", ".wav": "audio/wav", ".flac": "audio/flac"}.get(path.suffix.lower(), "application/octet-stream")
    rng = req.headers.get("range")
    if not rng:
        return FileResponse(path, media_type=ctype, headers={"Accept-Ranges": "bytes"})
    m = re.match(r"bytes=(\d*)-(\d*)", rng); a = int(m.group(1) or 0); b = int(m.group(2)) if m.group(2) else size - 1
    b = min(b, size - 1); length = b - a + 1
    def gen():
        with open(path, "rb") as fh:
            fh.seek(a); left = length
            while left > 0:
                chunk = fh.read(min(262144, left))
                if not chunk: break
                left -= len(chunk); yield chunk
    return StreamingResponse(gen(), status_code=206, media_type=ctype,
                             headers={"Accept-Ranges": "bytes", "Content-Range": f"bytes {a}-{b}/{size}", "Content-Length": str(length)})

# ------------------------------------------------------------------ misc
async def api_jobs(req): return J(jobs.active())
async def api_doctor(req): return J(doctor.STATE)
async def index(req): return FileResponse(WEB_DIR / "index.html", headers={"Cache-Control": "no-cache"})
async def setup_page(req): return FileResponse(WEB_DIR / "setup.html")

routes = [
    Route("/", index), Route("/setup", setup_page),
    Route("/api/doctor", api_doctor), Route("/api/jobs", api_jobs),
    Route("/api/pieces", list_pieces), Route("/api/pieces", create_piece, methods=["POST"]),
    Route("/api/pieces/{pid:int}", get_piece), Route("/api/pieces/{pid:int}", update_piece, methods=["PATCH"]),
    Route("/api/pieces/{pid:int}", delete_piece, methods=["DELETE"]),
    Route("/api/pieces/{pid:int}/pages/{n:int}", page_image),
    Route("/api/pieces/{pid:int}/structure", save_structure, methods=["PUT"]),
    Route("/api/pieces/{pid:int}/barlines", save_barlines, methods=["PUT"]),
    Route("/api/pieces/{pid:int}/reread", reread_score, methods=["POST"]),
    Route("/api/pieces/{pid:int}/check", check_score, methods=["POST"]),
    Route("/api/pieces/{pid:int}/recordings", add_recording, methods=["POST"]),
    Route("/api/recordings/{rid:int}", update_recording, methods=["PATCH"]),
    Route("/api/recordings/{rid:int}", delete_recording, methods=["DELETE"]),
    Route("/api/recordings/{rid:int}/realign", realign, methods=["POST"]),
    Route("/api/recordings/{rid:int}/move", move_recording, methods=["POST"]),
    Route("/api/recordings/{rid:int}/audio", audio),
    *passages.ROUTES, *takes.ROUTES, *spotcheck.ROUTES, *edits.ROUTES,
    Mount("/web", StaticFiles(directory=str(WEB_DIR)), name="web"),
]
from contextlib import asynccontextmanager
@asynccontextmanager
async def lifespan(app):
    start_worker()
    yield
    if _worker and _worker.poll() is None: _worker.terminate()
app = Starlette(routes=routes, lifespan=lifespan)
