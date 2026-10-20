"""Score processing: PDF -> page images -> OMR (homr) -> measure boxes + per-bar notes -> movement proposal.

Bars are defined by what is visible on the page (barlines found in each system), because that is what the
user clicks and corrects. homr supplies each system's box, how many barlines it read there, and the notes;
the notes of a system are spread over its visible bars in order (OMR measure splits/merges don't matter)."""
import json, re, subprocess, sys, os, numpy as np, warnings
from pathlib import Path
warnings.filterwarnings("ignore")
from .barlines import find_barlines

DPI = 200

def render_pages(pdf_path, out_dir, dpi=DPI, first=None, last=None, progress=lambda f, m="": None):
    """Pages first..last (1-based, inclusive) of the PDF -> p001.png, p002.png, ..."""
    import pymupdf
    out_dir = Path(out_dir); out_dir.mkdir(parents=True, exist_ok=True)
    d = pymupdf.open(str(pdf_path)); pages = []
    a = max(1, first or 1); b = min(len(d), last or len(d))
    for k, i in enumerate(range(a - 1, b)):
        progress(k / max(1, b - a + 1), f"rendering page {k + 1}")
        p = out_dir / f"p{k + 1:03d}.png"
        if not p.exists(): d[i].get_pixmap(dpi=dpi, colorspace=pymupdf.csGRAY).save(str(p))
        pages.append(p)
    return pages

def run_homr(png, log_path):
    """homr writes <page>.musicxml and (with --write-staff-positions) <page>.txt next to the image."""
    png = Path(png)
    if png.with_suffix(".musicxml").exists() and png.with_suffix(".txt").exists() and Path(log_path).exists():
        return True
    code = "import sys, warnings; warnings.filterwarnings('ignore'); from homr.main import main; sys.argv=['homr','--write-staff-positions',sys.argv[1]]; main()"
    with open(log_path, "w", encoding="utf-8") as lf:
        r = subprocess.run([sys.executable, "-W", "ignore", "-c", code, str(png)], stdout=lf, stderr=subprocess.STDOUT, cwd=str(png.parent))
    return r.returncode == 0 and png.with_suffix(".musicxml").exists()

def _system_counts(log_path):
    s = Path(log_path).read_text(encoding="utf-8", errors="ignore"); w = s.rfind("Writing XML")
    if w < 0: return []
    s = s[w:]
    return [t.count("barline") for t in s.split("newline")]

def _xml_systems(xml_path):
    """[(system_index, measure music21 object)] in order, using the <print new-system> breaks in homr's MusicXML."""
    import music21 as m21
    raw = Path(xml_path).read_text(encoding="utf-8", errors="ignore")
    chunks = re.split(r"(?=<measure[\s>])", raw)[1:]
    sysidx = []; k = -1
    for c in chunks:
        if 'new-system="yes"' in c or k < 0: k += 1
        sysidx.append(k)
    s = m21.converter.parse(str(xml_path))
    parts = [list(p.getElementsByClass("Measure")) for p in s.parts]
    # MusicXML measures are shared by staves in one part; music21 splits staves into parts with equal measure lists
    nmeas = len(parts[0]) if parts else 0
    per = []
    for i in range(nmeas):
        notes = []; content = 0.0; ts = None; reps = []
        for p in parts:
            if i >= len(p): continue
            m = p[i]
            if m.timeSignature is not None and ts is None: ts = m.timeSignature.ratioString
            for bl in (m.leftBarline, m.rightBarline):
                if bl is not None and getattr(bl, "direction", None): reps.append(("start" if bl is m.leftBarline else "end", bl.direction))
            for n in m.recurse().notes:
                off = float(n.getOffsetInHierarchy(m)); d = float(n.quarterLength) or 0.1
                for pt in n.pitches: notes.append((off, d, int(pt.midi)))
                content = max(content, off + d)
        per.append(dict(sys=sysidx[i] if i < len(sysidx) else (sysidx[-1] if sysidx else 0), notes=notes, content=content, ts=ts, repeats=reps))
    return per

def analyse_page(png, xml, txt, log):
    """-> list of systems: dict(box, bars=[dict(x0,x1,y0,y1,conf)], notes=[...], ts, repeat_marks)"""
    from PIL import Image
    g = np.array(Image.open(png).convert("L")); H, W = g.shape
    boxes = []
    if Path(txt).exists():
        for line in Path(txt).read_text().splitlines():
            p = line.split()
            if len(p) >= 5:
                _, cx, cy, w, h = map(float, p[:5]); boxes.append(((cx - w / 2) * W, (cy - h / 2) * H, (cx + w / 2) * W, (cy + h / 2) * H))
    boxes.sort(key=lambda b: b[1])
    counts = _system_counts(log)
    meas = _xml_systems(xml) if Path(xml).exists() else []
    systems = []
    for si, box in enumerate(boxes):
        n = counts[si] if si < len(counts) and counts[si] > 0 else 4
        ylim = (boxes[si - 1][3] if si > 0 else 0, boxes[si + 1][1] if si + 1 < len(boxes) else H)
        xs, conf, (yt, yb), xl = find_barlines(g, box, n, ylim)
        bars = []; prev = xl
        for x, c in zip(xs, conf):
            bars.append(dict(x0=prev / W, x1=x / W, y0=yt / H, y1=yb / H, conf=round(float(c), 2))); prev = x
        # the system's notes as one timeline, then cut into the visible bars (equal shares)
        ms = [m for m in meas if m["sys"] == si]
        per_bar = [m["notes"] for m in ms] if len(ms) == len(bars) else None     # homr's own measures, when they agree
        per_bar_rep = [m["repeats"] for m in ms] if len(ms) == len(bars) else None
        tl = []; t = 0.0; ts = None; reps = []
        for m in ms:
            for o, d, p in m["notes"]: tl.append((t + o, d, p))
            t += max(m["content"], 0.25); ts = ts or m["ts"]
            reps += [(len(tl), r) for r in m["repeats"]]
        systems.append(dict(box=[b / (W if k % 2 == 0 else H) for k, b in enumerate(box)], bars=bars, timeline=tl, total_q=t, ts=ts, per_bar=per_bar, per_bar_rep=per_bar_rep,
                            x_left=xl / W, y0=yt / H, y1=yb / H,
                            indent=(xl / W), n_xml=len(ms), xml_repeats=[r for m in ms for r in m["repeats"]]))
    return systems

def build_bars(pages_systems):
    """Flatten into bars with notes. pages_systems: [(page_no, systems)]. Returns list of bar dicts."""
    bars = []
    for pg, systems in pages_systems:
        for si, s in enumerate(systems):
            N = max(1, len(s["bars"])); T = max(s["total_q"], 1e-3); share = T / N
            for k, b in enumerate(s["bars"]):
                a, z = k * share, (k + 1) * share
                if s.get("per_bar") is not None: notes = s["per_bar"][k]
                else: notes = [(o - a, d, p) for o, d, p in s["timeline"] if a - 1e-6 <= o < z - 1e-6]
                content = max([o + d for o, d, _ in notes], default=0.0)
                bars.append(dict(page=pg, system=si, idx=k, box=[b["x0"], b["y0"], b["x1"], b["y1"]], conf=b["conf"],
                                 notes=notes, share_q=share, content_q=content, ts=s["ts"], indent=s["indent"]))
    return bars

def propose_movements(bars, pages_systems):
    """New movement where a system is clearly indented (typical for the first system of a movement)."""
    lefts = [s["box"][0] for _, ss in pages_systems for s in ss]
    if not lefts: return [0]
    base = float(np.median(lefts)); starts = [0]; k = 0
    for pg, ss in pages_systems:
        for si, s in enumerate(ss):
            wide = s["box"][2] - s["box"][0] > 0.5            # a full-width system, not a small fragment
            if k > 0 and wide and s["box"][0] > base + 0.035: starts.append(k)
            k += len(s["bars"])
    return starts
