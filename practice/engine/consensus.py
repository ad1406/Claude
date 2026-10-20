"""Cross-check every recording of a piece against every other one.

For each recording X and each other recording Y, stretches of continuous playing are aligned audio-to-audio
(MrMsDTW), and Y's bar times are carried over into X. The median of X's own estimate and all carried-over
estimates is kept; their spread is the confidence. One bad pairing gets outvoted."""
import numpy as np, itertools, warnings
warnings.filterwarnings("ignore")
from .features import FR

def _chunks(occ, runs):
    """contiguous stretches inside runs where bars follow the notated order (a jump starts a new chunk)"""
    out = []
    for i0, i1, t0, t1 in runs:
        cur = [i0]
        for k in range(i0 + 1, i1):
            cur.append(k)
        # split on backward/forward jumps in bar order handled by caller via bar index map
        out.append(cur)
    return out

def run(recs, bar_order, progress=lambda f, m="": None):
    """recs: {name: dict(occ=[[bar, pass, t0, t1]], runs=[[i0,i1,t0,t1]], chroma, dlnco)};
    bar_order: {bar_id: index in notated order}. Returns {name: (times[], spread[], n_est[])} per occurrence."""
    from synctoolbox.dtw.mrmsdtw import sync_via_mrmsdtw
    from synctoolbox.dtw.utils import make_path_strictly_monotonic
    names = list(recs)
    est = {n: [[o[2]] for o in recs[n]["occ"]] for n in names}
    pairs = list(itertools.permutations(names, 2))
    for pi, (X, Y) in enumerate(pairs):
        progress(pi / max(1, len(pairs)), f"cross-checking {X} with {Y}")
        ox, oy = recs[X]["occ"], recs[Y]["occ"]
        idxY = {(o[0], o[1]): k for k, o in enumerate(oy)}; idxY1 = {o[0]: k for k, o in reversed(list(enumerate(oy))) if o[1] == 1}
        for i0, i1, _, _ in recs[X]["runs"]:
            # split X's run where the notated order jumps (repeat, D.C., cut)
            pieces = [[i0]]
            for k in range(i0 + 1, i1):
                if bar_order[ox[k][0]] == bar_order[ox[k - 1][0]] + 1: pieces[-1].append(k)
                else: pieces.append([k])
            for pc in pieces:
                chain = []
                for k in pc:
                    ky = idxY.get((ox[k][0], ox[k][1]), idxY1.get(ox[k][0]))
                    if ky is None: continue
                    if chain and not (oy[ky][2] > oy[chain[-1][1]][2] and oy[ky][2] - oy[chain[-1][1]][3] < 2.0): 
                        if len(chain) >= 6: _pair(recs, X, Y, chain, est, sync_via_mrmsdtw, make_path_strictly_monotonic)
                        chain = []
                    chain.append((k, ky))
                if len(chain) >= 6: _pair(recs, X, Y, chain, est, sync_via_mrmsdtw, make_path_strictly_monotonic)
    out = {}
    for n in names:
        e = est[n]
        med = [float(np.median(v)) for v in e]
        spr = [float(np.percentile(v, 75) - np.percentile(v, 25)) if len(v) > 2 else None for v in e]
        out[n] = (med, spr, [len(v) for v in e])
    return out

def _pair(recs, X, Y, chain, est, sync, mono):
    ox, oy = recs[X]["occ"], recs[Y]["occ"]
    x0 = max(0, ox[chain[0][0]][2] - 1); x1 = ox[chain[-1][0]][3] + 1
    y0 = max(0, oy[chain[0][1]][2] - 1); y1 = oy[chain[-1][1]][3] + 1
    cx = recs[X]["chroma"][:, int(x0 * FR):int(x1 * FR)]; dx = recs[X]["dlnco"][:, int(x0 * FR):int(x1 * FR)]
    cy = recs[Y]["chroma"][:, int(y0 * FR):int(y1 * FR)]; dy = recs[Y]["dlnco"][:, int(y0 * FR):int(y1 * FR)]
    if cx.shape[1] < 100 or cy.shape[1] < 100: return
    try: p = mono(sync(cy, cx, dy, dx, input_feature_rate=FR)).astype(float)
    except Exception: return
    for k, ky in chain[1:-1]:
        est[X][k].append(float(np.interp(oy[ky][2] - y0, p[0] / FR, p[1] / FR) + x0))
