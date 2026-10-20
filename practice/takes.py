"""Your own takes (recorded in the app) and flags dropped while listening.

A take arrives as a browser recording (webm/opus, which seeks badly); it is converted to FLAC before analysis,
then aligned like any other recording, so it is bar-indexed. Flags are just (recording, time, bar, note).
"""
from pathlib import Path
from starlette.responses import JSONResponse
from starlette.routing import Route
from . import store, jobs

def J(obj, status=200): return JSONResponse(obj, status_code=status)

def to_flac(src):
    """Decode any audio file and write <stem>.flac next to it (mono, original rate). Returns the new path."""
    import av
    src = Path(src); dst = src.with_suffix(".flac")
    with av.open(str(src)) as inp:
        ast = inp.streams.audio[0]
        rate = ast.codec_context.sample_rate or 48000
        with av.open(str(dst), "w", format="flac") as out:
            ost = out.add_stream("flac", rate=rate); ost.layout = "mono"
            res = av.AudioResampler(format="s16", layout="mono", rate=rate)
            for frame in inp.decode(ast):
                frame.pts = None
                for f in res.resample(frame):
                    for pkt in ost.encode(f): out.mux(pkt)
            for f in res.resample(None):
                for pkt in ost.encode(f): out.mux(pkt)
            for pkt in ost.encode(None): out.mux(pkt)
    return dst

# ------------------------------------------------------------------ dynamics per bar
_DB = {}
def _db(rid):
    """The recording's loudness curve (dB re its max, 10 per second), cached on the feature file's mtime."""
    import numpy as np
    f = store.rec_dir(rid) / "feats.npz"
    if not f.exists(): return None
    key = (rid, f.stat().st_mtime)
    if key not in _DB:
        for k in [k for k in _DB if k[0] == rid]: del _DB[k]
        _DB[key] = np.load(f)["db"]
    return _DB[key]

async def api_dynamics(req):
    """?bars=id,id,...: for every aligned recording, the loudness of each bar (first time through), in dB: the 80th
    percentile inside the bar, so it follows the notes' attacks rather than their decay. Only the shape is meaningful:
    pianos, rooms and microphones differ, so the page compares each recording with itself."""
    import numpy as np
    pid = int(req.path_params["pid"]); ids = [x for x in (req.query_params.get("bars") or "").split(",") if x]
    out = {}
    for r in store.q("SELECT id, range_start FROM recordings WHERE piece_id=? AND status='ready'", (pid,)):
        a = store.read_json(store.rec_dir(r["id"]) / "alignment.json"); db = _db(r["id"])
        if not a or db is None: continue
        first = {}
        for o in a["occ"]:
            if o[0] not in first: first[o[0]] = (o[2], o[3])
        vals = []
        for b in ids:
            if b not in first: vals.append(None); continue
            t0, t1 = first[b]; i0, i1 = int(t0 * 10), max(int(t0 * 10) + 1, int(t1 * 10))
            seg = db[i0:i1]
            vals.append(round(float(np.percentile(seg, 80)), 2) if len(seg) else None)
        if sum(v is not None for v in vals) >= 2: out[str(r["id"])] = vals
    return J(out)

# ------------------------------------------------------------------ flags
async def api_flags(req):
    pid = int(req.path_params["pid"])
    return J(store.q("SELECT * FROM flags WHERE piece_id=? ORDER BY recording_id, t", (pid,)))

async def api_add_flag(req):
    rid = int(req.path_params["rid"]); body = await req.json()
    r = store.q1("SELECT piece_id FROM recordings WHERE id=?", (rid,))
    if not r: return J({"error": "no such recording"}, 404)
    i = store.ex("INSERT INTO flags(piece_id,recording_id,t,bar,pass,note,created,kind,frac) VALUES(?,?,?,?,?,?,?,?,?)",
                 (r["piece_id"], rid, float(body.get("t") or 0), body.get("bar"), body.get("pass"), body.get("note") or "", store.now(),
                  body.get("kind") or None, body.get("frac")))
    return J(store.q1("SELECT * FROM flags WHERE id=?", (i,)))

async def api_update_flag(req):
    i = int(req.path_params["id"]); body = await req.json()
    for k in ("note", "bar", "t", "done", "kind"):
        if k in body: store.ex(f"UPDATE flags SET {k}=? WHERE id=?", (body[k], i))
    return J(store.q1("SELECT * FROM flags WHERE id=?", (i,)) or {})

async def api_delete_flag(req):
    store.ex("DELETE FROM flags WHERE id=?", (int(req.path_params["id"]),)); return J({"ok": True})

# ------------------------------------------------------------------ anchors (tapped downbeats that fix the alignment)
async def api_add_anchor(req):
    rid = int(req.path_params["rid"]); body = await req.json()
    r = store.q1("SELECT piece_id FROM recordings WHERE id=?", (rid,))
    if not r or not body.get("bar"): return J({"error": "no such recording / bar"}, 400)
    f = store.rec_dir(rid) / "anchors.json"; an = store.read_json(f) or []
    new = dict(bar=body["bar"], t=float(body["t"]), created=store.now()); new["pass"] = int(body.get("pass") or 1)
    an = [a for a in an if not (a["bar"] == new["bar"] and a.get("pass", 1) == new["pass"]) and abs(a["t"] - new["t"]) > 0.3] + [new]
    store.write_json(f, an); jobs.enqueue("align", r["piece_id"], rid)
    return J({"anchors": len(an)})

async def api_clear_anchors(req):
    rid = int(req.path_params["rid"]); r = store.q1("SELECT piece_id FROM recordings WHERE id=?", (rid,))
    (store.rec_dir(rid) / "anchors.json").unlink(missing_ok=True)
    if r: jobs.enqueue("align", r["piece_id"], rid)
    return J({"ok": True})

# ------------------------------------------------------------------ performance cues (annotations on bars)
CUE_KINDS = ("basic", "interpretive", "expressive", "structural")

async def api_cues(req):
    pid = int(req.path_params["pid"])
    if req.method == "POST":
        body = await req.json()
        if not body.get("bar"): return J({"error": "no bar"}, 400)
        kind = body.get("kind") if body.get("kind") in CUE_KINDS else "expressive"
        i = store.ex("INSERT INTO cues(piece_id,bar,kind,text,created) VALUES(?,?,?,?,?)", (pid, body["bar"], kind, body.get("text") or "", store.now()))
        return J(store.q1("SELECT * FROM cues WHERE id=?", (i,)))
    return J(store.q("SELECT * FROM cues WHERE piece_id=? ORDER BY id", (pid,)))

async def api_cue(req):
    i = int(req.path_params["id"])
    if req.method == "DELETE": store.ex("DELETE FROM cues WHERE id=?", (i,)); return J({"ok": True})
    body = await req.json()
    for k in ("kind", "text", "bar"):
        if k in body: store.ex(f"UPDATE cues SET {k}=? WHERE id=?", (body[k], i))
    return J(store.q1("SELECT * FROM cues WHERE id=?", (i,)) or {})

ROUTES = [
    Route("/api/pieces/{pid:int}/dynamics", api_dynamics),
    Route("/api/pieces/{pid:int}/cues", api_cues, methods=["GET", "POST"]),
    Route("/api/cues/{id:int}", api_cue, methods=["PATCH", "DELETE"]),
    Route("/api/recordings/{rid:int}/anchors", api_add_anchor, methods=["POST"]),
    Route("/api/recordings/{rid:int}/anchors", api_clear_anchors, methods=["DELETE"]),
    Route("/api/pieces/{pid:int}/flags", api_flags),
    Route("/api/recordings/{rid:int}/flags", api_add_flag, methods=["POST"]),
    Route("/api/flags/{id:int}", api_update_flag, methods=["PATCH"]),
    Route("/api/flags/{id:int}", api_delete_flag, methods=["DELETE"]),
]
