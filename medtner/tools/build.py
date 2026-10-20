"""Build medtner/data.js: page images, bar boxes, recording sync, motif statements, estimated key plan, tempo.
Run from the repo root: python medtner/tools/build.py (needs pymupdf, pretty_midi, librosa, synctoolbox)."""
import json, os, sys, bisect, warnings, numpy as np, pymupdf, pretty_midi as pm
warnings.filterwarnings("ignore")
sys.path.insert(0, os.path.dirname(__file__))
import pdfbars, motifs as mf
from catalog import CATALOG
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "medtner")
PIECES = {
    "nw": dict(title="Sonata in E minor, Op. 25 No. 2 “Night Wind”", pdf="Nikolai Medtner Op. 25 No. 2 - Piano Sonata.pdf",
               mid="Nikolai Medtner Op. 25 No. 2 - Piano Sonata.mid", audio="Night Wind.mp3", first=1),
    "mi": dict(title="Sonata minacciosa, Op. 53 No. 2", pdf="Nikolai Medtner Op.53 No.2 - Sonata Minacciosa.pdf",
               mid="Nikolai Medtner Op.53 No.2 - Sonata Minacciosa.mid", audio="Minacciosa.mp3", first=0),
}

def pages_and_bars(key, P):
    d = pymupdf.open(os.path.join(ROOT, P["pdf"])); bars = []; pages = []
    for i, p in enumerate(d):
        fn = f"pages/{key}-{i + 1:02d}.webp"; path = os.path.join(OUT, fn)
        if not os.path.exists(path):
            pix = p.get_pixmap(dpi=150, colorspace=pymupdf.csGRAY)
            from PIL import Image
            Image.frombytes("L", (pix.width, pix.height), pix.samples).save(path, "WEBP", quality=70, method=6)
        pages.append(dict(src=fn, w=round(p.rect.width, 1), h=round(p.rect.height, 1)))
        W, H = p.rect.width, p.rect.height
        for s in pdfbars.page_bars(p):
            prev = s["x0"]
            for x in s["bars"]:
                bars.append([i + 1, round(prev / W, 4), round((s["top"] - 26) / H, 4), round(x / W, 4), round((s["bot"] + 26) / H, 4)]); prev = x
    return pages, bars

def sync(key, P):
    cache = os.path.join(OUT, "tools", f"sync-{key}.json")
    if not os.path.exists(cache):
        import subprocess
        subprocess.run([sys.executable, os.path.join(OUT, "tools", "sync.py"), os.path.join(ROOT, P["mid"]), os.path.join(ROOT, P["audio"]), cache], check=True)
    return json.load(open(cache))

def chroma_profile(m, db, end):
    """Per bar: how evenly the 12 pitch classes are used (duration-weighted, 4-bar window), 0 = one pitch class, 1 = all equal."""
    nb = len(db); H = np.zeros((nb, 12)); edges = list(db) + [end]
    for inst in m.instruments:
        for n in inst.notes:
            a = bisect.bisect_right(db, n.start + 1e-4) - 1; b = bisect.bisect_right(db, n.end - 1e-4) - 1
            for k in range(max(0, a), min(nb, b + 1)):
                ov = min(n.end, edges[k + 1]) - max(n.start, edges[k])
                if ov > 0: H[k, n.pitch % 12] += ov / max(1e-3, edges[k + 1] - edges[k])
    from scipy.ndimage import uniform_filter1d
    Hs = uniform_filter1d(H, size=4, axis=0, mode="nearest") + 1e-9
    P = Hs / Hs.sum(1, keepdims=True)
    return [round(float(x), 3) for x in -(P * np.log(P)).sum(1) / np.log(12)]

def main():
    data = dict(pieces={})
    for key, P in PIECES.items():
        pages, boxes = pages_and_bars(key, P)
        s = sync(key, P)
        piece = mf.Piece(os.path.join(ROOT, P["mid"]), P["first"])
        m = pm.PrettyMIDI(os.path.join(ROOT, P["mid"])); db = list(m.get_downbeats())
        assert len(boxes) == len(db), (key, len(boxes), len(db))
        path = np.array(s["path"])                      # [midi_t, audio_t] at 2 Hz
        a2 = lambda t: float(np.interp(t, path[:, 0], path[:, 1]))
        bar_t = s["bars"] + [s["end"]]
        nominal = np.diff(db + [piece.end])
        perf = np.diff(bar_t)
        tempo = [round(float(n / p), 3) if p > 0 else None for n, p in zip(nominal, perf)]
        mots = []
        for mo in CATALOG[key]:
            hits = mf.find(piece, mo["def"])
            for h in hits:
                h["a0"] = round(a2(h["t0"]), 3); h["a1"] = round(a2(h["notes"][-1][1]), 3)
                h["pitches"] = [pm.note_number_to_name(n[2]) for n in h["notes"]]
                for k in ("i", "t0", "t1", "notes", "line"): h.pop(k, None)
            mots.append(dict(id=mo["id"], name=mo["name"], color=mo["color"], desc=mo["desc"], kind=mo["kind"], hits=hits))
            print(key, mo["id"], len(hits))
        data["pieces"][key] = dict(title=P["title"], audio="../" + P["audio"], first=P["first"], pages=pages, bars=boxes,
                                   times=[round(t, 3) for t in bar_t], tempo=tempo, chroma=chroma_profile(m, db, piece.end), motifs=mots,
                                   duration=s["duration"])
    with open(os.path.join(OUT, "data.js"), "w") as f:
        f.write("window.MEDTNER_DATA = "); json.dump(data, f, separators=(",", ":")); f.write(";\n")
    print("wrote data.js", os.path.getsize(os.path.join(OUT, "data.js")) // 1024, "KB")

if __name__ == "__main__":
    main()
