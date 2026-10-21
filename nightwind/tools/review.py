"""Draw motif boxes on rendered pages (PNG) for checking: python review.py page [page...]"""
import json, os, sys, re, cairosvg
sys.path.insert(0, os.path.dirname(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def boxes_svg(hits, L, page, colors):
    """one tight box per system the statement touches; noteheads ringed"""
    sysy = [(sy[2] - 900, sy[4] + 900) for sy in L["systems"] if sy[0] == page]
    out = []
    for h in hits:
        pts = [L["heads"][i] for i in h["ids"] if i in L["heads"] and L["heads"][i][0] == page]
        if not pts: continue
        c = colors[h["motif"]]; segs = {}
        for p in pts:
            k = next((j for j, (a, b) in enumerate(sysy) if a <= p[2] <= b), -1); segs.setdefault(k, []).append(p)
        first = True
        for seg in segs.values():
            xs = [p[1] for p in seg]; ys = [p[2] for p in seg]
            out.append(f'<rect x="{min(xs)-60}" y="{min(ys)-150}" width="{max(xs)-min(xs)+380}" height="{max(ys)-min(ys)+300}" fill="{c}" fill-opacity="0.10" stroke="{c}" stroke-width="22" rx="50"/>')
            if first: out.append(f'<text x="{min(xs)-40}" y="{min(ys)-180}" font-size="200" fill="{c}" font-family="sans-serif">{h["motif"]}</text>'); first = False
        for p in pts: out.append(f'<ellipse cx="{p[1]+140}" cy="{p[2]}" rx="180" ry="130" fill="none" stroke="{c}" stroke-width="26"/>')
    return "\n".join(out)
if __name__ == "__main__":
    from match import Corpus, find; from catalog import MOTIFS, FAMILIES
    only = os.environ.get("ONLY", "").split(",") if os.environ.get("ONLY") else None
    C = Corpus(); L = json.load(open(os.path.join(ROOT, "build", "layout.json")))
    fam = {f["id"]: f["color"] for f in FAMILIES}; colors = {m["id"]: fam[m["family"]] for m in MOTIFS}
    hits = []
    for m in MOTIFS:
        if only and m["id"] not in only: continue
        for h in find(C, m["spec"]): h["motif"] = m["id"]; hits.append(h)
    for a in sys.argv[1:]:
        p = int(a); s = open(os.path.join(ROOT, "build", "svg", f"p{p:02d}.svg")).read()
        k = s.rindex("</svg>"); k = s.rindex("</svg>", 0, k)
        s = s[:k] + boxes_svg(hits, L, p, colors) + s[k:]
        cairosvg.svg2string = None
        cairosvg.svg2png(bytestring=s.encode(), write_to=f"/tmp/claude-0/-home-user-Claude/9fe8fc53-3622-5ca8-afd2-eefbd9d60b5b/scratchpad/rv{p:02d}.png", output_width=1300, background_color="white")
        print("page", p)
