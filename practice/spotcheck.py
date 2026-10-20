"""Spot checks: instead of tapping downbeats in real time, the app picks the few places where the alignment of a
recording is least certain and matters most, and asks "which click is on the downbeat?" with a handful of
candidates (onsets near the estimate). Every answer becomes a hard anchor; "can't tell" is remembered too.

Uncertainty of a bar start: recordings disagree there (consensus spread), this recording's own estimate differs
from the cross-checked one, the tempo jumps, or several onsets compete near the estimate.
Consequence: the bar starts or ends one of your passages, carries a flag, or starts a run after a pause."""
import io, json, math, wave
import numpy as np
from starlette.responses import JSONResponse, Response
from starlette.routing import Route
from . import store, jobs, piecemodel

def J(obj, status=200): return JSONResponse(obj, status_code=status)
FR = 50

def _onsets(rid):
    f = store.rec_dir(rid) / "feats.npz"
    if not f.exists(): return None
    z = np.load(f); on = z["dlnco"].sum(0)
    from scipy.ndimage import uniform_filter1d
    return uniform_filter1d(on, 3)

def _peaks(on, a, b, k=4):
    """Strongest onset peaks between times a and b: [(t, strength)]."""
    from scipy.signal import find_peaks
    i0, i1 = max(0, int(a * FR)), min(len(on), int(b * FR) + 1)
    if i1 - i0 < 3: return []
    seg = on[i0:i1]; pk, pr = find_peaks(seg, distance=4, prominence=max(1e-6, 0.15 * float(seg.max())))
    order = np.argsort(-pr["prominences"])[:k]
    return [((i0 + pk[j]) / FR, float(pr["prominences"][j])) for j in order]

def candidates(rid, occ, k):
    on = _onsets(rid)
    t = occ[k][2]; d_next = (occ[k][3] - t) if occ[k][3] > t else 1.0
    d_prev = (t - occ[k - 1][2]) if k > 0 and occ[k - 1][2] < t else d_next
    if on is None: return [t]
    pk = _peaks(on, t - 0.45 * d_prev, t + 0.45 * d_next)
    cands = [p for p, _ in pk]
    if not any(abs(c - t) < 0.06 for c in cands): cands.append(t)
    cands = sorted(set(round(c, 3) for c in cands))
    out = []
    for c in cands:                                   # merge near-duplicates (keep the estimate when close)
        if out and c - out[-1] < 0.08: out[-1] = t if abs(c - t) < abs(out[-1] - t) else out[-1]
        else: out.append(c)
    return out

def plan(pid, rid, n=6):
    a = store.read_json(store.rec_dir(rid) / "alignment.json")
    if not a or not a.get("occ"): return []
    cons = (store.read_json(store.piece_dir(pid) / "consensus.json", {}) or {}).get(str(rid))
    occ = [list(o) for o in a["occ"]]
    own = [o[2] for o in occ]
    for k in range(len(occ)):
        if cons and k < len(cons["times"]): occ[k][2] = cons["times"][k]
    for i0, i1, t0, t1 in a["runs"]:
        for k in range(i0, i1): occ[k][3] = occ[k + 1][2] if k + 1 < i1 else t1
    anchors = store.read_json(store.rec_dir(rid) / "anchors.json") or []
    skipped = set(tuple(x) for x in (store.read_json(store.rec_dir(rid) / "spot_skip.json") or []))
    anchored = set()
    for an in anchors:
        for k, o in enumerate(occ):
            if o[0] == an["bar"] and abs(o[2] - an["t"]) < 3.0: anchored.add(k)
    near_anchor = {k + d for k in anchored for d in range(-3, 4)}
    # consequence: passage edges and flags in this piece
    edge = set(); flagged = set()
    for p in store.q("SELECT a,b FROM passages WHERE piece_id=? AND archived=0", (pid,)): edge |= {p["a"], p["b"]}
    for f in store.q("SELECT bar FROM flags WHERE recording_id=? AND done=0", (rid,)): flagged.add(f["bar"])
    run_start = {r[0] for r in a["runs"]}
    on = _onsets(rid)
    scored = []
    for k, o in enumerate(occ):
        if k in near_anchor or (o[0], o[1]) in skipped: continue
        d = max(0.2, o[3] - o[2]); dp = max(0.2, o[2] - occ[k - 1][2]) if k > 0 else d
        sp = cons["spread"][k] if cons and k < len(cons["spread"]) and cons["spread"][k] is not None else 0.0
        u = sp / d + abs(own[k] - o[2]) / d + 0.5 * abs(math.log(d / dp))
        if on is not None:                            # no clear onset right at the estimate
            i = int(o[2] * FR); w = on[max(0, i - 3):i + 4]
            loc = float(w.max()) if len(w) else 0.0; ref = float(np.percentile(on[max(0, i - 200):i + 200], 90)) + 1e-6
            u += 0.3 * max(0.0, 1.0 - loc / ref)
        c = 1.0 + (2.0 if o[0] in edge else 0) + (1.5 if o[0] in flagged else 0) + (0.5 if k in run_start else 0)
        why = []
        if sp / d > 0.12: why.append("the recordings disagree here")
        if abs(own[k] - o[2]) / d > 0.12: why.append("two ways of lining it up disagree")
        if abs(math.log(d / dp)) > 0.35: why.append("the tempo changes suddenly")
        if o[0] in edge: why.append("a passage starts or ends here")
        if o[0] in flagged: why.append("you flagged this bar")
        scored.append((u * c, k, why))
    scored.sort(reverse=True); picks = []
    for s, k, why in scored:
        if len(picks) >= n: break
        if s < 0.12: break                            # nothing uncertain enough to be worth your time
        if any(abs(k - p["k"]) < 4 for p in picks): continue
        picks.append(dict(k=k, bar=occ[k][0], pass_=occ[k][1], t=round(occ[k][2], 3), score=round(s, 3),
                          why=", ".join(why) or "the least certain place left", cands=candidates(rid, occ, k)))
    picks.sort(key=lambda p: p["k"])
    for p in picks: p["pass"] = p.pop("pass_")
    return picks

def jobs_pending(rid):
    return bool(store.q1("SELECT id FROM jobs WHERE recording_id=? AND kind IN ('align','features') AND status IN ('queued','running')", (rid,)))

# ------------------------------------------------------------------ routes
async def api_plan(req):
    rid = int(req.path_params["rid"]); r = store.q1("SELECT piece_id FROM recordings WHERE id=?", (rid,))
    if not r: return J({"error": "no such recording"}, 404)
    n = int(req.query_params.get("n") or 6)
    a = store.read_json(store.rec_dir(rid) / "alignment.json") or {}
    st = store.read_json(store.piece_dir(r["piece_id"]) / "structure.json") or {}
    if a.get("structure_version") != st.get("version") or jobs_pending(rid):
        return J({"error": "This recording is being lined up with the score again. Check its timing when that is done."})
    return J(plan(r["piece_id"], rid, n))

async def api_answer(req):
    """body {bar, pass, t} = the chosen downbeat (anchor), or {bar, pass, skip: true}. Re-aligns only with {done: true}."""
    rid = int(req.path_params["rid"]); body = await req.json()
    r = store.q1("SELECT piece_id FROM recordings WHERE id=?", (rid,))
    if not r: return J({"error": "no such recording"}, 404)
    d = store.rec_dir(rid)
    if body.get("bar"):
        if body.get("skip"):
            sk = store.read_json(d / "spot_skip.json") or []; sk.append([body["bar"], int(body.get("pass") or 1)])
            store.write_json(d / "spot_skip.json", sk)
        else:
            an = store.read_json(d / "anchors.json") or []
            new = dict(bar=body["bar"], t=float(body["t"]), created=store.now(), source="spotcheck"); new["pass"] = int(body.get("pass") or 1)
            an = [x for x in an if not (x["bar"] == new["bar"] and x.get("pass", 1) == new["pass"]) and abs(x["t"] - new["t"]) > 0.3] + [new]
            store.write_json(d / "anchors.json", an)
    if body.get("done"): jobs.enqueue("align", r["piece_id"], rid)
    return J({"ok": True})

async def api_snippet(req):
    """Mono 22.05 kHz WAV of [t0, t1] seconds of a recording (for exact click placement in the browser)."""
    import av
    rid = int(req.path_params["rid"]); r = store.q1("SELECT path FROM recordings WHERE id=?", (rid,))
    if not r or not r["path"]: return Response(status_code=404)
    t0 = max(0.0, float(req.query_params.get("t0", 0))); t1 = float(req.query_params.get("t1", t0 + 3))
    t1 = min(t1, t0 + 20)
    sr = 22050; out = []
    with av.open(r["path"]) as c:
        st = c.streams.audio[0]
        c.seek(int(max(0, t0 - 0.5) / st.time_base), stream=st, any_frame=False, backward=True)
        res = av.AudioResampler(format="s16", layout="mono", rate=sr)
        for fr in c.decode(st):
            ft = float(fr.pts * st.time_base) if fr.pts is not None else None
            for f in res.resample(fr):
                a = f.to_ndarray().reshape(-1)
                if ft is None: continue
                out.append((ft, a))
            if ft is not None and ft > t1: break
    if not out: return Response(status_code=404)
    start = out[0][0]; y = np.concatenate([a for _, a in out]).astype(np.int16)
    i0 = max(0, int((t0 - start) * sr)); i1 = max(i0, int((t1 - start) * sr))
    y = y[i0:i1]
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(sr); w.writeframes(y.tobytes())
    return Response(buf.getvalue(), media_type="audio/wav", headers={"X-Start": str(t0)})

ROUTES = [
    Route("/api/recordings/{rid:int}/spotchecks", api_plan),
    Route("/api/recordings/{rid:int}/spotchecks", api_answer, methods=["POST"]),
    Route("/api/recordings/{rid:int}/snippet", api_snippet),
]
