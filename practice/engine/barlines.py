"""Measure boxes on the page: homr gives each system's box and how many barlines it read there;
this finds the system's staves and where its barlines are (vertical strokes through the staves).

homr's boxes are only a starting point: they are sometimes narrower than the staff (so the first bar would start
after its first notes and the last bar end before the barline), or cover one staff of a grand staff. So the staves
are looked for in a band around the box (never reaching into the neighbouring systems), and the horizontal extent
is taken from the staff lines themselves."""
import numpy as np

VERSION = 2

def _rows(B, thr):
    prof = B.mean(1); rows = np.where(prof > thr)[0]; groups = []
    for r in rows:
        if groups and r - groups[-1][-1] <= 2: groups[-1].append(r)
        else: groups.append([r])
    return [float(np.mean(g)) for g in groups]

def staves(B):
    """Find 5-line staves (equally spaced line groups) inside a crop. Rows are judged on the part of the crop where
    lines are drawn, so short (indented) staves still count."""
    for thr in (0.5, 0.4, 0.3, 0.2):
        ys = _rows(B, thr); found = []; i = 0
        while i + 4 < len(ys):
            g = np.diff(ys[i:i + 5])
            if g.min() > 3 and g.max() < 1.3 * g.min(): found.append((ys[i], ys[i + 4])); i += 5
            else: i += 1
        if len(found) >= 2 or (len(found) == 1 and thr == 0.2): return found
    return found

def deskew(B, max_deg=1.5, step=0.1, rows=None, cols=None):
    """Scans are often a little rotated (and differently in different parts of the page): straighten one system.
    The angle is measured on rows/cols (the system itself) and applied to all of B."""
    from scipy.ndimage import rotate
    R = B[rows[0]:rows[1]] if rows else B
    if cols: R = R[:, cols[0]:cols[1]]
    small = R[::2, ::2].astype(np.float32)          # halving both axes keeps angles unchanged
    best, ba = -1, 0.0
    for a in np.arange(-max_deg, max_deg + 1e-9, step):
        p = rotate(small, a, reshape=False, order=0, prefilter=False).sum(1)
        v = float((p ** 2).sum())
        if v > best: best, ba = v, a
    if abs(ba) < 0.05: return B
    return rotate(B.astype(np.uint8), ba, reshape=False, order=0, prefilter=False) > 0

def _longest_run(row, max_gap=4):
    """(start, end) of the longest run of True in a 1-D bool array, bridging gaps up to max_gap."""
    on = np.where(row)[0]
    if not len(on): return None
    best = (on[0], on[0]); s = on[0]; p = on[0]
    for x in on[1:]:
        if x - p > max_gap + 1:
            if p - s > best[1] - best[0]: best = (s, p)
            s = x
        p = x
    if p - s > best[1] - best[0]: best = (s, p)
    return best

def _pick_staves(st, box_top, box_bot):
    """The two staves of this (piano) system: the staves homr's box covers, or the one it covers plus the nearest
    neighbour when the box holds a single staff (a grand staff with little in one hand)."""
    if not st: return None
    ov = [max(0.0, min(b, box_bot) - max(a, box_top)) / max(1.0, b - a) for a, b in st]
    inside = [i for i, o in enumerate(ov) if o > 0.5]
    if len(inside) >= 2: return st[inside[0]], st[inside[-1]]
    if len(inside) == 1:
        i = inside[0]; a, b = st[i]; h = b - a
        cands = [j for j in (i - 1, i + 1) if 0 <= j < len(st)]
        cands = [j for j in cands if (st[j][0] - b if j > i else a - st[j][1]) < 3.5 * h]
        if cands:
            j = min(cands, key=lambda j: (st[j][0] - b) if j > i else (a - st[j][1]))
            return (st[i], st[j]) if j > i else (st[j], st[i])
        return st[i], st[i]
    return st[0], st[-1]

def find_barlines(gray, box, n_expected, ylim=None):
    """gray: uint8 page; box: (x0,y0,x1,y1) of one system from homr; ylim: (top, bottom) the system may extend to
    (up to the neighbouring systems). Returns (xs, conf, (ytop, ybot), x_left) in page pixels."""
    H, W = gray.shape
    bx0, by0, bx1, by1 = [float(v) for v in box]; bh = max(10.0, by1 - by0)
    lo_lim, hi_lim = ylim if ylim else (0, H)
    ya = int(max(0, lo_lim, by0 - 1.0 * bh)); yb = int(min(H, hi_lim, by1 + 1.0 * bh))
    B = gray[ya:yb, :] < 150
    B = deskew(B, rows=(int(by0 - ya), int(by1 - ya)), cols=(int(max(0, bx0 - 0.03 * W)), int(min(W, bx1 + 0.03 * W))))
    st = staves(B)
    pick = _pick_staves(st, by0 - ya, by1 - ya)
    if pick is None: pick = ((by0 - ya, by0 - ya + bh / 2), (by1 - ya - bh / 2, by1 - ya))
    up, lo = pick
    space = max(4.0, (up[1] - up[0]) / 4.0)
    # horizontal extent of the system: where the staff lines are drawn (columns crossing at least 4 dark rows in the
    # staff band, bridging short breaks), not homr's box
    def extent(stf):
        a = max(0, int(stf[0] - 0.6 * space)); b = int(stf[1] + 0.6 * space) + 1
        return _longest_run(B[a:b].sum(0) >= 4, max_gap=int(2 * space))
    runs = [r for r in (extent(up), extent(lo)) if r and r[1] - r[0] > 0.3 * (bx1 - bx0)]
    if runs: xl, xr = int(min(r[0] for r in runs)), int(max(r[1] for r in runs))
    else: xl, xr = int(bx0), int(bx1)
    cov = lambda a, b: B[int(round(a)):int(round(b)) + 1].mean(0)
    from scipy.ndimage import maximum_filter1d
    cu = cov(*up); cl = cov(*lo)
    # (a barline may lean by a pixel on its way through the gap between the staves)
    gap = maximum_filter1d(cov(up[1] + 2, lo[0] - 2), 3) if lo[0] > up[1] + 6 else np.ones(B.shape[1])
    both = np.minimum(cu, cl)
    # a barline in a slightly rotated scan leans: its top-staff part may be a pixel or two off its bottom-staff part
    # (used only to fill in barlines homr counted but the straight search did not find)
    both_lean = np.max([np.minimum(np.roll(cu, k), np.roll(cl, -k)) for k in (-1, 0, 1)], axis=0)
    both[:max(0, xl)] = 0; both[xr + 1:] = 0; both_lean[:max(0, xl)] = 0; both_lean[xr + 1:] = 0
    top = np.argsort(-both)[: max(3, n_expected * 2)]
    strong = top[both[top] > 0.9]
    connected = len(strong) > 0 and np.mean(gap[strong] > 0.85) > 0.6
    score = np.where(gap > 0.6, both, both * 0.5) if connected else both
    score_lean = np.where(gap > 0.6, both_lean, both_lean * 0.5) if connected else both_lean
    # rows of the staff lines themselves (ignored when looking at what is attached to a vertical stroke)
    lines = set()
    for stf in (up, lo):
        for k in range(5):
            y = int(round(stf[0] + k * space)); lines.update(range(y - 1, y + 2))
    rows = [r for r in range(int(up[0]), int(lo[1]) + 1) if r not in lines and 0 <= r < B.shape[0]]
    def stem_like(x):
        """A barline is a bare line from the top staff line to the bottom one. Stems of chords in both hands that
        happen to line up have noteheads attached along them, and usually stick out above or below the staves."""
        x = int(round(x)); x = max(range(max(0, x - 3), min(B.shape[1], x + 4)), key=lambda c: B[rows, c].mean() if rows else 0)
        a = x; b = x                                  # a thick barline (final, repeat) is several solid columns wide
        while a > 0 and both[a - 1] > 0.9: a -= 1
        while b < len(both) - 1 and both[b + 1] > 0.9: b += 1
        if b - a + 1 >= 0.4 * space: return False
        one = [0, 0]; two = 0                       # rows with ink reaching out on the left only / right only / both
        widths = []
        for r in rows:
            if not B[r, x]: continue
            a = x; b = x
            while a > 0 and B[r, a - 1] and x - a < space: a -= 1
            while b < B.shape[1] - 1 and B[r, b + 1] and b - x < space: b += 1
            widths.append(b - a + 1)
            l, rr = x - a >= 0.5 * space, b - x >= 0.5 * space
            if l and not rr: one[0] += 1
            elif rr and not l: one[1] += 1
            elif l and rr: two += 1
        side = max(one) / max(1, len(rows))           # noteheads sit on one side of a stem; slurs cross both sides
        if widths and np.median(widths) >= 0.45 * space and side < 0.1 and np.std(widths) < 0.5 * space:
            return False                              # a thick (final / repeat) barline: uniformly wide
        above = B[max(0, int(up[0] - 1.2 * space)):max(0, int(up[0] - 2)), max(0, x - 1):x + 2].any(1).mean() if up[0] > 3 else 0
        below = B[int(lo[1] + 2):int(lo[1] + 1.2 * space), max(0, x - 1):x + 2].any(1).mean() if lo[1] + 3 < B.shape[0] else 0
        # stems of both hands together stick out above and below the staves; beams cross a stem, never a barline
        return side > 0.055 or (above > 0.5 and below > 0.5) or two / max(1, len(rows)) > 0.1
    memo = {}
    def groups(thr, arr=None):
        arr = score if arr is None else arr
        cols = np.where(arr > thr)[0]; gs = []
        for c in cols:
            if gs and c - gs[-1][-1] <= 1: gs[-1].append(c)
            else: gs.append([c])
        out = []
        for g in gs:
            if len(g) > 0.7 * space: continue
            x = float(np.mean(g)); sc_ = float(arr[g].max()); key = int(round(x))
            if key not in memo: memo[key] = stem_like(x)
            if memo[key]: sc_ *= 0.5
            if sc_ > thr: out.append((x, sc_))
        merged = []
        for x, s in out:
            if merged and x - merged[-1][0] < 1.4 * space: merged[-1] = (x, max(s, merged[-1][1]))
            else: merged.append((x, s))
        return merged
    inner = lambda cs: [c for c in cs if xl + 3 * space < c[0] < xr - 2 * space]   # not the system's left edge
    # clean strokes through the whole system (not stems) are barlines, however many homr counted; below that,
    # homr's count decides how far down the evidence scale to go
    n_homr = n_expected
    clean = inner(groups(0.95))
    if len(clean) >= n_expected - 1:
        cands = clean
    else:
        # fewer than homr read: weaker strokes, and strokes that lean by a pixel (rotated scans), compete on strength
        pool = inner(groups(0.7)) + inner(groups(0.9, score_lean))
        pool.sort(key=lambda c: -c[1]); cands = []
        for c in pool:
            if all(abs(c[0] - d[0]) > 1.4 * space for d in cands): cands.append(c)
        cands = sorted(cands[:max(0, n_expected - 1)], key=lambda c: c[0])
    n_expected = max(n_expected, len(cands) + 1)
    # just after the clef and key signature, only a clear barline counts (key signatures and accidentals make
    # short vertical strokes through both staves)
    head = xl + 12 * space
    cands = [c for c in cands if c[0] > head or c[1] >= 0.9]
    # more barlines than homr read: extra ones in the clef/key area, or ones that would make a sliver of a bar,
    # are more likely stems of chords that happen to line up
    def narrow(cs):
        pts = [float(xl)] + [c[0] for c in cs] + [float(xr)]; wd = np.diff(pts); med = float(np.median(wd))
        # (a sliver after the last barline is a courtesy key/meter change: that barline is real)
        return [i for i, c in enumerate(cs) if wd[i] < 0.4 * med or (wd[i + 1] < 0.4 * med and i + 1 < len(cs))]
    while len(cands) > max(0, n_homr - 1):
        hd = [c for c in cands if c[0] <= head]
        nr = narrow(cands)
        if hd: cands.remove(min(hd, key=lambda c: c[1]))
        elif nr:   # of two strokes too close together, drop one that does not run through the gap between the staves
            gv = lambda c: float(gap[max(0, int(c[0]) - 2):int(c[0]) + 3].max())
            weak_nr = [cands[i] for i in nr if not connected or gv(cands[i]) < 0.85]   # (a short bar is fine)
            if not weak_nr: break
            cands.remove(min(weak_nr, key=lambda c: (c[1] + gv(c), -c[0])))
        else: break
    # fewer than homr read: look again allowing a slight lean, away from the barlines already found
    if len(cands) < n_homr - 1:
        extra = [c for c in inner(groups(0.9, score_lean)) if c[0] > head and all(abs(c[0] - d[0]) > 3 * space for d in cands)]
        for c in sorted(extra, key=lambda c: -c[1])[:n_homr - 1 - len(cands)]: cands.append(c)
        cands.sort(key=lambda c: c[0])
    n_expected = max(n_homr, len(cands) + 1)
    # the system always ends with a barline at the end of the staff lines
    cands = sorted(cands + [(float(xr), 1.0)], key=lambda c: c[0])
    # a bar much wider than the others with a plausible barline inside it is two bars (OMR missed one)
    # (inside a suspiciously wide bar, a stroke through both staves counts even if it is broken in the gap)
    weak = [c for c in inner(groups(0.7)) + inner(groups(0.9, both)) if c[0] > head]
    for _ in range(4):
        pts = [float(xl)] + [c[0] for c in cands]; wd = np.diff(pts)
        if len(wd) < 2: break
        med = float(np.median(wd)); added = False
        for i in np.argsort(-wd):
            if wd[i] < 1.7 * med: break
            a, b = pts[i], pts[i + 1]
            inside = [c for c in weak if a + 0.2 * (b - a) < c[0] < b - 0.2 * (b - a)]
            if inside:
                c = max(inside, key=lambda c: c[1]); cands = sorted(cands + [c], key=lambda c: c[0]); added = True; break
        if not added: break
    xs = [c[0] for c in cands]; conf = [c[1] for c in cands]
    while len(xs) < n_expected:                              # not enough evidence: split the widest bar, flag it
        pts = [xl] + xs; gaps = [(pts[i + 1] - pts[i], i) for i in range(len(pts) - 1)]
        g, i = max(gaps); xs.insert(i, (pts[i] + pts[i + 1]) / 2); conf.insert(i, 0.0)
    ytop = ya + up[0] - 2.5 * space; ybot = ya + lo[1] + 2.5 * space
    return xs, conf, (max(0, ytop), min(H, ybot)), float(xl)
