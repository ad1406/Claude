import os
"""Align one recording to a piece: find its form (path through the structure graph) and every bar's time."""
import numpy as np, warnings
warnings.filterwarnings("ignore")
from scipy.ndimage import median_filter, uniform_filter1d
from . import jump
from .structure import blocks_for_movement, COST
from .reference import render, cens2, QS
from .features import FR

W1 = (1.2, 1.0, 1.0, 1.1, 1.25)          # first pass: tempo unknown, be lenient
W2 = (2.0, 1.0, 1.3, 1.8, 2.5)           # second pass: blocks rendered at this recording's tempo -> prefer the diagonal

def piece_graph(piece):
    out = []
    for mi, mv in enumerate(piece["movements"]):
        bl, ed, st, fi = blocks_for_movement(mv["bars"], mv.get("markup", {}))
        out.append(dict(name=mv["name"], blocks=bl, edges=ed, start=st, finals=fi))
    return out

def _render_blocks(piece, graph, scale):
    blocks = []; edges = []; starts = []; finals = []
    for mi, g in enumerate(graph):
        for bl in g["blocks"]:
            gid = f"{mi}:{bl['id']}"
            bars = [piece["bars"][b] for b in bl["bars"]]
            sc = scale.get(gid, scale.get(mi, 1.0))
            durs = [b["len_q"] * QS * scale.get(("bar", bid), sc) for b, bid in zip(bars, bl["bars"])]
            st, ch, _ = render(bars, durs, with_onsets=False)
            f = cens2(ch)
            bar_frames = np.minimum((st[:-1] * 2).astype(int), f.shape[1] - 1)
            blocks.append(dict(id=gid, feat=f, bar_frames=bar_frames, bars=bl["bars"], mv=mi, scale=sc))
        edges += [(f"{mi}:{a}", f"{mi}:{b}", COST[k]) for a, b, k in g["edges"]]
        starts.append(f"{mi}:{g['start']}"); finals += [f"{mi}:{x}" for x in g["finals"]]
    return blocks, edges, starts, finals

def _quiet(db10, M, chroma=None):
    """Frames with no music: silence (relative to this recording's own noise floor), or sound without pitch
    content (applause, talking, handling noise) - these go to the GAP state instead of being matched as music."""
    n = min(len(db10) // 5, M)
    q = np.zeros(M, bool)
    floor = float(np.percentile(db10, 3)) if len(db10) else -80
    thr = float(np.clip(floor + 8.0, -55.0, -35.0))          # adapt only to a real noise floor
    loud = db10[:n * 5].reshape(n, 5).max(1)
    silent = np.zeros(M, bool)
    if chroma is not None:
        k = min(chroma.shape[1] // 25, M)
        c = chroma[:, :k * 25].reshape(12, k, 25).mean(2) + 1e-9
        p = c / c.sum(0, keepdims=True); ent = -(p * np.log(p)).sum(0) / np.log(12)
        silent[:k] = ent > 0.97                                  # no usable pitch content (silence, decay, noise)
    q[:n] = (loud < thr) | ((loud < -30) & silent[:n])
    q[n:] = True
    return q, silent

SHIFTS = (0, 11, 1, 10, 2)        # tuning differences only (A=415, A=392, sharp old pianos); never a real key change

def transposition(piece, graph, feats):
    """Semitone shift between the recording and the score (e.g. a harpsichord at A=415 sounds a semitone low).
    Only small shifts are considered, and 0 wins unless another is clearly better. A recording much shorter than the
    piece (a practice take of one passage) is matched as a subsequence, not end to end."""
    from synctoolbox.dtw.utils import shift_chroma_vectors
    from synctoolbox.dtw.cost import cosine_distance
    import librosa
    bars = [piece["bars"][b] for g in graph for bl in g["blocks"] for b in bl["bars"]]
    _, ch, _ = render(bars, [b["len_q"] * QS for b in bars], with_onsets=False)
    R = cens2(ch); X = cens2(feats["chroma"])
    short = X.shape[1] < 0.5 * R.shape[1]
    costs = {}
    for k in SHIFTS:
        C = cosine_distance(shift_chroma_vectors(X, k), R)            # recording frames x score frames
        if short:
            D = librosa.sequence.dtw(C=C, subseq=True, backtrack=False)
            costs[k] = float(D[-1].min()) / X.shape[1]
        else:
            D = librosa.sequence.dtw(C=C, backtrack=False)
            costs[k] = float(D[-1, -1]) / (X.shape[1] + R.shape[1])
    best = min(costs, key=costs.get)
    if os.environ.get('SHIFT_DEBUG'): print(short, {k: round(v, 4) for k, v in costs.items()})
    margin = 0.85 if short else 0.97                         # a short clip is noisier: demand a clear win
    return int(best) if costs[best] < margin * costs[0] else 0

def shifted(feats, k):
    if not k: return feats
    from synctoolbox.dtw.utils import shift_chroma_vectors
    f = dict(feats); f["chroma"] = shift_chroma_vectors(feats["chroma"], k); f["dlnco"] = shift_chroma_vectors(feats["dlnco"], k)
    return f

def coarse(piece, feats):
    graph = piece_graph(piece)
    ch = feats["chroma"]; A = cens2(ch); M = A.shape[1]
    quiet, silent = _quiet(feats["db"], M, ch)
    # first pass at a few global tempi (performances range from very broad to very fast)
    best = None
    for g in (0.7, 1.0, 1.5, 2.3):
        blocks, edges, starts, finals = _render_blocks(piece, graph, {mi: g for mi in range(len(graph))})
        r = jump.run(blocks, edges, starts, finals, A, quiet, silent, w=W1)
        if best is None or r["cost"] < best[0]["cost"]: best = (r, blocks)
    r1, blocks = best
    v1 = jump.visits(r1, blocks)
    # refine: re-render every bar at the local tempo this recording actually took, and align again (twice)
    r, bl, v = r1, blocks, v1
    for _ in range(2):
        scale = _scales(piece, graph, bl, r, v)
        bl, edges, starts, finals = _render_blocks(piece, graph, scale)
        r = jump.run(bl, edges, starts, finals, A, quiet, silent, w=W2)
        v = jump.visits(r, bl)
    return graph, bl, r, v

def _scales(piece, graph, blocks, r1, v1):
    """Tempo (seconds per nominal second) per block, per movement, and smoothed per bar, from a pass."""
    B = {b["id"]: b for b in blocks}; num = {}; den = {}
    for v in v1:
        if v["block"] == "GAP": continue
        rf = v["i_last"] - v["i0"] + 1; af = v["j1"] - v["j0"]
        if rf < 4: continue
        num[v["block"]] = num.get(v["block"], 0) + af; den[v["block"]] = den.get(v["block"], 0) + rf
    scale = {}; per_mv = {}
    for gid in num:
        per_mv.setdefault(B[gid]["mv"], []).append(num[gid] / den[gid] * B[gid]["scale"])
    for mi, xs in per_mv.items(): scale[mi] = float(np.median(xs))
    for gid in num:
        mi = B[gid]["mv"]; scale[gid] = float(np.clip(num[gid] / den[gid] * B[gid]["scale"], 0.6 * scale[mi], 1.6 * scale[mi]))
    occ1, runs1 = occurrences(blocks, r1, v1)
    dur = {}
    for r in runs1:
        sl = occ1[r["i0"]:r["i1"]]
        for k, o in enumerate(sl):
            t1 = sl[k + 1][2] if k + 1 < len(sl) else r["t1"]
            if t1 > o[2]: dur.setdefault(o[0], []).append(t1 - o[2])
    for mi, g in enumerate(graph):
        order = [b for bl in g["blocks"] for b in bl["bars"]]
        vals = np.array([np.log(np.median(dur[b]) / max(piece["bars"][b]["len_q"] * QS, 1e-3)) if b in dur else np.nan for b in order])
        if np.all(np.isnan(vals)): continue
        fill = np.where(np.isnan(vals), np.nanmedian(vals), vals)
        sm = median_filter(fill, size=min(9, len(fill) // 2 * 2 + 1 if len(fill) > 2 else 1), mode="nearest")
        for b, v in zip(order, sm): scale[("bar", b)] = float(np.exp(v))
    return scale


def occurrences(blocks, res, vis):
    """Coarse bar start times (s) from the path; runs = continuous playing (no gap)."""
    B = {b["id"]: b for b in blocks}; s = res["start"]; path = res["path"]
    occ = []; runs = []; cur = None; seen = {}
    for v in vis:
        if v["block"] == "GAP":
            if cur is not None: cur["t1"] = v["j0"] / 2.0; runs.append(cur); cur = None
            continue
        b = B[v["block"]]; base = s[b["id"]]
        js = np.arange(v["j0"], v["j1"]); iv = path[v["j0"]:v["j1"]]
        added = False
        for k, bar in enumerate(b["bars"]):
            f = base + b["bar_frames"][k]
            if f < v["i0"] or f > v["i_last"]:
                continue
            hit = js[iv >= f]
            if len(hit) == 0: continue
            seen[bar] = seen.get(bar, 0) + 1
            if cur is None: cur = dict(i0=len(occ), t0=hit[0] / 2.0)
            occ.append([bar, seen[bar], hit[0] / 2.0]); added = True
        if cur is not None: cur["t1"] = v["j1"] / 2.0
    if cur is not None: runs.append(cur)
    fc = res.get("fcost")
    for r in runs:
        a, b = int(r["t0"] * 2), max(int(r["t0"] * 2) + 1, int(r["t1"] * 2))
        r["match"] = float(np.mean(fc[a:b])) if fc is not None else 0.0
    for k, r in enumerate(runs):
        r["i1"] = runs[k + 1]["i0"] if k + 1 < len(runs) else len(occ)
    return occ, runs

def merge_runs(piece, occ, runs, max_gap=12.0):
    """A short pause (fermata, breath, page turn) that continues with the next bar is not a new run."""
    order = {}; mvof = {}
    for mi, mv in enumerate(piece["movements"]):
        for b in mv["bars"]: order[b] = len(order); mvof[b] = mi
    out = []
    for r in runs:
        if out and r["i0"] < len(occ) and out[-1]["i1"] > out[-1]["i0"]:
            p = out[-1]; last = occ[r["i0"] - 1][0]; first = occ[r["i0"]][0]
            if r["t0"] - p["t1"] < max_gap and mvof[first] == mvof[last] and 0 <= order[first] - order[last] <= 1:
                p["i1"] = r["i1"]; p["t1"] = r["t1"]; continue
        out.append(dict(r))
    return out

def _pitched10(chroma, db10, floor):
    """10 Hz: frames with clear pitch content a little above the noise floor (a soft chord decaying, not a click
    or talking)."""
    n = min(chroma.shape[1] // 5, len(db10))
    c = chroma[:, :n * 5].reshape(12, n, 5).mean(2) + 1e-9
    p = c / c.sum(0, keepdims=True); ent = -(p * np.log(p)).sum(0) / np.log(12)
    out = np.zeros(len(db10), bool); out[:n] = (ent < 0.9) & (db10[:n] > floor + 4.0)
    return out

def trim_runs(runs, occ, db10, chroma=None, fcost=None):
    """Leading/trailing silence belongs to no bar: pull a run's edges in to where sustained sound is
    (a click when the recording starts or stops is not music)."""
    if db10 is None or not len(db10): return runs
    floor = float(np.percentile(db10, 3)); thr = float(np.clip(floor + 8.0, -55.0, -35.0))
    snd = (db10 > thr).astype(float)
    tail = np.maximum(snd, _pitched10(chroma, db10, floor)) if chroma is not None else snd   # a soft final chord decays
    sus = np.convolve(snd, np.ones(12) / 12, mode="same") > 0.85       # >= ~1 s of continuous sound
    for r in runs:
        a, z = int(r["t0"] * 10), min(int(r["t1"] * 10), len(sus))
        if z <= a: continue
        on = np.where(sus[a:z])[0]
        if not len(on): continue
        fi, la = a + on[0], a + on[-1]
        # extend over short silences (a staccato opening chord, a final chord's release): gaps < 0.8 s
        k = fi; gap = 0
        while k - 1 >= a and gap < 8:
            k -= 1; gap = 0 if snd[k] else gap + 1
            if snd[k]: fi = k
        k = la; gap = 0
        while k + 1 < z and gap < 8:
            k += 1; gap = 0 if tail[k] else gap + 1
            if tail[k]: la = k
        first, last = fi / 10.0, la / 10.0
        if fcost is not None and last < r["t1"] - 0.6:
            # a very soft ending can sit near the noise floor: keep it if the pitch content still matches the score well
            fa, fz = int(r["t0"] * 2), int(r["t1"] * 2); ta = int(last * 2)
            if fz - ta >= 2 and fz - fa > 10:
                body = float(np.median(fcost[fa:ta])); rest = float(np.mean(fcost[ta:fz]))
                if rest < body + 0.08: last = r["t1"] - 0.4
        if last < r["t1"] - 0.6:
            r["t1"] = last + 0.4
            while r["i1"] - 1 > r["i0"] and occ[r["i1"] - 1][2] >= r["t1"] - 0.25: r["i1"] -= 1
        if first > r["t0"] + 0.6:
            r["t0"] = first - 0.25
            for k in range(r["i0"], r["i1"]):
                if occ[k][2] < r["t0"]: occ[k][2] = r["t0"]
    keep = []; new_runs = []
    for r in runs:
        i0 = len(keep); keep += occ[r["i0"]:r["i1"]]; nr = dict(r); nr["i0"] = i0; nr["i1"] = len(keep); new_runs.append(nr)
    occ[:] = keep
    return new_runs

def _odd(x, lo, hi): x = int(round(min(hi, max(lo, x)))); return x if x % 2 else x + 1

def fine(piece, feats, occ, runs, iters=None):
    """Refine bar times run by run: render at a smoothed local tempo, then MrMsDTW (chroma + onsets).
    The tempo is smoothed over a window of about 4 s of music, then 2 s (in bars: fewer for slow bars), so that
    2 Hz noise in short bars is averaged out but a sudden slow-down or speed-up in long bars is kept."""
    from synctoolbox.dtw.mrmsdtw import sync_via_mrmsdtw
    from synctoolbox.dtw.utils import make_path_strictly_monotonic
    ch, dl = feats["chroma"], feats["dlnco"]; T = ch.shape[1] / FR
    out = [list(o) + [None] for o in occ]
    for r in runs:
        sl = occ[r["i0"]:r["i1"]]
        if len(sl) < 2: 
            for k in range(r["i0"], r["i1"]): out[k][3] = r["t1"]
            continue
        bars = [piece["bars"][o[0]] for o in sl]
        t = np.array([o[2] for o in sl] + [r["t1"]])
        # the recording stops in the middle of the last bar (a practice take): render only the part that was played,
        # or the whole bar gets squeezed into what is left and pulls the bars before it late
        if r["t1"] >= T - 0.8 and len(sl) >= 4 and not r.get("pin1"):
            spq = float(np.median(np.diff(t[-5:-1]) / np.array([max(b["len_q"], 0.25) for b in bars[-4:-1]])))   # seconds per quarter
            lb = bars[-1]; played = r["t1"] - t[-2]; expect = spq * max(lb["len_q"], 0.25)
            if played < 0.8 * expect:
                f = max(0.15, played / expect); cq = max(lb.get("content_q") or lb["len_q"], 1e-3)
                lb = dict(lb, len_q=lb["len_q"] * f, content_q=cq * f,
                          notes=[(o, min(d, cq * f - o), p) for o, d, p in lb["notes"] if o < cq * f])
                bars = bars[:-1] + [lb]
        nom = np.array([max(b["len_q"], 0.25) for b in bars])
        # an anchored edge (a downbeat the user tapped) is exact: no slack around it
        its = iters
        if its is None:
            md = float(np.median(np.maximum(np.diff(t), 0.05)))
            mode = os.environ.get("FINE_MODE", "mid")
            if mode == "old": its = ((31, 10), (9, 3))
            elif mode == "new": its = ((_odd(4.0 / md, 1, 31), max(1, int(round(1.3 / md)))), (_odd(2.0 / md, 1, 9), 1))
            else: its = ((_odd(10.0 / md, 3, 31), max(1, int(round(3.0 / md)))), (_odd(5.0 / md, 1, 9), 1))
        a0 = max(0, int((r["t0"] - (0.0 if r.get("pin0") else 0.1)) * FR))
        a1 = min(ch.shape[1], int((r["t1"] + (0.0 if r.get("pin1") else 1.0)) * FR))
        for size, sm in its:
            spb = np.maximum(np.diff(t), 1e-3) / nom
            size = min(size, max(1, len(spb) // 2 * 2 - 1))
            spb = np.exp(uniform_filter1d(median_filter(np.log(spb), size=size, mode="nearest"), size=max(1, min(sm, len(spb))), mode="nearest"))
            st, rch, rdl = render(bars, spb * nom)
            if a1 - a0 < 20: break
            try:
                p = make_path_strictly_monotonic(sync_via_mrmsdtw(rch, ch[:, a0:a1], rdl, dl[:, a0:a1], input_feature_rate=FR)).astype(float)
            except Exception:
                break
            t = np.interp(st, p[0] / FR, p[1] / FR) + a0 / FR
        for k, o in enumerate(sl):
            out[r["i0"] + k][2] = float(t[k]); out[r["i0"] + k][3] = float(t[k + 1])
    return out

def apply_anchors(occ, runs, anchors):
    """Hard anchors [(bar, pass, t)]: that occurrence starts exactly at t. Runs are split there, so the fine
    alignment on each side is constrained to end/start at the anchor. Returns (runs, anchored occurrence indices)."""
    runs = [dict(r) for r in runs]; pinned = []
    for bar, ps, t in sorted(anchors or [], key=lambda a: a[2]):
        cands = [k for k, o in enumerate(occ) if o[0] == bar]
        if not cands: continue
        k = min(cands, key=lambda k: (abs(occ[k][2] - t) > 20, occ[k][1] != ps, abs(occ[k][2] - t)))
        ri = next((i for i, r in enumerate(runs) if r["i0"] <= k < r["i1"]), None)
        if ri is None: continue
        r = runs[ri]; occ[k][2] = t; pinned.append(k)
        if k == r["i0"]: r["t0"] = t; r["pin0"] = True; continue
        new = dict(r, i0=k, t0=t, pin0=True); r["i1"] = k; r["t1"] = t; r["pin1"] = True
        runs.insert(ri + 1, new)
    return runs, pinned

def align(piece, feats, anchors=None):
    k = transposition(piece, piece_graph(piece), feats)
    feats = shifted(feats, k)
    graph, blocks, res, vis = coarse(piece, feats)
    occ, runs = occurrences(blocks, res, vis)
    runs = merge_runs(piece, occ, runs)
    runs = trim_runs(runs, occ, feats.get("db"), feats.get("chroma"), res.get("fcost"))
    runs, pinned = apply_anchors(occ, runs, anchors)
    occf = fine(piece, feats, occ, runs)
    for k in pinned: occf[k][2] = occ[k][2]
    for i, r in enumerate(runs):                     # anchored neighbours: close the gap exactly
        if r.get("pin1") and r["i1"] > r["i0"]: occf[r["i1"] - 1][3] = r["t1"]
    # form summary per movement: visited blocks in order
    forms = {}
    for v in vis:
        if v["block"] == "GAP": continue
        mi, bid = v["block"].split(":")
        forms.setdefault(int(mi), []).append(bid)
    gaps = [[v["j0"] / 2.0, v["j1"] / 2.0] for v in vis if v["block"] == "GAP"]
    return dict(occ=[[o[0], o[1], round(o[2], 3), round(o[3], 3)] for o in occf],
                runs=[[r["i0"], r["i1"], round(r["t0"], 2), round(r["t1"], 2)] for r in runs],
                forms={graph[int(m)]["name"]: v for m, v in forms.items()}, gaps=gaps, cost=res["cost"], shift=k,
                anchored=sorted(pinned))
