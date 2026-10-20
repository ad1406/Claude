"""Whole-line edits for scores the reader got wrong: a line's height, removing a line, adding a missed line.

Barlines inside a line are edited elsewhere (server.save_barlines). Here:
  * extent: the top/bottom of a line (bars take their height from it);
  * remove: every bar of the line is marked "not a bar" (reversible: the same call again restores them);
  * add: the user draws a box around a line the reader missed; the score reader runs on just that crop (a job),
    its barlines and notes become a new line, and the ids of the lines below it on that page are renamed.
Every edit bumps the structure version, so recordings are re-aligned in the background.
"""
import re, shutil
from starlette.responses import JSONResponse
from starlette.routing import Route
from . import store, jobs, piecemodel

def J(obj, status=200): return JSONResponse(obj, status_code=status)

def _load(pid):
    d = store.piece_dir(pid)
    return d, store.read_json(d / "score.json"), store.read_json(d / "structure.json")

def _save(pid, d, score, st):
    st["version"] = int(st.get("version", 1)) + 1
    store.write_json(d / "score.json", score); store.write_json(d / "structure.json", st)
    for r in store.q("SELECT id FROM recordings WHERE piece_id=?", (pid,)): jobs.enqueue("align", pid, r["id"])

def _system(score, page, idx):
    return next((s for s in score["systems"] if s["page"] == page and s["idx"] == idx), None)

# ------------------------------------------------------------------ height
async def api_extent(req):
    """body {page, idx, y0, y1} (fractions of the page height)."""
    pid = int(req.path_params["pid"]); body = await req.json()
    d, score, st = _load(pid); s = _system(score, int(body["page"]), int(body["idx"]))
    if not s: return J({"error": "no such line"}, 404)
    y0, y1 = sorted((float(body["y0"]), float(body["y1"])))
    if y1 - y0 < 0.01: return J({"error": "too small"}, 400)
    s["y0"], s["y1"] = max(0.0, y0), min(1.0, y1); s["edited"] = True
    piecemodel.rebuild_bars(score); _save(pid, d, score, st)
    return J({"ok": True})

# ------------------------------------------------------------------ remove / restore
async def api_remove(req):
    """body {page, idx}: mark all bars of the line as not bars, or restore them if they all are already."""
    pid = int(req.path_params["pid"]); body = await req.json()
    d, score, st = _load(pid)
    ids = [b["id"] for b in score["bars"] if b["page"] == int(body["page"]) and b["system"] == int(body["idx"])]
    if not ids: return J({"error": "no such line"}, 404)
    dele = set(st.get("deleted") or [])
    removed = not all(i in dele for i in ids)
    if removed: dele |= set(ids)
    else: dele -= set(ids)
    st["deleted"] = sorted(dele)
    if removed:                                       # a movement can't start on a removed bar: the next kept one
        order = [b["id"] for b in score["bars"]]; last = order.index(ids[-1])
        after = [b for b in order[last + 1:] if b not in dele]
        for m in st.get("movements", []):
            if m["start"] in ids and after: m["start"] = after[0]
    _save(pid, d, score, st)
    return J({"ok": True, "removed": removed})

# ------------------------------------------------------------------ add a missed line
async def api_add(req):
    """body {page, box: [x0, y0, x1, y1]} (fractions of the page): queue reading that crop as a new line."""
    pid = int(req.path_params["pid"]); body = await req.json()
    box = [float(x) for x in body.get("box") or []]
    if len(box) != 4 or box[2] - box[0] < 0.1 or box[3] - box[1] < 0.02: return J({"error": "draw a box around the whole line"}, 400)
    jobs.enqueue("addline", pid, params=dict(page=int(body["page"]), box=box), dedupe=False)
    return J({"ok": True})

def rename_ids(pid, ren):
    """Rename bar ids everywhere they are referenced: structure (movements, markup and its repeat targets, deleted),
    passages, flags, cues, anchors. ren: old id -> new id (ids not in it are kept)."""
    f = lambda x: ren.get(x, x) if isinstance(x, str) else x
    d = store.piece_dir(pid); st = store.read_json(d / "structure.json")
    for m in st.get("movements", []): m["start"] = f(m["start"])
    st["markup"] = {f(k): {kk: (f(vv) if kk == "repeat_to" else vv) for kk, vv in v.items()} for k, v in (st.get("markup") or {}).items()}
    st["deleted"] = [f(x) for x in st.get("deleted") or []]
    store.write_json(d / "structure.json", st)
    for p in store.q("SELECT id,a,b FROM passages WHERE piece_id=?", (pid,)):
        store.ex("UPDATE passages SET a=?, b=? WHERE id=?", (f(p["a"]), f(p["b"]), p["id"]))
    for x in store.q("SELECT id,bar FROM flags WHERE piece_id=? AND bar IS NOT NULL", (pid,)):
        store.ex("UPDATE flags SET bar=? WHERE id=?", (f(x["bar"]), x["id"]))
    for x in store.q("SELECT id,sec_a,sec_b FROM recordings WHERE piece_id=? AND sec_a IS NOT NULL", (pid,)):
        store.ex("UPDATE recordings SET sec_a=?, sec_b=? WHERE id=?", (f(x["sec_a"]), f(x["sec_b"]), x["id"]))
    for x in store.q("SELECT id,bar FROM cues WHERE piece_id=?", (pid,)):
        store.ex("UPDATE cues SET bar=? WHERE id=?", (f(x["bar"]), x["id"]))
    for r in store.q("SELECT id FROM recordings WHERE piece_id=?", (pid,)):
        af = store.rec_dir(r["id"]) / "anchors.json"; an = store.read_json(af)
        if an: store.write_json(af, [dict(a, bar=f(a["bar"])) for a in an])

def do_addline(j, progress=lambda f, m="": None):
    """The job: crop, read with the score reader, insert the line(s) found, renumber the lines below."""
    from PIL import Image
    from .engine import score as S
    pid = j["piece_id"]; pg = j["params"]["page"]; x0, y0, x1, y1 = j["params"]["box"]
    d, score, st = _load(pid)
    src = d / "pages" / f"p{pg:03d}.png"
    im = Image.open(src); W, H = im.size
    # a margin around the box, so the staff lines are whole
    cx0, cy0 = max(0, int((x0 - 0.01) * W)), max(0, int((y0 - 0.02) * H))
    cx1, cy1 = min(W, int((x1 + 0.01) * W)), min(H, int((y1 + 0.02) * H))
    work = d / "omr" / "added"; work.mkdir(parents=True, exist_ok=True)
    n = len(list(work.glob("*.png"))); crop = work / f"p{pg:03d}_{n}.png"
    im.crop((cx0, cy0, cx1, cy1)).save(crop)
    progress(0.1, "reading the new line (OMR)")
    S.run_homr(crop, crop.with_suffix(".log"))
    progress(0.8, "finding its barlines")
    found = S.analyse_page(crop, crop.with_suffix(".musicxml"), crop.with_suffix(".txt"), crop.with_suffix(".log"))
    if not found: raise RuntimeError("no staff found in the box: draw it around the whole line, both staves")
    cw, ch = (cx1 - cx0) / W, (cy1 - cy0) / H
    X = lambda v: cx0 / W + v * cw; Y = lambda v: cy0 / H + v * ch
    new_systems = []
    for s in found:
        b = s["box"]
        new_systems.append(dict(page=pg, idx=-1, box=[X(b[0]), Y(b[1]), X(b[2]), Y(b[3])], x_left=X(s["x_left"]), y0=Y(s["y0"]), y1=Y(s["y1"]),
                                barlines=[X(bb["x1"]) for bb in s["bars"]], conf=[bb["conf"] for bb in s["bars"]],
                                timeline=[list(t) for t in s["timeline"]], total_q=s["total_q"], ts=s["ts"],
                                per_bar=[[list(nn) for nn in nb] for nb in s["per_bar"]] if s.get("per_bar") is not None else None,
                                per_bar_rep=s.get("per_bar_rep"), n_xml=s.get("n_xml"), edited=True, added=True))
    others = [s for s in score["systems"] if s["page"] != pg]
    page_sys = [s for s in score["systems"] if s["page"] == pg]
    # drop existing lines that the new one overlaps mostly (the user redrew a line that was there but wrong)
    def overlap(a, b): return max(0.0, min(a["y1"], b["y1"]) - max(a["y0"], b["y0"])) / max(1e-6, min(a["y1"] - a["y0"], b["y1"] - b["y0"]))
    page_sys = [s for s in page_sys if not any(overlap(s, nsy) > 0.6 for nsy in new_systems)]
    merged = sorted(page_sys + new_systems, key=lambda s: s["y0"])
    ren = {}
    for k, s in enumerate(merged):
        if s["idx"] >= 0 and s["idx"] != k:
            for bi in range(len(s["barlines"])): ren[piecemodel.bar_id(pg, s["idx"], bi)] = piecemodel.bar_id(pg, k, bi)
        s["idx"] = k
    score["systems"] = sorted(others + merged, key=lambda s: (s["page"], s["idx"]))
    piecemodel.rebuild_bars(score)
    # rename first (it rewrites structure.json), then save the new score and bump the version
    store.write_json(d / "score.json", score)
    if ren: rename_ids(pid, ren)
    st = store.read_json(d / "structure.json")
    _save(pid, d, score, st)
    (d / "check.json").unlink(missing_ok=True)
    return len(new_systems)

ROUTES = [
    Route("/api/pieces/{pid:int}/line/extent", api_extent, methods=["PUT"]),
    Route("/api/pieces/{pid:int}/line/remove", api_remove, methods=["POST"]),
    Route("/api/pieces/{pid:int}/line/add", api_add, methods=["POST"]),
]
