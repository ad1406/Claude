"""Render the Night Wind walkthrough to an MP4: the score (two systems at a time, page-turn style) with the motif boxes,
the bar being played and the notes sounding now; the walkthrough text and the statements sounding now on the right;
the timeline with the four motif families at the bottom; the recording as the soundtrack.

    pip install pillow numpy imageio-ffmpeg
    python make_video.py                          # the whole sonata -> night-wind.mp4
    python make_video.py --start 285 --end 300    # bars 285-300 only
    python make_video.py --preview 372            # one frame at bar 372 -> preview.png (to check the layout)

Options: --fps, --size 1920x1080, --families call,song,descent,wind, --motifs call,chrom (only these motifs), --crf (quality,
lower = better), --out. Run it from anywhere; it finds data.js, the page images and "Night Wind.mp3" next to the tool."""
import argparse, bisect, json, math, os, re, subprocess, sys, time
import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__)); NW = os.path.dirname(HERE); REPO = os.path.dirname(NW)

# ---------------------------------------------------------------- fonts
FONT_DIRS = [os.path.join(os.environ.get("WINDIR", "C:\\Windows"), "Fonts"), "/System/Library/Fonts/Supplemental", "/System/Library/Fonts",
             "/Library/Fonts", "/usr/share/fonts/truetype/dejavu", "/usr/share/fonts/truetype/liberation", "/usr/share/fonts"]
CANDIDATES = {
    "regular": ["georgia.ttf", "Georgia.ttf", "DejaVuSerif.ttf", "LiberationSerif-Regular.ttf", "times.ttf"],
    "italic": ["georgiai.ttf", "Georgia Italic.ttf", "DejaVuSerif-Italic.ttf", "LiberationSerif-Italic.ttf", "timesi.ttf"],
    "bold": ["georgiab.ttf", "Georgia Bold.ttf", "DejaVuSerif-Bold.ttf", "LiberationSerif-Bold.ttf", "timesbd.ttf"],
    "sans": ["segoeui.ttf", "arial.ttf", "Arial.ttf", "DejaVuSans.ttf", "LiberationSans-Regular.ttf", "Helvetica.ttc"],
    "sansbold": ["segoeuib.ttf", "arialbd.ttf", "Arial Bold.ttf", "DejaVuSans-Bold.ttf", "LiberationSans-Bold.ttf"],
    "symbol": ["seguisym.ttf", "DejaVuSans.ttf", "Apple Symbols.ttf", "AppleSymbols.ttf", "arialuni.ttf", "Arial Unicode.ttf"],
}
def find_font(kind):
    for name in CANDIDATES[kind]:
        for d in FONT_DIRS:
            p = os.path.join(d, name)
            if os.path.exists(p): return p
            if os.path.isdir(d):              # fonts in sub-folders (Linux)
                for root, _, files in os.walk(d):
                    if name in files: return os.path.join(root, name)
    return None
_fcache = {}
def font(kind, size):
    k = (kind, size)
    if k not in _fcache:
        p = find_font(kind) or (find_font("regular") if kind != "regular" else None)
        _fcache[k] = ImageFont.truetype(p, size) if p else ImageFont.load_default(size)
    return _fcache[k]
SYMBOLS = set("♯♭♮♩♪𝅗𝅥𝅝↑↓◀▶·–—→↔")

def text_w(s, f, fs):
    return sum(font("symbol", fs).getlength(ch) if ch in SYMBOLS else f.getlength(ch) for ch in s) if any(c in SYMBOLS for c in s) else f.getlength(s)

def draw_text(d, xy, s, f, fs, fill):
    """text with music symbols drawn from a symbol font when the main font lacks them"""
    x, y = xy
    if not any(c in SYMBOLS for c in s):
        d.text((x, y), s, font=f, fill=fill); return f.getlength(s)
    x0 = x; buf = ""
    for ch in s + "\0":
        if ch in SYMBOLS or ch == "\0":
            if buf: d.text((x, y), buf, font=f, fill=fill); x += f.getlength(buf); buf = ""
            if ch != "\0":
                sf = font("symbol", fs); d.text((x, y), ch, font=sf, fill=fill); x += sf.getlength(ch)
        else: buf += ch
    return x - x0

# ---------------------------------------------------------------- data
def load():
    raw = open(os.path.join(NW, "data.js"), encoding="utf-8").read()
    D = json.loads(raw[raw.index("{"):raw.rindex("}") + 1])
    tp = os.path.join(NW, "build", "text.json")
    if os.path.exists(tp): T = json.load(open(tp, encoding="utf-8"))
    else:   # no exported text: try node, else run without commentary
        try:
            subprocess.run(["node", os.path.join(NW, "tools", "export_text.js")], check=True); T = json.load(open(tp, encoding="utf-8"))
        except Exception:
            T = dict(thesis="", stations=[[1, len(D["bars"]), "Night Wind", "", ""]])
    return D, T

def hexrgb(h): h = h.lstrip("#"); return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))

def rich_runs(html, D):
    """station HTML -> list of (text, style, color) runs. style: r / i / b / chip"""
    mot = {m["id"]: m for m in D["motifs"]}; fam = {f["id"]: f for f in D["families"]}
    col = {m["id"]: fam[m["family"]]["color"] for m in D["motifs"]}
    s = re.sub(r"\{m:(\w+)\}", lambda m: f"\x01{mot[m.group(1)]['name']}\x02{col[m.group(1)]}\x03" if m.group(1) in mot else m.group(1), html)
    s = re.sub(r"\{f:(\w+)\}", lambda m: f"\x01{fam[m.group(1)]['name']}\x02{fam[m.group(1)]['color']}\x03" if m.group(1) in fam else m.group(1), s)
    s = re.sub(r"\{(\d+)-(\d+)\}", r"bb. \1–\2", s); s = re.sub(r"\{(\d+)\}", r"b. \1", s)
    s = re.sub(r"\s+", " ", s)                                   # source line breaks are just spaces
    s = re.sub(r"\s*<li>\s*", "\n• ", s); s = re.sub(r"\s*</?(ul|p|br)\s*/?>\s*", "\n", s)
    s = s.replace("𝅗𝅥", "half note").replace("𝅝", "whole note")
    runs = []; style = ["r"]
    for tok in re.split(r"(<[^>]+>|\x01[^\x03]*\x03)", s):
        if not tok: continue
        if tok.startswith("\x01"):
            name, c = tok[1:-1].split("\x02"); runs.append((name, "chip", c)); continue
        if tok.startswith("<"):
            t = tok.strip("<>/ ").split()[0].lower() if tok.strip("<>/ ") else ""
            if tok.startswith("</"):
                if len(style) > 1: style.pop()
            elif t in ("i", "em"): style.append("i")
            elif t in ("b", "strong"): style.append("b")
            continue
        runs.append((tok.replace("&amp;", "&").replace("&lt;", "<").replace("&gt;", ">"), style[-1], None))
    return runs

def layout_runs(runs, width, fs, line_h):
    """greedy word wrap -> list of lines, each a list of (x, text, style, color)"""
    lines = [[]]; x = 0
    fonts = {"r": font("regular", fs), "i": font("italic", fs), "b": font("bold", fs), "chip": font("sansbold", int(fs * 0.86))}
    for text, style, color in runs:
        parts = [text] if style == "chip" else re.split(r"(\n| )", text)     # a chip never breaks
        for w in parts:
            if w == "": continue
            if w == "\n":
                if lines[-1]: lines.append([]); x = 0
                continue
            f = fonts[style]; ww = text_w(w, f, fs) + (12 if style == "chip" and w.strip() else 0)
            if w == " ":
                if x > 0: x += f.getlength(" ")
                continue
            if x + ww > width and x > 0: lines.append([]); x = 0
            lines[-1].append((x, w, style, color)); x += ww
    return lines, fonts

# ---------------------------------------------------------------- renderer
class Renderer:
    def __init__(self, D, T, W, H, fams, motifs_only):
        self.D, self.T, self.W, self.H = D, T, W, H
        self.fam = {f["id"]: f for f in D["families"]}
        self.mot = D["motifs"]
        self.show = lambda h: self.mot[h["m"]]["family"] in fams and (not motifs_only or self.mot[h["m"]]["id"] in motifs_only)
        self.notes = D["notes"]; self.hits = [h for h in D["hits"] if self.show(h)]
        self.stations = [dict(a=s[0], b=s[1], title=s[2], mark=s[3], text=s[4]) for s in T["stations"]]
        self.bar_t = D["bar_t"]
        self.top_h = int(H * 0.065); self.tl_h = int(H * 0.11)
        self.score_w = int(W * 0.66); self.panel_x = self.score_w
        self.score_h = H - self.top_h - self.tl_h
        self.pages = {}; self.views = {}; self.panels = {}
        self.bar_sys = {}
        for k, s in enumerate(D["systems"]):
            for b in range(s[5], s[6] + 1): self.bar_sys[b] = k
        # notes by time (for the sounding-now dots)
        idx = [i for i, n in enumerate(self.notes) if n[0] and not n[8]]
        idx.sort(key=lambda i: self.notes[i][3]); self.by_t = idx; self.t_arr = [self.notes[i][3] for i in idx]
        self.hit_t0 = sorted(range(len(self.hits)), key=lambda k: self.hits[k]["t0"])
        self.timeline = self.draw_timeline_base()

    # -- pages and systems
    def page(self, p):
        if p not in self.pages:
            im = Image.open(os.path.join(NW, "build", "png", f"p{p:02d}.png")).convert("RGB")
            self.pages[p] = (im, im.width / self.D["page_w"])
            if len(self.pages) > 6: self.pages.pop(next(iter(self.pages)))
        return self.pages[p]

    def system_crop(self, k):
        s = self.D["systems"][k]; p = s[0]; im, sc = self.page(p)
        ph = self.D["pages"][p - 1]["h"]
        y0 = max(0, s[2] - 1150); y1 = min(ph, s[4] + 1150)
        box = (0, int(y0 * sc), im.width, int(y1 * sc))
        return im.crop(box), (0, y0), sc

    def view(self, k):
        """cached: systems k and k+1 stacked, scaled to the score area, with the static motif boxes and rings"""
        if k in self.views: return self.views[k]
        pad = 14; avail_w = self.score_w - 2 * pad
        ks = [k]; crops = [self.system_crop(k)]
        scale_w = avail_w / crops[0][0].width
        while k + len(ks) < len(self.D["systems"]) and len(ks) < 4:   # add following systems while they fit at full width
            c = self.system_crop(k + len(ks))
            if (sum(x[0].height for x in crops) + c[0].height) * scale_w * 0.85 + 10 * len(ks) > self.score_h - 2 * pad: break
            ks.append(k + len(ks)); crops.append(c)
        if len(ks) == 1 and k + 1 < len(self.D["systems"]):            # always show what comes next, smaller if necessary
            ks.append(k + 1); crops.append(self.system_crop(k + 1))
        avail_h = self.score_h - 2 * pad - 10 * (len(ks) - 1)
        tot_h = sum(c[0].height for c in crops)
        scale = min(scale_w, avail_h / tot_h)
        canvas = Image.new("RGB", (self.score_w, self.score_h), (244, 241, 234))
        geo = {}; y = pad
        for j, (im, (ox, oy), sc) in zip(ks, crops):
            w, h = int(im.width * scale), int(im.height * scale)
            x = (self.score_w - w) // 2
            canvas.paste(im.resize((w, h), Image.LANCZOS), (x, y))
            f = sc * scale                         # page units -> canvas px
            geo[j] = dict(x=x, y=y, f=f, ox=ox, oy=oy, page=self.D["systems"][j][0], w=w, h=h)
            y += h + 10
        over = Image.new("RGBA", canvas.size, (0, 0, 0, 0)); d = ImageDraw.Draw(over)
        segs = []
        for hk, h in enumerate(self.hits):
            for j in ks:
                ns = [i for i in h["n"] if self.notes[i][0] and self.bar_sys.get(self.notes[i][7]) == j]
                if not ns: continue
                g = geo[j]; P = lambda X, Y: (g["x"] + (X - g["ox"]) * g["f"], g["y"] + (Y - g["oy"]) * g["f"])
                xs = [self.notes[i][1] for i in ns]; ys = [self.notes[i][2] for i in ns]
                x0, y0 = P(min(xs) - 70, min(ys) - 165); x1, y1 = P(max(xs) + 360, max(ys) + 165)
                c = hexrgb(self.fam[self.mot[h["m"]]["family"]]["color"])
                d.rounded_rectangle((x0, y0, x1, y1), radius=max(2, int(60 * g["f"])), fill=c + (16,), outline=c + (120,), width=max(1, int(18 * g["f"])))
                rings = [P(self.notes[i][1] + 145, self.notes[i][2]) for i in ns]
                for cx, cy in rings:
                    d.ellipse((cx - 175 * g["f"], cy - 128 * g["f"], cx + 175 * g["f"], cy + 128 * g["f"]), outline=c + (125,), width=max(1, int(20 * g["f"])))
                segs.append(dict(h=hk, box=(x0, y0, x1, y1), rings=rings, c=c, f=g["f"], first=(j == min(self.bar_sys.get(self.notes[i][7], 1e9) for i in h["n"] if self.notes[i][0]))))
        canvas = Image.alpha_composite(canvas.convert("RGBA"), over).convert("RGB")
        bars = {}
        for b, bx in enumerate(self.D["bars"], start=1):
            j = self.bar_sys.get(b)
            if j in geo:
                g = geo[j]; P = lambda X, Y: (g["x"] + (X - g["ox"]) * g["f"], g["y"] + (Y - g["oy"]) * g["f"])
                bars[b] = P(bx[1], bx[2] - 700) + P(bx[3], bx[4] + 700)
        v = dict(img=canvas, geo=geo, segs=segs, bars=bars, ks=ks)
        self.views[k] = v
        if len(self.views) > 4: self.views.pop(next(iter(self.views)))
        return v

    # -- panel (station text + sounding now)
    def panel(self, si, now_keys):
        key = (si, now_keys)
        if key in self.panels: return self.panels[key]
        W = self.W - self.panel_x; H = self.score_h
        im = Image.new("RGB", (W, H), (255, 253, 249)); d = ImageDraw.Draw(im)
        pad = int(W * 0.05); fs = max(14, int(self.H * 0.0185)); lh = int(fs * 1.42)
        st = self.stations[si]
        y = pad
        tf = font("bold", int(fs * 1.25)); draw_text(d, (pad, y), st["title"], tf, int(fs * 1.25), (29, 27, 24)); y += int(fs * 1.25 * 1.4)
        sub = f"bb. {st['a']}–{st['b']}" + (f"  ·  {st['mark']}" if st["mark"] else "")
        sub = sub.replace("𝅗𝅥", "half note")
        lines, fonts = layout_runs([(sub, "i", None)], W - 2 * pad, int(fs * 0.85), lh)
        for ln in lines:
            for x, w, style, c in ln: draw_text(d, (pad + x, y), w, fonts[style], int(fs * 0.85), (109, 103, 93))
            y += int(lh * 0.9)
        y += int(lh * 0.4)
        now_h = int(H * 0.30)
        lines, fonts = layout_runs(rich_runs(st["text"], self.D), W - 2 * pad, fs, lh)
        maxy = H - now_h - pad
        for ln in lines:
            if y + lh > maxy: draw_text(d, (pad, y), "…", fonts["r"], fs, (109, 103, 93)); break
            for x, w, style, c in ln:
                if style == "chip":
                    cc = hexrgb(c); tw = fonts["chip"].getlength(w)
                    d.rounded_rectangle((pad + x, y + 2, pad + x + tw + 10, y + lh - 4), radius=int(lh / 2.6), fill=cc)
                    draw_text(d, (pad + x + 5, y + int(fs * 0.12)), w, fonts["chip"], int(fs * 0.86), (255, 255, 255))
                else:
                    draw_text(d, (pad + x, y), w, fonts[style], fs, (29, 27, 24))
            y += lh
        # sounding now
        y = H - now_h
        d.line((pad, y, W - pad, y), fill=(220, 213, 200), width=1); y += int(lh * 0.5)
        draw_text(d, (pad, y), "Sounding now", font("bold", fs), fs, (29, 27, 24)); y += lh
        sf = font("sans", int(fs * 0.82)); sfb = font("sansbold", int(fs * 0.82))
        if not now_keys: draw_text(d, (pad, y), "—", sf, int(fs * 0.82), (109, 103, 93))
        for hk in now_keys[:7]:
            h = self.hits[hk]; m = self.mot[h["m"]]; c = hexrgb(self.fam[m["family"]]["color"])
            d.rounded_rectangle((pad, y + 5, pad + 12, y + 17), radius=3, fill=c)
            nm = m["name"]; x = pad + 20; draw_text(d, (x, y), nm, sfb, int(fs * 0.82), (29, 27, 24)); x += sfb.getlength(nm) + 8
            desc = h["form"] + ("" if h["mode"] == "exact" else f", {h['mode']}") + (f", {h['aug']}" if h.get("aug") else "") + \
                   ("" if h["level"] == "surface" else ", structural") + f"  ·  bb. {h['bar']}" + (f"–{h['bar_end']}" if h["bar_end"] != h["bar"] else "")
            draw_text(d, (x, y), desc, sf, int(fs * 0.82), (109, 103, 93)); y += int(lh * 0.95)
        if len(now_keys) > 7: draw_text(d, (pad, y), f"+ {len(now_keys) - 7} more", sf, int(fs * 0.82), (109, 103, 93))
        self.panels[key] = im
        if len(self.panels) > 64: self.panels.pop(next(iter(self.panels)))
        return im

    # -- timeline
    def x_of(self, t): return int(t / self.D["duration"] * (self.W - 120)) + 110
    def draw_timeline_base(self):
        W, H = self.W, self.tl_h; im = Image.new("RGB", (W, H), (255, 253, 249)); d = ImageDraw.Draw(im)
        fs = max(10, int(self.H * 0.012)); f = font("sans", fs)
        band = int(H * 0.22)
        for i, s in enumerate(self.stations):
            x0 = self.x_of(self.bar_t[s["a"] - 1]); x1 = self.x_of(self.bar_t[min(s["b"], len(self.bar_t) - 1)])
            d.rectangle((x0, 0, x1, band), fill=(236, 232, 225) if i % 2 else (246, 243, 237))
        lane = (H - band - 6) / 4
        for fi, fm in enumerate(self.D["families"]):
            y = band + 4 + fi * lane; c = hexrgb(fm["color"])
            draw_text(d, (6, y), fm["name"], f, fs, c)
            for h in self.D["hits"]:
                if self.mot[h["m"]]["family"] != fm["id"]: continue
                on = self.show(h)
                d.rectangle((self.x_of(h["t0"]), y + 2, max(self.x_of(h["t0"]) + 1, self.x_of(h["t1"])), y + lane - 3),
                            fill=c if on else tuple(int(v * 0.25 + 245 * 0.75) for v in c))
        p2 = self.x_of(self.bar_t[284]); d.line((p2, 0, p2, H), fill=(176, 65, 46), width=2)
        return im

    # -- one frame
    def frame(self, t):
        D = self.D
        b = max(1, bisect.bisect_right(self.bar_t, t + 1e-3)); b = min(b, len(D["bars"]))
        k = self.bar_sys.get(b, 0); v = self.view(k)
        img = v["img"].copy(); d = ImageDraw.Draw(img, "RGBA")
        if b in v["bars"]:
            x0, y0, x1, y1 = v["bars"][b]; d.rectangle((x0, y0, x1, y1), fill=(243, 181, 17, 26), outline=(243, 181, 17, 230), width=3)
        now = tuple(sorted(hk for hk in range(len(self.hits)) if self.hits[hk]["t0"] <= t <= self.hits[hk]["t1"])) if True else ()
        for sg in v["segs"]:
            if sg["h"] in now:
                c = sg["c"]; f = sg["f"]
                d.rounded_rectangle(sg["box"], radius=max(2, int(60 * f)), fill=c + (40,), outline=c + (255,), width=max(2, int(40 * f)))
                for cx, cy in sg["rings"]:
                    d.ellipse((cx - 175 * f, cy - 128 * f, cx + 175 * f, cy + 128 * f), outline=c + (255,), width=max(2, int(32 * f)))
                if sg["first"]:
                    m = self.mot[self.hits[sg["h"]]["m"]]; fs = max(11, int(self.H * 0.013)); lf = font("sansbold", fs)
                    x0, y0 = sg["box"][0], sg["box"][1] - fs - 4
                    d.rectangle((x0, y0, x0 + lf.getlength(m["name"]) + 8, y0 + fs + 3), fill=(255, 255, 255, 220))
                    d.text((x0 + 4, y0), m["name"], font=lf, fill=c + (255,))
        # sounding notes
        i0 = bisect.bisect_left(self.t_arr, t - 8)
        for kk in range(i0, len(self.t_arr)):
            if self.t_arr[kk] > t: break
            n = self.notes[self.by_t[kk]]
            if t > n[3] + max(0.12, n[4]): continue
            j = self.bar_sys.get(n[7])
            if j not in v["geo"]: continue
            g = v["geo"][j]; cx = g["x"] + (n[1] + 145 - g["ox"]) * g["f"]; cy = g["y"] + (n[2] - g["oy"]) * g["f"]; r = 118 * g["f"]
            d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(243, 181, 17, 215))
        # assemble
        out = Image.new("RGB", (self.W, self.H), (244, 241, 234))
        out.paste(img, (0, self.top_h))
        si = max(i for i, s in enumerate(self.stations) if s["a"] <= b) if any(s["a"] <= b for s in self.stations) else 0
        out.paste(self.panel(si, now), (self.panel_x, self.top_h))
        tl = self.timeline.copy(); td = ImageDraw.Draw(tl)
        st = self.stations[si]; xa = self.x_of(self.bar_t[st["a"] - 1]); xb = self.x_of(self.bar_t[min(st["b"], len(self.bar_t) - 1)])
        td.rectangle((xa, 0, xb, int(self.tl_h * 0.22)), outline=(243, 181, 17), width=2)
        x = self.x_of(t); td.rectangle((x - 1, 0, x + 1, self.tl_h), fill=(224, 160, 0))
        out.paste(tl, (0, self.H - self.tl_h))
        d = ImageDraw.Draw(out); fs = int(self.top_h * 0.36)
        draw_text(d, (18, int(self.top_h * 0.28)), "Medtner · Sonata in E minor, Op. 25 No. 2 “Night Wind”", font("regular", fs), fs, (29, 27, 24))
        right = f"bar {b}   ·   {st['title']}   ·   {int(t // 60)}:{int(t % 60):02d}"
        rf = font("sans", int(fs * 0.9)); draw_text(d, (self.W - 18 - text_w(right, rf, int(fs * 0.9)), int(self.top_h * 0.3)), right, rf, int(fs * 0.9), (109, 103, 93))
        d.line((0, self.top_h - 1, self.W, self.top_h - 1), fill=(220, 213, 200))
        return out

# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--start", type=int, default=1, help="first bar"); ap.add_argument("--end", type=int, default=723, help="last bar")
    ap.add_argument("--fps", type=float, default=24); ap.add_argument("--size", default="1920x1080")
    ap.add_argument("--families", default="call,song,descent,wind"); ap.add_argument("--motifs", default="")
    ap.add_argument("--crf", type=int, default=20); ap.add_argument("--out", default="night-wind.mp4")
    ap.add_argument("--preview", type=int, default=0, help="write one frame at this bar to preview.png and stop")
    ap.add_argument("--audio", default=os.path.join(REPO, "Night Wind.mp3"))
    a = ap.parse_args()
    W, H = (int(x) for x in a.size.lower().split("x"))
    D, T = load()
    R = Renderer(D, T, W, H, set(a.families.split(",")), set(filter(None, a.motifs.split(","))))
    bt = D["bar_t"]
    if a.preview:
        t = bt[a.preview - 1] + 0.4; R.frame(t).save("preview.png"); print("wrote preview.png"); return
    t0 = max(0.0, bt[max(1, a.start) - 1] - (0.0 if a.start > 1 else bt[0]))
    t1 = bt[min(a.end, len(D["bars"]))] + (2.5 if a.end >= len(D["bars"]) else 0.0)
    if a.end >= len(D["bars"]): t1 = min(D["duration"], bt[-1] + 3.0)
    n = int(math.ceil((t1 - t0) * a.fps))
    try:
        import imageio_ffmpeg; ff = imageio_ffmpeg.get_ffmpeg_exe()
    except ImportError:
        ff = "ffmpeg"
    cmd = [ff, "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}", "-r", str(a.fps), "-i", "-",
           "-ss", f"{t0:.3f}", "-t", f"{t1 - t0:.3f}", "-i", a.audio,
           "-map", "0:v", "-map", "1:a", "-c:v", "libx264", "-preset", "medium", "-crf", str(a.crf), "-pix_fmt", "yuv420p",
           "-c:a", "aac", "-b:a", "192k", "-shortest", "-movflags", "+faststart", a.out]
    print(f"bars {a.start}-{a.end}: {fmt(t0)}-{fmt(t1)}, {n} frames at {a.fps} fps -> {a.out}")
    p = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    start = time.time()
    try:
        for i in range(n):
            t = t0 + i / a.fps
            p.stdin.write(R.frame(t).tobytes())
            if i % int(a.fps * 10) == 0 and i:
                el = time.time() - start; eta = el / i * (n - i)
                print(f"\r  {i}/{n} frames  ({100 * i / n:.0f}%)  {fmt(el)} elapsed, about {fmt(eta)} left   ", end="", flush=True)
    finally:
        p.stdin.close(); p.wait()
    print(f"\ndone: {a.out} ({fmt(time.time() - start)})")

def fmt(s): s = int(s); return f"{s // 3600}:{s // 60 % 60:02d}:{s % 60:02d}" if s >= 3600 else f"{s // 60}:{s % 60:02d}"

if __name__ == "__main__":
    main()
