"""From verovio's page SVGs: notehead position of every note, box of every measure, box of every system (viewBox units)."""
import json, os, re
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OPEN = re.compile(r'<g\b([^>]*)>|</g>|<use [^>]*transform="translate\(([-\d.]+), ([-\d.]+)\)[^>]*/>|<path [^>]*d="M(\d+) (\d+) L(\d+) (\d+)"')
ATTR_ID = re.compile(r'data-id="([^"]+)"'); ATTR_CL = re.compile(r'class="([^"]+)"')

def main():
    D = json.load(open(os.path.join(ROOT, "build", "notes.json")))
    mid2n = {m["id"]: k + 1 for k, m in enumerate(D["measures"])}
    heads, bars, systems = {}, {}, []
    sysg = {}; barx = {}; order = []; msys = {}; heights = []
    pages = sorted(os.listdir(os.path.join(ROOT, "build", "svg")))
    for pi, fn in enumerate(pages):
        s = open(os.path.join(ROOT, "build", "svg", fn)).read()
        heights.append(int(re.search(r'class="definition-scale"[^>]*viewBox="0 0 \d+ (\d+)"', s).group(1)))
        stack = []; ox, oy = 500, 500
        for m in OPEN.finditer(s):
            tok = m.group(0)
            if tok.startswith("</g"):
                if stack: stack.pop()
                continue
            if tok.startswith("<g"):
                a = m.group(1) or ""; i = ATTR_ID.search(a); c = ATTR_CL.search(a)
                stack.append((i.group(1) if i else None, c.group(1) if c else ""))
                continue
            classes = [c for _, c in stack]; ids = {c: i for i, c in stack}
            cur_note = next((i for i, c in reversed(stack) if c == "note"), None)
            cur_meas = next((i for i, c in reversed(stack) if c == "measure"), None)
            cur_sys = next((i for i, c in reversed(stack) if c == "system"), None)
            if tok.startswith("<use") and cur_note and stack and stack[-1][1] == "notehead":
                heads[cur_note] = [pi + 1, int(float(m.group(2))) + ox, int(float(m.group(3))) + oy]
            elif tok.startswith("<path"):
                x1, y1, x2, y2 = (int(m.group(k)) for k in (4, 5, 6, 7))
                if stack and stack[-1][1] == "staff" and y1 == y2 and x2 > x1 and cur_sys:      # staff line (drawn per system)
                    sb = sysg.setdefault(cur_sys, [pi + 1, 1e9, 1e9, -1e9, -1e9])
                    sb[1] = min(sb[1], x1 + ox); sb[3] = max(sb[3], x2 + ox); sb[2] = min(sb[2], y1 + oy); sb[4] = max(sb[4], y1 + oy)
                elif stack and stack[-1][1] == "barLine" and x1 == x2 and cur_meas:
                    bl = barx.setdefault(cur_meas, [cur_sys, -1e9]); bl[1] = max(bl[1], x1 + ox)
            if cur_meas and cur_meas not in order: order.append(cur_meas); msys[cur_meas] = cur_sys
    prev_sys = None; left = None; sysbox = {}
    for mid in order:
        sy = msys[mid]; sb = sysg.get(sy)
        if sb is None: continue
        if sy != prev_sys: left = sb[1]; prev_sys = sy
        right = barx.get(mid, [sy, sb[3]])[1]
        if right < left: right = sb[3]
        n = mid2n.get(mid)
        if n: bars[n] = [sb[0], left, sb[2], right, sb[4]]
        x = sysbox.setdefault(sy, sb + [n or 10**6, n or 0]); x[5] = min(x[5], n or 10**6); x[6] = max(x[6], n or 0)
        left = right
    out = dict(page_w=21000, page_h=max(heights), page_hs=heights, pages=len(pages), heads=heads,
               bars={n: b[:5] for n, b in sorted(bars.items())}, systems=sorted(sysbox.values(), key=lambda s: s[5]))
    json.dump(out, open(os.path.join(ROOT, "build", "layout.json"), "w"))
    print(len(heads), "noteheads,", len(bars), "bars,", len(sysbox), "systems")

if __name__ == "__main__":
    main()
