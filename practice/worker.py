"""Background worker: runs queued jobs one at a time in its own process (so the UI stays responsive)."""
import warnings; warnings.filterwarnings("ignore")
import logging, os, shutil, sys, time, traceback, json
from pathlib import Path
from . import store, jobs, piecemodel, LOG_DIR, DATA_DIR

log = logging.getLogger("worker")

# ---------------------------------------------------------------- score
def do_score(j):
    from PIL import Image
    from .engine import score as S
    pid = j["piece_id"]; p = store.q1("SELECT * FROM pieces WHERE id=?", (pid,)); d = store.piece_dir(pid)
    store.ex("UPDATE pieces SET status='processing', message='Reading the score' WHERE id=?", (pid,))
    pages = S.render_pages(p["pdf"], d / "pages", first=p.get("page_from"), last=p.get("page_to"),
                           progress=lambda f, m: jobs.progress(j["id"], 0.05 * f, m))
    store.ex("UPDATE pieces SET npages=? WHERE id=?", (len(pages), pid))
    omr = d / "omr"; omr.mkdir(exist_ok=True); ps = []; sizes = []
    for i, png in enumerate(pages):
        jobs.progress(j["id"], 0.05 + 0.9 * i / len(pages), f"reading page {i + 1} of {len(pages)} (OMR)")
        o = omr / png.name
        if not o.exists(): shutil.copy(png, o)
        S.run_homr(o, o.with_suffix(".log"))
        ps.append((i + 1, S.analyse_page(o, o.with_suffix(".musicxml"), o.with_suffix(".txt"), o.with_suffix(".log"))))
        with Image.open(png) as im: sizes.append((i + 1, im.size))
    score = piecemodel.build_score(ps, sizes)
    from .engine.barlines import VERSION; score["detector"] = VERSION
    flat = S.build_bars(ps); mv_starts = S.propose_movements(flat, ps)
    store.write_json(d / "score.json", score)
    if not (d / "structure.json").exists():
        store.write_json(d / "structure.json", piecemodel.default_structure(score, mv_starts))
    store.ex("UPDATE pieces SET status='ready', message=NULL, updated=? WHERE id=?", (store.now(), pid))
    for r in store.q("SELECT id FROM recordings WHERE piece_id=?", (pid,)): jobs.enqueue("align", pid, r["id"])

def do_redetect(j):
    """Find the barlines again with a newer detector, keeping lines the user fixed by hand, and carry everything
    that refers to bars (structure, passages, flags, anchors) over to the new bars."""
    from PIL import Image
    from .engine import score as S
    from .engine.barlines import VERSION
    pid = j["piece_id"]; d = store.piece_dir(pid); omr = d / "omr"
    old = store.read_json(d / "score.json"); st = store.read_json(d / "structure.json")
    if not old or not st: return
    pages = sorted({s_["page"] for s_ in old["systems"]}); ps = []; sizes = []
    for i, pg in enumerate(pages):
        jobs.progress(j["id"], i / len(pages), f"finding barlines, page {pg}")
        o = omr / f"p{pg:03d}.png"
        if not (o.exists() and o.with_suffix(".log").exists()): return          # no OMR output: leave it
        ps.append((pg, S.analyse_page(o, o.with_suffix(".musicxml"), o.with_suffix(".txt"), o.with_suffix(".log"))))
        with Image.open(o) as im: sizes.append((pg, im.size))
    new = piecemodel.build_score(ps, sizes); new["detector"] = VERSION
    edited = {(s_["page"], s_["idx"]): s_ for s_ in old["systems"] if s_.get("edited")}
    new["systems"] = [edited.get((s_["page"], s_["idx"]), s_) for s_ in new["systems"]]
    piecemodel.rebuild_bars(new)
    find = piecemodel.bar_mapper(old, new)
    nst = piecemodel.remap_structure(old, new, st)
    for p_ in store.q("SELECT id,a,b FROM passages WHERE piece_id=?", (pid,)):
        store.ex("UPDATE passages SET a=?, b=? WHERE id=?", (find(p_["a"], "start") or p_["a"], find(p_["b"], "end") or p_["b"], p_["id"]))
    for f in store.q("SELECT id,bar FROM flags WHERE piece_id=? AND bar IS NOT NULL", (pid,)):
        store.ex("UPDATE flags SET bar=? WHERE id=?", (find(f["bar"]) or f["bar"], f["id"]))
    for x in store.q("SELECT id,sec_a,sec_b FROM recordings WHERE piece_id=? AND sec_a IS NOT NULL", (pid,)):
        store.ex("UPDATE recordings SET sec_a=?, sec_b=? WHERE id=?", (find(x["sec_a"], "start") or x["sec_a"], find(x["sec_b"], "end") or x["sec_b"], x["id"]))
    for c in store.q("SELECT id,bar FROM cues WHERE piece_id=?", (pid,)):
        store.ex("UPDATE cues SET bar=? WHERE id=?", (find(c["bar"], "start") or c["bar"], c["id"]))
    for r in store.q("SELECT id FROM recordings WHERE piece_id=?", (pid,)):
        af = store.rec_dir(r["id"]) / "anchors.json"; an = store.read_json(af)
        if an: store.write_json(af, [dict(a, bar=find(a["bar"], "start") or a["bar"]) for a in an])
    store.write_json(d / "score.old.json", old)
    store.write_json(d / "score.json", new); store.write_json(d / "structure.json", nst)
    (d / "check.json").unlink(missing_ok=True)
    for r in store.q("SELECT id FROM recordings WHERE piece_id=?", (pid,)): jobs.enqueue("align", pid, r["id"])

def queue_redetect():
    """At start: pieces whose bars were found by an older detector get the new one (once)."""
    from .engine.barlines import VERSION
    for p in store.q("SELECT id FROM pieces WHERE status='ready'"):
        sc = store.read_json(store.piece_dir(p["id"]) / "score.json")
        if sc and sc.get("detector", 1) < VERSION: jobs.enqueue("redetect", p["id"])

# ---------------------------------------------------------------- recordings
def do_download(j):
    import yt_dlp
    from .doctor import deno_path
    rid = j["recording_id"]; r = store.q1("SELECT * FROM recordings WHERE id=?", (rid,)); d = store.rec_dir(rid)
    store.ex("UPDATE recordings SET status='downloading' WHERE id=?", (rid,))
    def hook(h):
        if h.get("status") == "downloading" and h.get("total_bytes"):
            jobs.progress(j["id"], h.get("downloaded_bytes", 0) / h["total_bytes"], "downloading")
    opts = {"format": "bestaudio", "outtmpl": str(d / "audio.%(ext)s"), "noplaylist": True, "quiet": True, "progress_hooks": [hook]}
    dp = deno_path()
    if dp: opts["js_runtimes"] = {"deno": {"path": dp}}
    with yt_dlp.YoutubeDL(opts) as y:
        info = y.extract_info(r["url"], download=True)
    f = next(d.glob("audio.*"))
    name = r["name"] or info.get("title")
    store.ex("UPDATE recordings SET path=?, name=?, duration=?, status='processing' WHERE id=?", (str(f), name, info.get("duration"), rid))
    chapters = info.get("chapters") or []
    if chapters: store.write_json(d / "chapters.json", chapters)
    jobs.enqueue("features", r["piece_id"], rid)

def do_features(j):
    from .engine import features as F
    rid = j["recording_id"]; r = store.q1("SELECT * FROM recordings WHERE id=?", (rid,))
    store.ex("UPDATE recordings SET status='processing', message='analysing audio' WHERE id=?", (rid,))
    if r["source"] == "take" and not r["path"].endswith(".flac"):       # browser recordings seek badly: make a FLAC
        from .takes import to_flac
        jobs.progress(j["id"], 0.02, "converting the take")
        # the browser recording is kept: it may be playing right now (listening back straight after recording)
        new = to_flac(r["path"])
        store.ex("UPDATE recordings SET path=? WHERE id=?", (str(new), rid)); r = dict(r, path=str(new))
    f = F.compute(r["path"], str(store.rec_dir(rid) / "feats.npz"), progress=lambda x, m="": jobs.progress(j["id"], x, m))
    store.ex("UPDATE recordings SET duration=?, status='waiting', message=NULL WHERE id=?", (float(f["duration"]), rid))
    jobs.enqueue("align", r["piece_id"], rid)

def _feats(rid, r):
    import numpy as np
    from .engine import features as F
    f = F.compute(r["path"], str(store.rec_dir(rid) / "feats.npz"))
    off = 0.0
    a, b = r.get("range_start"), r.get("range_end")
    if a or b:
        a = a or 0.0; b = b or float(f["duration"]); off = a
        f = dict(f); f["chroma"] = f["chroma"][:, int(a * F.FR):int(b * F.FR)]; f["dlnco"] = f["dlnco"][:, int(a * F.FR):int(b * F.FR)]
        f["db"] = f["db"][int(a * 10):int(b * 10)]
    return f, off

def do_align(j):
    from .engine.align import align
    pid, rid = j["piece_id"], j["recording_id"]
    p = store.q1("SELECT * FROM pieces WHERE id=?", (pid,)); r = store.q1("SELECT * FROM recordings WHERE id=?", (rid,))
    if not p or not r: return
    if p["status"] != "ready": return                      # re-queued after the score is read
    if not r["path"] or not (store.rec_dir(rid) / "feats.npz").exists():
        jobs.enqueue("features", pid, rid); return
    score, structure = piecemodel.load(pid)
    store.ex("UPDATE recordings SET status='aligning', message=NULL WHERE id=?", (rid,))
    jobs.progress(j["id"], 0.1, "finding the form and every bar")
    f, off = _feats(rid, r)
    anchors = [(a["bar"], a.get("pass") or 1, a["t"] - off) for a in (store.read_json(store.rec_dir(rid) / "anchors.json") or [])
               if a["t"] - off >= 0]
    span = None
    if r.get("sec_a"):                                     # a take of one section: align against those bars only
        span = (r["sec_a"], r.get("sec_b") or r["sec_a"])
    elif r.get("passage_id"):
        ps = store.q1("SELECT a,b FROM passages WHERE id=?", (r["passage_id"],))
        if ps: span = (ps["a"], ps["b"])
    res = align(piecemodel.engine_model(score, structure, span), f, anchors)
    if off:
        for o in res["occ"]: o[2] += off; o[3] += off
        for rr in res["runs"]: rr[2] += off; rr[3] += off
    res["structure_version"] = structure.get("version", 1); res["created"] = store.now()
    store.write_json(store.rec_dir(rid) / "alignment.json", res)
    store.ex("UPDATE recordings SET status='ready' WHERE id=?", (rid,))
    jobs.enqueue("consensus", pid)

def do_consensus(j):
    from .engine import consensus
    from .engine.align import shifted
    pid = j["piece_id"]; score, structure = piecemodel.load(pid)
    recs = {}
    # section and passage takes are short and yours: they don't vote on where the bars are
    for r in store.q("SELECT * FROM recordings WHERE piece_id=? AND status='ready' AND sec_a IS NULL AND passage_id IS NULL", (pid,)):
        a = store.read_json(store.rec_dir(r["id"]) / "alignment.json")
        if not a or not a.get("occ"): continue
        f, off = _feats(r["id"], r); f = shifted(f, a.get("shift", 0))
        # times in the alignment are absolute; features are cropped by `off`
        occ = [[o[0], o[1], o[2] - off, o[3] - off] for o in a["occ"]]; runs = [[x[0], x[1], x[2] - off, x[3] - off] for x in a["runs"]]
        recs[r["id"]] = dict(occ=occ, runs=runs, chroma=f["chroma"], dlnco=f["dlnco"], off=off)
    if len(recs) < 2:
        store.write_json(store.piece_dir(pid) / "consensus.json", {}); return
    mv, lists = piecemodel.movement_of(score, structure)
    order = {b: i for i, b in enumerate([b for l in lists for b in l])}
    out = consensus.run(recs, order, progress=lambda x, m="": jobs.progress(j["id"], x, m))
    store.write_json(store.piece_dir(pid) / "consensus.json",
                     {str(k): dict(times=[t + recs[k]["off"] for t in v[0]], spread=v[1], n=v[2]) for k, v in out.items()})

def do_addline(j):
    from . import edits
    edits.do_addline(j, progress=lambda x, m="": jobs.progress(j["id"], x, m))

def do_check(j):
    from . import vision
    vision.run_check(j["piece_id"], progress=lambda x, m="": jobs.progress(j["id"], x, m))

HANDLERS = dict(addline=do_addline, redetect=do_redetect, check=do_check, score=do_score, download=do_download, features=do_features, align=do_align, consensus=do_consensus)

def main():
    LOG_DIR.mkdir(exist_ok=True)
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s worker: %(message)s",
                        handlers=[logging.FileHandler(LOG_DIR / "worker.log", encoding="utf-8")])
    jobs.reset_stale()
    try: queue_redetect()
    except Exception as e: log.exception("redetect check failed: %s", e)
    parent = os.getppid()
    while True:
        j = jobs.claim()
        if not j:
            time.sleep(0.5); continue
        log.info("job %s %s piece=%s rec=%s", j["id"], j["kind"], j["piece_id"], j["recording_id"])
        try:
            HANDLERS[j["kind"]](j); jobs.done(j["id"])
        except Exception as e:
            log.error(traceback.format_exc()); jobs.fail(j["id"], f"{type(e).__name__}: {e}")
            if j.get("recording_id"): store.ex("UPDATE recordings SET status='failed', message=? WHERE id=?", (f"{type(e).__name__}: {e}"[:300], j["recording_id"]))
            elif j.get("piece_id") and j["kind"] == "score": store.ex("UPDATE pieces SET status='failed', message=? WHERE id=?", (f"{type(e).__name__}: {e}"[:300], j["piece_id"]))

if __name__ == "__main__":
    main()
