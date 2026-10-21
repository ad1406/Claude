"""MXL -> verovio: page SVGs (original system/page breaks), MEI, timemap. Everything later keys on verovio's note ids."""
import json, os, sys, verovio
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
verovio.enableLog(verovio.LOG_OFF) if hasattr(verovio, "LOG_OFF") else None
tk = verovio.toolkit()
tk.setOptions({"breaks": "encoded", "pageWidth": 2100, "pageHeight": 2970, "scale": 40, "adjustPageHeight": True,
               "footer": "none", "header": "auto", "svgHtml5": True, "svgBoundingBoxes": False, "xmlIdSeed": 1})
assert tk.loadFile(os.path.join(ROOT, "src", "night-wind.mxl"))
n = tk.getPageCount(); print("pages", n)
os.makedirs(os.path.join(ROOT, "build", "svg"), exist_ok=True)
import re
# tempo marks use SMuFL text glyphs (Leipzig font), which browsers don't have: use the Unicode music symbols instead
SMUFL = {"\ue1d2": "\U0001D15D", "\ue1d3": "\U0001D15E", "\ue1d5": "\u2669", "\ue1d7": "\u266A", "\ue1e7": ".", "\ue1d9": "\U0001D161",
         "\ueca2": "\U0001D15D", "\ueca3": "\U0001D15E", "\ueca5": "\u2669", "\ueca7": "\u266A", "\uecb7": ".", " ": ""}
def fix(svg):
    def rep(m):
        t = "".join(SMUFL.get(c, c) for c in m.group(1))
        return f'<tspan font-family="Times, serif" font-size="460px">{t}</tspan>'
    return re.sub(r'<tspan font-family="Leipzig" font-size="[\d.]+px">([^<]*)</tspan>', rep, svg)
for p in range(1, n + 1):
    open(os.path.join(ROOT, "build", "svg", f"p{p:02d}.svg"), "w").write(fix(tk.renderToSVG(p)))
open(os.path.join(ROOT, "build", "score.mei"), "w").write(tk.getMEI({"pageNo": 0, "scoreBased": True}))
json.dump(tk.renderToTimemap({"includeMeasures": True, "includeRests": False}), open(os.path.join(ROOT, "build", "timemap.json"), "w"))
