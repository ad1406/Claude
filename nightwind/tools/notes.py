"""Note table from verovio (same ids as the SVG): id, bar, staff, layer, chord, onset/offset (quarters), midi, grace, tied-from."""
import json, os, re, verovio, xml.etree.ElementTree as ET
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
NS = "{http://www.music-encoding.org/ns/mei}"; XID = "{http://www.w3.org/XML/1998/namespace}id"
def toolkit():  # (kept for render parity)
    tk = verovio.toolkit()
    tk.setOptions({"breaks": "encoded", "pageWidth": 2100, "pageHeight": 2970, "scale": 40, "adjustPageHeight": True,
                   "footer": "none", "header": "auto", "svgHtml5": True, "xmlIdSeed": 1})
    assert tk.loadFile(os.path.join(ROOT, "src", "night-wind.mxl")); return tk
if __name__ == "__main__":
    tm = json.load(open(os.path.join(ROOT, "build", "timemap.json")))
    on = {}; off = {}; mon = {}
    for e in tm:
        for i in e.get("on", []): on.setdefault(i, (e["qstamp"], e["tstamp"]))
        for i in e.get("off", []): off[i] = (e["qstamp"], e["tstamp"])
        if "measureOn" in e: mon[e["measureOn"]] = (e["qstamp"], e["tstamp"])
    root = ET.parse(os.path.join(ROOT, "build", "score.mei")).getroot()
    tie_to = {}
    for t in root.iter(NS + "tie"):
        a, b = t.get("startid", "").lstrip("#"), t.get("endid", "").lstrip("#"); tie_to[b] = a
    rows = []; measures = []
    for k, m in enumerate(root.iter(NS + "measure")):
        mid = m.get(XID); measures.append(dict(id=mid, n=m.get("n"), q=mon.get(mid, (None, None))[0], ms=mon.get(mid, (None, None))[1]))
        for st in m.findall(NS + "staff"):
            for ly in st.findall(NS + "layer"):
                parent = {c: q for q in ly.iter() for c in q}
                for el in ly.iter():
                    if el.tag != NS + "note": continue
                    up = parent.get(el); chord = up.get(XID) if up is not None and up.tag == NS + "chord" else None
                    dstaff = el.get("staff") or (up.get("staff") if up is not None and up.tag == NS + "chord" else None) or st.get("n")
                    nid = el.get(XID)
                    if nid not in on: continue
                    pc = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}[el.get("pname")]
                    acc = el.get("accid") or el.get("accid.ges"); ch = el.find(NS + "accid")
                    if ch is not None: acc = ch.get("accid") or ch.get("accid.ges") or acc
                    alt = {"s": 1, "f": -1, "n": 0, "ss": 2, "x": 2, "ff": -2, None: 0}.get(acc, 0)
                    mv = {"pitch": 12 * (int(el.get("oct")) + 1) + pc + alt}
                    rows.append(dict(id=nid, bar=k + 1, staff=int(dstaff), layer=int(ly.get("n")), chord=chord, q0=round(on[nid][0], 4),
                                     q1=round(off.get(nid, on[nid])[0], 4), t0=on[nid][1], pitch=mv.get("pitch"),
                                     grace=1 if el.get("grace") else 0, tie=tie_to.get(nid)))
    # octaves: MusicXML <pitch> is the sounding pitch (ottavas are display only), so take each note's octave from it,
    # matched by bar, onset within the bar and pitch class
    xml = ET.parse(os.path.join(ROOT, "src", "mxl", "score.xml")).getroot()
    STEP = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
    sound = {}
    for part in xml.findall("part"):
        div = 1
        for k, ms in enumerate(part.findall("measure")):
            t = 0.0; last = 0.0
            for el in ms:
                if el.tag == "attributes" and el.find("divisions") is not None: div = int(el.find("divisions").text)
                elif el.tag == "backup": t -= int(el.find("duration").text) / div
                elif el.tag == "forward": t += int(el.find("duration").text) / div
                elif el.tag == "note":
                    d = int(el.find("duration").text) / div if el.find("duration") is not None else 0.0
                    on = last if el.find("chord") is not None else t
                    if el.find("chord") is None: last = t
                    pt = el.find("pitch")
                    if pt is not None:
                        mp = 12 * (int(pt.find("octave").text) + 1) + STEP[pt.find("step").text] + int(float(pt.findtext("alter") or 0))
                        sound.setdefault((k + 1, round(on, 2), mp % 12), []).append(mp)
                    if el.find("chord") is None and el.find("grace") is None: t += d
    mq = {i + 1: m["q"] for i, m in enumerate(measures)}
    fixed = miss = 0
    bypc = {}
    for (bar, on, pc), v in sound.items(): bypc.setdefault((bar, pc), []).append((on, v))
    for r in rows:
        rel = round(r["q0"] - mq[r["bar"]], 2)
        c = sound.get((r["bar"], rel, r["pitch"] % 12))
        if not c:   # grace notes and rounding: nearest onset of the same pitch class in the bar
            cand = [x for x in bypc.get((r["bar"], r["pitch"] % 12), []) if abs(x[0] - rel) <= 0.5]
            c = min(cand, key=lambda x: abs(x[0] - rel))[1] if cand else None
        if not c: miss += 1; continue
        best = min(c, key=lambda x: abs(x - r["pitch"]))
        if best != r["pitch"]: fixed += 1; r["pitch"] = best
    print("octave corrections", fixed, "unmatched", miss)
    json.dump(dict(measures=measures, notes=rows), open(os.path.join(ROOT, "build", "notes.json"), "w"))
    print(len(measures), "measures", len(rows), "notes", sum(1 for r in rows if r["pitch"] is None), "without pitch")
