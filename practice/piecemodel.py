"""The piece as the app sees it: score.json (pages, systems, visible bars + notes) and structure.json
(movements, meters, repeat/ending/D.C. markup, deleted bars). Conversions to the alignment engine's model."""
import numpy as np
from . import store

def bar_id(page, system, idx): return f"p{page}s{system}b{idx}"

def bars_for_system(s, page, si):
    """(Re)build the visible bars of one system from its barline x positions and its notes."""
    xs = s["barlines"]; N = max(1, len(xs)); T = max(s.get("total_q", 0), 1e-3); share = T / N
    per_bar = s.get("per_bar") if s.get("per_bar") is not None and len(s["per_bar"]) == N else None
    out = []; prev = s["x_left"]
    for k, x in enumerate(xs):
        a, z = k * share, (k + 1) * share
        notes = per_bar[k] if per_bar else [(o - a, d, p) for o, d, p in s.get("timeline", []) if a - 1e-6 <= o < z - 1e-6]
        content = max([o + d for o, d, _ in notes], default=0.0)
        out.append(dict(id=bar_id(page, si, k), page=page, system=si, idx=k, box=[prev, s["y0"], x, s["y1"]],
                        conf=(s.get("conf") or [1.0] * N)[k] if k < len(s.get("conf") or []) else 1.0,
                        notes=[list(n) for n in notes], content_q=content, share_q=share))
        prev = x
    return out

def build_score(pages_systems, page_sizes):
    """pages_systems: [(page_no, systems from engine.score.analyse_page)] -> score dict"""
    systems = []; bars = []
    for pg, ss in pages_systems:
        for si, s in enumerate(ss):
            sysd = dict(page=pg, idx=si, box=s["box"], x_left=s["x_left"], y0=s["y0"], y1=s["y1"],
                        barlines=[b["x1"] for b in s["bars"]], conf=[b["conf"] for b in s["bars"]],
                        timeline=[list(t) for t in s["timeline"]], total_q=s["total_q"], ts=s["ts"],
                        per_bar=[[list(n) for n in nb] for nb in s["per_bar"]] if s.get("per_bar") is not None else None,
                        per_bar_rep=s.get("per_bar_rep"), n_xml=s.get("n_xml"))
            systems.append(sysd); bars += bars_for_system(sysd, pg, si)
    return dict(pages=[dict(n=n, w=w, h=h) for n, (w, h) in page_sizes], systems=systems, bars=bars)

def rebuild_bars(score):
    bars = []
    for s in score["systems"]: bars += bars_for_system(s, s["page"], s["idx"])
    score["bars"] = bars; return score

def _meter_q(ts):
    try:
        n, d = ts.split("/"); return int(n) * 4.0 / int(d)
    except Exception: return None

def _plausible(ts):
    try: n, d = (int(x) for x in ts.split("/"))
    except Exception: return False
    return 1 <= n <= 12 and d in (1, 2, 4, 8, 16)

def guess_meter(bars, sysmap):
    """A movement's meter: the time signature printed at its start (as homr read it), cross-checked against how
    much music the bars actually hold; misreads fall back to the typical bar length."""
    seen, tss = set(), []
    for b in bars:                                   # systems in order, each once
        key = (b["page"], b["system"])
        if key in seen: continue
        seen.add(key); t = sysmap[key].get("ts")
        if t and _plausible(t): tss.append(t)
    cont = sorted(b["content_q"] for b in bars[1:] if b.get("content_q"))
    med = cont[len(cont) // 2] if cont else None
    for t in ([tss[0]] if tss else []) + sorted(set(tss), key=tss.count, reverse=True):
        if med is None or 0.7 <= med / _meter_q(t) <= 1.4: return t
    if med:
        q = round(med * 2) / 2
        return f"{int(q * 2)}/8" if q % 1 else f"{int(q)}/4"
    return tss[0] if tss else "4/4"

def default_structure(score, mv_starts):
    bars = score["bars"]; ids = [b["id"] for b in bars]
    sysmap = {(s["page"], s["idx"]): s for s in score["systems"]}
    movements = []
    for k, st in enumerate(mv_starts):
        end = mv_starts[k + 1] if k + 1 < len(mv_starts) else len(bars)
        ts = guess_meter(bars[st:end], sysmap)
        q = _meter_q(ts) or 4.0
        first = bars[st]; pickup = 0 < first["content_q"] < 0.6 * q
        movements.append(dict(name=["I", "II", "III", "IV", "V", "VI", "VII", "VIII"][k] if len(mv_starts) > 1 else "", start=ids[st],
                              meter=ts, pickup=bool(pickup)))
    # repeat signs read by homr, where its measures line up with the visible bars
    markup = {}
    for s in score["systems"]:
        reps = s.get("per_bar_rep")
        if not reps: continue
        for k, rs in enumerate(reps):
            for side, direction in rs:
                bid = bar_id(s["page"], s["idx"], k)
                if direction == "forward": markup.setdefault(bid, {})["repeat_start"] = 1
                if direction == "backward": markup.setdefault(bid, {})["repeat_end"] = 1
    # a sliver at either end of a line is usually a courtesy meter/key change or a repeat sign, not a bar
    deleted = []
    for s in score["systems"]:
        xs = [s["x_left"]] + s["barlines"]; w = [xs[i + 1] - xs[i] for i in range(len(xs) - 1)]
        if len(w) < 3: continue
        med = float(np.median(w))
        k = len(w) - 1                      # courtesy key/meter change after the last barline of a line
        if w[k] < 0.3 * med and w[k] < 0.045: deleted.append(bar_id(s["page"], s["idx"], k))
    return dict(movements=movements, markup=markup, deleted=deleted, version=1)

def movement_of(score, structure):
    """bar id -> movement index; plus ordered bar lists per movement (deleted bars excluded)."""
    ids = [b["id"] for b in score["bars"]]; pos = {b: i for i, b in enumerate(ids)}
    starts = sorted([(pos.get(m["start"], 0), k) for k, m in enumerate(structure["movements"])])
    deleted = set(structure.get("deleted", []))
    lists = [[] for _ in structure["movements"]]; mv = {}
    for i, b in enumerate(ids):
        k = max([kk for p, kk in starts if p <= i], default=starts[0][1] if starts else 0)
        if b in deleted: continue
        lists[k].append(b); mv[b] = k
    return mv, lists

def labels(score, structure):
    """Printed-style bar numbers per movement: pickup = 0, endings share a number (12a, 12b)."""
    mv, lists = movement_of(score, structure); mk = structure.get("markup", {}); out = {}
    for k, ids in enumerate(lists):
        m = structure["movements"][k]; n = 0 if m.get("pickup") else 1; prev_end = None; letter = 0
        for i, b in enumerate(ids):
            e = mk.get(b, {}).get("ending")
            if e and prev_end is not None and prev_end != e:      # a later ending in the same group: same number
                letter += 1; out[b] = f"{n - 1}{'abcdefgh'[letter]}"; prev_end = e; continue
            if e and prev_end is None: letter = 0; out[b] = f"{n}a"; n += 1; prev_end = e; continue
            if not e: prev_end = None
            out[b] = str(n); n += 1
    return out

def engine_model(score, structure, span=None):
    """The model the aligner uses. span=(a, b): only those bars (plus one before and two after), played straight
    through: for a take of one passage, which is far more robust than searching the whole piece."""
    bars = {b["id"]: b for b in score["bars"]}; mv, lists = movement_of(score, structure)
    keep = None
    if span:
        flat = [b for l in lists for b in l]
        if span[0] in flat and span[1] in flat:
            ia, ib = sorted((flat.index(span[0]), flat.index(span[1])))
            keep = set(flat[max(0, ia - 1): ib + 3])
    model_bars = {}; movements = []
    for k, ids in enumerate(lists):
        m = structure["movements"][k]; q = _meter_q(m.get("meter", "4/4")) or 4.0
        mk = structure.get("markup", {})
        sel = []
        for i, b in enumerate(ids):
            if mk.get(b, {}).get("meter"): q = _meter_q(mk[b]["meter"]) or q      # meter change from this bar on
            if keep is not None and b not in keep: continue
            bb = bars[b]; L = q
            if i == 0 and m.get("pickup") and bb["content_q"] > 0: L = min(q, bb["content_q"])
            model_bars[b] = dict(notes=[tuple(n) for n in bb["notes"]], len_q=L, content_q=bb["content_q"] or L)
            sel.append(b)
        if not sel: continue
        markup = {b: v for b, v in mk.items() if b in set(sel)}
        if keep is not None:                         # a passage take is played straight through
            markup = {b: {kk: vv for kk, vv in v.items() if kk == "meter"} for b, v in markup.items()}
            markup = {b: v for b, v in markup.items() if v}
        movements.append(dict(name=m.get("name") or f"{k + 1}", bars=sel, markup=markup))
    return dict(bars=model_bars, movements=movements)

def load(pid):
    d = store.piece_dir(pid)
    return store.read_json(d / "score.json"), store.read_json(d / "structure.json")

# ------------------------------------------------------------------ barlines found again (better detector)
START_KEYS = ("repeat_start", "segno", "coda", "meter")
END_KEYS = ("repeat_end", "fine", "dc", "ds", "to_coda", "repeat_to")

def courtesy_slivers(score):
    out = []
    for s in score["systems"]:
        xs = [s["x_left"]] + s["barlines"]; w = [xs[i + 1] - xs[i] for i in range(len(xs) - 1)]
        if len(w) < 3: continue
        med = float(np.median(w)); k = len(w) - 1
        if w[k] < 0.3 * med and w[k] < 0.045: out.append(bar_id(s["page"], s["idx"], k))
    return out

def bar_mapper(old, new):
    """find(bar_id, how) -> the bar of `new` that corresponds to a bar of `old` (how: start / end / mid)."""
    ob = {b["id"]: b for b in old["bars"]}; nids = {b["id"] for b in new["bars"]}
    by_sys = {}
    for b in new["bars"]: by_sys.setdefault((b["page"], b["system"]), []).append(b)
    def find(bid, how="mid"):
        b = ob.get(bid)
        if not b: return bid if bid in nids else None
        cands = by_sys.get((b["page"], b["system"]), [])
        if not cands: return None
        x0, _, x1, _ = b["box"]
        if how == "start": return min(cands, key=lambda c: abs(c["box"][0] - x0))["id"]
        if how == "end": return min(cands, key=lambda c: abs(c["box"][2] - x1))["id"]
        xm = (x0 + x1) / 2
        inside = [c for c in cands if c["box"][0] <= xm <= c["box"][2]]
        return (inside or [min(cands, key=lambda c: abs((c["box"][0] + c["box"][2]) / 2 - xm))])[0]["id"]
    return find

def remap_structure(old, new, st):
    """Carry the user's structure (movements, markup, deleted bars) from the bars of `old` to the bars of `new`
    (same pages and systems, different barlines): marks at the start of a bar follow its left edge, marks at the end
    follow its right edge, anything else follows the bar's middle."""
    find = bar_mapper(old, new); ob = {b["id"]: b for b in old["bars"]}
    out = dict(st); mk = {}
    for bid, marks in (st.get("markup") or {}).items():
        for k, v in marks.items():
            how = "start" if k in START_KEYS else "end" if k in END_KEYS else "mid"
            if k == "repeat_to": v = find(v, "start") or v
            nb = find(bid, how)
            if nb: mk.setdefault(nb, {})[k] = v
    out["markup"] = mk
    nbw = {b["id"]: b["box"][2] - b["box"][0] for b in new["bars"]}
    deleted = set()
    for bid in st.get("deleted") or []:
        nb = find(bid, "mid"); b = ob.get(bid)
        if nb and b and nbw[nb] < 1.5 * (b["box"][2] - b["box"][0]): deleted.add(nb)   # still a sliver
    deleted |= set(courtesy_slivers(new))
    deleted -= set(mk)                                  # a marked bar is a real bar
    out["deleted"] = sorted(deleted)
    out["movements"] = [dict(m, start=find(m["start"], "start") or m["start"]) for m in st.get("movements", [])]
    out["version"] = int(st.get("version", 1)) + 1
    return out
