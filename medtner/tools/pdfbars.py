"""Measure boxes from the vector PDFs: staff lines are the long thin horizontal strokes,
barlines the vertical strokes of barline width; systems = pairs of 5-line staves."""
import sys, json, pymupdf, numpy as np

def page_bars(page):
    hs, vs = [], []
    for x in page.get_drawings():
        if x['type'] != 's': continue
        w = round(x.get('width') or 0, 2)
        for it in x['items']:
            if it[0] != 'l': continue
            a, b = it[1], it[2]
            if abs(a.y - b.y) < 0.3 and abs(b.x - a.x) > 100: hs.append((a.y, min(a.x, b.x), max(a.x, b.x), w))
            elif abs(a.x - b.x) < 0.3 and abs(b.y - a.y) > 5: vs.append((a.x, min(a.y, b.y), max(a.y, b.y), w))
    from collections import Counter
    sw = Counter(w for *_, w in hs).most_common(1)[0][0]          # staff-line width; hairpins/lines differ
    hs = sorted(h for h in hs if h[3] == sw)
    ys = []
    for y, x0, x1, w in hs:
        if ys and abs(y - ys[-1][0]) < 0.3: continue
        ys.append((y, x0, x1))
    gaps = np.diff([y[0] for y in ys]); sp = float(np.median(gaps)) if len(gaps) else 4.0
    runs = []
    for k, l in enumerate(ys):
        if runs and l[0] - runs[-1][-1][0] < 1.5 * sp and abs(l[1] - runs[-1][-1][1]) < 2: runs[-1].append(l)
        else: runs.append([l])
    staves = [r for r in runs if len(r) == 5]
    # staves belong to one system when a vertical stroke (the system's left barline / brace line) bridges the gap
    def joined(u, l):
        g0, g1 = u[-1][0], l[0][0]; xl = max(u[0][1], l[0][1])
        return any(abs(x - xl) < 1.5 and y0 <= g0 + 1 and y1 >= g1 - 1 for x, y0, y1, w in vs)
    groups = []
    for st in staves:
        if groups and joined(groups[-1][-1], st): groups[-1].append(st)
        else: groups.append([st])
    systems = []
    for g in groups:
        lines = [l for st in g for l in st]
        top, bot = g[0][0][0], g[-1][-1][0]; x0 = min(l[1] for l in lines); x1 = max(l[2] for l in lines)
        # a barline starts exactly on the top staff's top line and (in pieces) reaches the bottom staff's bottom line;
        # cross-staff stems are as thick but end at noteheads
        st_ = {round(x, 1) for x, y0, y1, w in vs if w >= 0.6 and abs(y0 - top) < 0.6 and x > x0 + 3}
        en_ = {round(x, 1) for x, y0, y1, w in vs if w >= 0.6 and abs(y1 - bot) < 0.6 and x > x0 + 3}
        bl = sorted(x for x in st_ if any(abs(x - e) < 0.3 for e in en_))
        xs = []
        for x in bl:
            if xs and x - xs[-1] < 4: xs[-1] = x; continue
            xs.append(x)
        if not xs: xs.append(x1)      # (staff lines running past the last barline carry a courtesy key/time signature)
        systems.append(dict(top=top, bot=bot, x0=x0, x1=x1, bars=xs))
    return systems

if __name__ == '__main__':
    d = pymupdf.open(sys.argv[1]); out = []
    for i, p in enumerate(d):
        for s in page_bars(p):
            prev = s['x0']
            for x in s['bars']:
                out.append(dict(page=i + 1, box=[prev / p.rect.width, (s['top'] - 28) / p.rect.height, x / p.rect.width, (s['bot'] + 28) / p.rect.height])); prev = x
    print(len(out), file=sys.stderr)
    json.dump(out, open(sys.argv[2], 'w'))
