"""Jump-aware DTW over a whole recording.

States: the frames of every block of every movement (one copy each), plus two "not playing" states:
  G0 - before the first movement / between movements / after the end (entered from a movement's end),
  G1 - stopped in the middle of the music (a restart, a long interruption, the end of a practice take).
Moves: advance 0..4 reference frames per audio frame inside a block; notated jumps between blocks (repeat,
ending, D.C., ...); movement end -> G0 -> next movement; and, at a price, stopping anywhere, resuming
anywhere, or jumping anywhere (a restart). Every path has exactly one cell per audio frame, so any two
paths are directly comparable."""
import numpy as np
INF = 1e18

def run(blocks, edges, mv_starts, mv_finals, A, quiet, silent=None, w=(1.2, 1.0, 1.0, 1.1, 1.25),
        p_start=3.0, p_stop=3.0, p_resume=3.0, p_jump=8.0, p_other_mv=2.0,
        gap_quiet=0.30, gap_loud=0.55, music_on_silence=0.35):
    """blocks: list of dict(id, feat 12xn, bar_frames, bars); edges: [(from_id, to_id, cost)];
    mv_starts: block ids starting each movement (in order); mv_finals: block ids that may end a movement.
    A: audio 12xM (2 Hz CENS); quiet: bool[M] no music (silence/noise); silent: bool[M] no usable pitch content."""
    s = {}; e = {}; o = 0
    for b in blocks: s[b["id"]] = o; o += b["feat"].shape[1]; e[b["id"]] = o
    n = o; G0 = n; G1 = n + 1; M = A.shape[1]
    R = np.concatenate([b["feat"] for b in blocks], axis=1)
    Rn = R / (np.linalg.norm(R, axis=0, keepdims=True) + 1e-9); An = A / (np.linalg.norm(A, axis=0, keepdims=True) + 1e-9)
    C = (1 - Rn.T @ An).astype(np.float32)
    if silent is not None: C[:, silent] = music_on_silence       # rests and decays are part of the music too
    gcost = np.where(quiet, gap_quiet, gap_loud).astype(np.float32)
    bs = np.zeros(n, int)
    for b in blocks: bs[s[b["id"]]:e[b["id"]]] = s[b["id"]]
    barstart = np.zeros(n, bool)
    for b in blocks:
        for f in b["bar_frames"]: barstart[s[b["id"]] + int(f)] = True
    into = {}
    for a, b2, c in edges: into.setdefault(s[b2], []).append((e[a] - 1, c))
    final_ends = [e[f] - 1 for f in mv_finals]
    start_frames = [s[x] for x in mv_starts]
    idx = np.arange(n)
    D = np.full(n + 2, INF)
    D[:n][barstart] = C[barstart, 0] + p_start
    for f in start_frames: D[f] = C[f, 0]
    D[G0] = gcost[0]
    bp = np.full((M, n + 2), -1, np.int32)
    for j in range(1, M):
        prev = D; best = np.full(n + 2, INF); arg = np.full(n + 2, -1, np.int32)
        cj = C[:, j]
        for k in range(5):
            src = idx - k; ok = src >= bs
            cand = np.full(n, INF); cand[ok] = prev[src[ok]] + cj[ok] * w[k]
            m = cand < best[:n]; best[:n][m] = cand[m]; arg[:n][m] = src[m]
        for tgt, srcs in into.items():
            for sr, c in srcs:
                v = prev[sr] + cj[tgt] + c
                if v < best[tgt]: best[tgt] = v; arg[tgt] = sr
        gi = int(np.argmin(prev[:n]))
        # G0: stay, or arrive from a movement end
        c0 = [(prev[G0], G0)] + [(prev[f], f) for f in final_ends]
        v, a = min(c0, key=lambda x: x[0]); best[G0] = v + gcost[j]; arg[G0] = a
        # G1: stay, or stop playing anywhere
        c1 = [(prev[G1], G1), (prev[gi] + p_stop, gi)]
        v, a = min(c1, key=lambda x: x[0]); best[G1] = v + gcost[j]; arg[G1] = a
        # leaving the gaps: movement starts (free from G0), any bar start (resume)
        for f in start_frames:
            for g, c in ((G0, 0.0), (G1, p_other_mv)):
                v = prev[g] + c + cj[f]
                if v < best[f]: best[f] = v; arg[f] = g
        for g in (G0, G1):
            cand = np.where(barstart, prev[g] + p_resume + cj, INF); m = cand < best[:n]; best[:n][m] = cand[m]; arg[:n][m] = g
        # free jump to any bar start (restart / unexpected cut)
        cand = np.where(barstart, prev[gi] + p_jump + cj, INF); m = cand < best[:n]; best[:n][m] = cand[m]; arg[:n][m] = gi
        D = best; bp[j] = arg
    ends = [(D[G0], G0), (D[G1], G1)] + [(D[f], f) for f in final_ends]
    gi = int(np.argmin(D[:n])); ends.append((D[gi] + p_stop, gi))
    cost, i = min(ends, key=lambda x: x[0])
    path = np.zeros(M, np.int64)
    for j in range(M - 1, -1, -1):
        path[j] = i; i = bp[j, i] if j > 0 else i
    fcost = np.array([C[i, j] if i < n else gcost[j] for j, i in enumerate(path)], np.float32)
    return dict(path=path, cost=float(cost / M), start=s, end=e, gaps=(G0, G1), n=n, fcost=fcost)

def visits(res, blocks):
    """Split a path into block visits: [dict(block, j0, j1, i0, i_last)]; gaps have block 'GAP'."""
    s, e, path = res["start"], res["end"], res["path"]; G0, G1 = res["gaps"]
    owner = np.empty(res["n"], object)
    for b in blocks: owner[s[b["id"]]:e[b["id"]]] = b["id"]
    out = []; cur = None
    for j, i in enumerate(path):
        b = "GAP" if i >= G0 else owner[i]
        newvisit = cur is None or b != cur["block"] or (b != "GAP" and (i < cur["i_last"] or i - cur["i_last"] > 4))
        if newvisit:
            if cur: out.append(cur)
            cur = dict(block=b, j0=j, i0=int(i), i_last=int(i), j1=j + 1)
        else:
            cur["i_last"] = int(i); cur["j1"] = j + 1
    if cur: out.append(cur)
    return out
