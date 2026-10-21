"""Everything the page needs -> nightwind/data.js"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from match import Corpus, find
from catalog import FAMILIES, MOTIFS
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
B = lambda *p: os.path.join(ROOT, "build", *p)

def main():
    D = json.load(open(B("notes.json"))); L = json.load(open(B("layout.json"))); S = json.load(open(B("sync.json")))
    allnotes = D["notes"]; idx = {n["id"]: k for k, n in enumerate(allnotes)}
    C = Corpus()
    # notes: [page, x, y, time, dur(s), pitch, hand]
    end_q = {}
    byid0 = {n["id"]: n for n in allnotes}
    nxt = {n["tie"]: n for n in allnotes if n["tie"] and n["tie"] in byid0 and byid0[n["tie"]]["pitch"] == n["pitch"]
           and abs(byid0[n["tie"]]["q1"] - n["q0"]) < 0.02}         # only real continuations (unclosed ties in the MusicXML)
    for n in allnotes:
        e = n
        while e["id"] in nxt: e = nxt[e["id"]]
        end_q[n["id"]] = e["q1"]
    import numpy as np
    ev = np.array(S["events"]); eq, et = ev[:, 0], ev[:, 1]
    qt = lambda q: float(np.interp(q, eq, et))
    notes = []
    for n in allnotes:
        h = L["heads"].get(n["id"])
        t0 = S["notes"].get(n["id"], qt(n["q0"])); t1 = qt(end_q[n["id"]])
        notes.append([h[0] if h else 0, h[1] if h else 0, h[2] if h else 0, round(t0, 3), round(max(0.05, t1 - t0), 3), n["pitch"],
                      1 if n["layer"] <= 4 else 2, n["bar"], 1 if n["tie"] else 0])
    fam_index = {f["id"]: k for k, f in enumerate(FAMILIES)}
    motifs = []; hits = []
    for mi, m in enumerate(MOTIFS):
        hs = find(C, m["spec"])
        for h in hs:
            ids = [i for r in h["runs"] for i in r] if h.get("runs") else h["ids"]
            ks = sorted({idx[i] for i in ids})
            hits.append(dict(m=mi, n=ks, core=[idx[i] for i in h["ids"]], bar=h["bar"], bar_end=h["bar_end"],
                             t0=round(min(notes[k][3] for k in ks), 3), t1=round(max(notes[k][3] + notes[k][4] for k in ks), 3),
                             form=h["form"], mode=h["mode"], level=h["level"], aug=h.get("aug"), line=h["line"], hand=h["hand"],
                             p=[allnotes[k]["pitch"] for k in [idx[i] for i in h["ids"]]], also=[list(a) for a in h.get("also", [])]))
        motifs.append(dict(id=m["id"], family=m["family"], name=m["name"], label=m["label"], desc=m["desc"], count=len(hs)))
        print(f"{m['id']:9s} {len(hs)}")
    hits.sort(key=lambda h: (h["t0"], h["m"]))
    # wedges: a rising statement (song family, rising) and a falling one (descent family) sounding together in different lines
    rising = {"ascent", "climb", "songfull", "sigh"}; falling = {"chrom", "steps", "counter"}
    mid = {k: m["id"] for k, m in enumerate(MOTIFS)}
    wedges = []
    R = [(k, h) for k, h in enumerate(hits) if mid[h["m"]] in rising and h["form"] == "prime"]
    F = [(k, h) for k, h in enumerate(hits) if mid[h["m"]] in falling]
    for a, ha in R:
        for b, hb in F:
            ov = min(ha["t1"], hb["t1"]) - max(ha["t0"], hb["t0"])
            if ov <= 1.0 or ha["line"] == hb["line"]: continue
            if set(ha["n"]) & set(hb["n"]): continue
            wedges.append([a, b, round(max(ha["t0"], hb["t0"]), 2), round(min(ha["t1"], hb["t1"]), 2)])
    print("wedges", len(wedges))
    pages = [dict(src=f"svg/p{p:02d}.svg", h=L["page_hs"][p - 1]) for p in range(1, L["pages"] + 1)]
    bars = [L["bars"][str(k)] for k in range(1, len(D["measures"]) + 1)]
    data = dict(page_w=L["page_w"], page_h=L["page_h"], pages=pages, bars=bars, systems=L["systems"],
                bar_t=S["bars"] + [S["end"]], notes=notes, families=FAMILIES, motifs=motifs, hits=hits, wedges=wedges,
                duration=S["stats"]["duration"], audio="../Night Wind.mp3",
                marks=open(B("marks.txt")).read())
    with open(os.path.join(ROOT, "data.js"), "w") as f:
        f.write("window.NW = "); json.dump(data, f, separators=(",", ":")); f.write(";\n")
    print("data.js", os.path.getsize(os.path.join(ROOT, "data.js")) // 1024, "KB")

if __name__ == "__main__":
    main()
