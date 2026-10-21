"""Approximate motif matching over a hierarchy of lines.

Lines (each a time-ordered list of note ids):
  * surface lines: for every voice (MusicXML layer), every hand (all its layers), and the whole texture: the top and the
    bottom note of each onset;
  * reduced lines: the same sources, reduced per window (half beat, beat, two beats, bar): in each window the highest
    (top lines) or lowest (bottom lines) note that starts in it. Embellishments (neighbour notes, octave displacements,
    repeated notes, arpeggiated filling) drop out, structural tones remain, e.g. b. 496: the bass A-G#-G-F#-F-E, each tone
    decorated by its lower neighbour an octave above, becomes a plain chromatic descent at beat level;
  * optional repeat-collapsing (a held or re-struck structural tone counts once).

Matching: a motif is a sequence of intervals (semitones) with an optional relative rhythm. A gapped dynamic program aligns
it to every line: the motif's notes must be a subsequence of the line, up to `gap` skipped notes between two motif notes
(interpolated embellishment), so ornamented statements match. Each interval is compared under a pitch mode:
  exact    same interval
  tonal    same direction and size class, at most 1 semitone different (diatonic/chromatic variants, modal change)
  octave   octave-equivalent: intervals compared mod 12 (register transfer, octave displacement of single notes)
  contour  same direction and same step/skip/leap class (free variation)
and under transformations: prime, inversion, retrograde, retrograde inversion. Rhythm (if given) must be proportional to the
motif's rhythm up to a tolerance, at any uniform scale (augmentation/diminution), or is ignored.
Every statement records its notes, line, level, mode, transformation and rhythmic scale; overlapping statements of the same
motif are merged, keeping the most literal description and the union of evidence."""
import json, os, math, bisect, collections
import numpy as np
from numba import njit
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INF = 1e9

def size_class(i):
    a = abs(i)
    return 0 if a == 0 else 1 if a <= 2 else 2 if a <= 4 else 3 if a <= 7 else 4

@njit(cache=True)
def _icost(d, w, mode):
    # d: line interval, w: wanted interval
    if mode == 0:                                   # exact
        return 0.0 if d == w else INF
    if mode == 3:                                   # octave-equivalent
        a = ((d % 12) + 12) % 12; b = ((w % 12) + 12) % 12
        return 0.0 if a == b else INF
    sd = 0 if d == 0 else (1 if d > 0 else -1); sw = 0 if w == 0 else (1 if w > 0 else -1)
    if sd != sw: return INF
    ad = abs(d); aw = abs(w)
    if mode == 1:                                   # tonal: +-1 semitone within the same size class
        if abs(ad - aw) > 1: return INF
        cd = 0 if ad == 0 else (1 if ad <= 2 else (2 if ad <= 4 else (3 if ad <= 7 else 4)))
        cw = 0 if aw == 0 else (1 if aw <= 2 else (2 if aw <= 4 else (3 if aw <= 7 else 4)))
        if cd != cw: return INF
        return 0.35 * abs(ad - aw)
    # contour: step / skip / leap classes
    cd = 0 if ad == 0 else (1 if ad <= 2 else (2 if ad <= 4 else (3 if ad <= 7 else 4)))
    cw = 0 if aw == 0 else (1 if aw <= 2 else (2 if aw <= 4 else (3 if aw <= 7 else 4)))
    if cd != cw: return INF
    return 0.2 * abs(ad - aw)

@njit(cache=True)
def _search(P, T, iv, rh, use_rh, mode, gap, rtol, smin, smax, maxcost, fold, out_idx, out_cost, out_scale):
    """For each start i, best alignment of the motif (len(iv)+1 notes) as a subsequence with <= gap skips between
    consecutive motif notes. Writes the chosen indices to out_idx[i], cost to out_cost[i] (INF if none)."""
    n = len(P); L = len(iv) + 1
    for i in range(n):
        out_cost[i] = INF
        # DP over (k, j): best cost to place motif note k at line index j
        W = (gap + 1) * (L - 1) + 1
        best = np.full((L, W), INF)
        back = np.full((L, W), -1, np.int64)
        best[0, 0] = 0.0
        for k in range(1, L):
            for jj in range(W):
                j = i + jj
                if j >= n: break
                if best[k - 1, jj] >= INF: continue
                for g in range(gap + 1):
                    j2 = j + 1 + g; jj2 = j2 - i
                    if j2 >= n or jj2 >= W: break
                    d = P[j2] - P[j]
                    if fold and d != 0: d = ((d + 5) % 12) - 5 if d > 0 else -(((-d + 5) % 12) - 5)   # octave displacement
                    c = _icost(d, iv[k - 1], mode)
                    if c >= INF: continue
                    c += 0.25 * g
                    v = best[k - 1, jj] + c
                    if v < best[k, jj2]:
                        best[k, jj2] = v; back[k, jj2] = jj
        bj = -1; bc = INF
        for jj in range(W):
            if best[L - 1, jj] < bc: bc = best[L - 1, jj]; bj = jj
        if bj < 0 or bc > maxcost: continue
        idx = np.empty(L, np.int64); jj = bj
        for k in range(L - 1, -1, -1):
            idx[k] = i + jj; jj = back[k, jj] if k > 0 else 0
        scale = 1.0
        if use_rh:
            # rhythm: inter-onset intervals proportional to rh at one scale
            lr = np.empty(L - 1)
            ok = True
            for k in range(L - 1):
                dt = T[idx[k + 1]] - T[idx[k]]
                if dt <= 1e-6 or rh[k] <= 0: ok = False; break
                lr[k] = math.log(dt / rh[k])
            if not ok: continue
            m = np.median(lr); dev = 0.0
            for k in range(L - 1): dev = max(dev, abs(lr[k] - m))
            if dev > rtol: continue
            scale = math.exp(m)
            if scale < smin or scale > smax: continue
            bc += dev
        out_cost[i] = bc; out_scale[i] = scale
        for k in range(L): out_idx[i, k] = idx[k]

class Corpus:
    def __init__(self):
        D = json.load(open(os.path.join(ROOT, "build", "notes.json")))
        byid0 = {n["id"]: n for n in D["notes"]}
        nxt = {n["tie"]: n for n in D["notes"] if n["tie"] and n["tie"] in byid0 and byid0[n["tie"]]["pitch"] == n["pitch"]
               and abs(byid0[n["tie"]]["q1"] - n["q0"]) < 0.02}     # only real continuations (unclosed ties in the MusicXML)
        for n in D["notes"]:                        # sounding end through ties
            e = n
            while e["id"] in nxt: e = nxt[e["id"]]
            n["q_end"] = e["q1"]
        self.notes = [n for n in D["notes"] if not n["tie"]]
        self.by_id = {n["id"]: n for n in self.notes}
        self.measures = D["measures"]
        self.mq = [m["q"] for m in self.measures] + [max(n["q1"] for n in D["notes"])]
        self.beat = self._beats()
        self.lines = self._lines()

    def _beats(self):
        """Beat length (quarters) per bar from the bar's length: compound bars (15/8, 18/8, 9/8...) beat in dotted quarters."""
        out = []
        for k in range(len(self.measures)):
            L = self.mq[k + 1] - self.mq[k]
            out.append(1.5 if abs(L / 1.5 - round(L / 1.5)) < 1e-3 and round(L / 1.5) in (3, 4, 5, 6) and abs(L - round(L)) > 1e-3 or
                       (abs(L - 7.5) < 1e-3 or abs(L - 9.0) < 1e-3) else 1.0)
        return out

    def window(self, n, level):
        """Index of the reduction window a note falls in: level = number of beats per window (0.5, 1, 2, 'bar')."""
        b = n["bar"] - 1; q0 = self.mq[b]
        if level == "bar": return (b, 0)
        w = self.beat[b] * level
        return (b, int(math.floor((n["q0"] - q0) / w + 1e-6)))

    def _lines(self):
        hand = lambda n: 1 if n["layer"] <= 4 else 2
        sources = collections.defaultdict(list)
        for n in self.notes:
            if n["grace"]: continue
            sources[("v", hand(n), n["layer"])].append(n)
            sources[("h", hand(n), 0)].append(n)
            sources[("all", 0, 0)].append(n)
        lines = []
        for src, ns in sources.items():
            ns = sorted(ns, key=lambda n: (n["q0"], n["pitch"]))
            groups = []
            for n in ns:
                if groups and abs(groups[-1][0]["q0"] - n["q0"]) < 1e-4: groups[-1].append(n)
                else: groups.append([n])
            for side in ("top", "bottom"):
                pick = (lambda g: max(g, key=lambda n: n["pitch"])) if side == "top" else (lambda g: min(g, key=lambda n: n["pitch"]))
                surf = [pick(g) for g in groups]
                lines.append(dict(src=src, side=side, level="surface", notes=surf))
                for level in (0.5, 1, 2, "bar"):
                    win = collections.OrderedDict()
                    for n in surf:
                        k = self.window(n, level)
                        if k not in win: win[k] = n
                        else:
                            cur = win[k]
                            if (side == "top" and n["pitch"] > cur["pitch"]) or (side == "bottom" and n["pitch"] < cur["pitch"]): win[k] = n
                    lines.append(dict(src=src, side=side, level=level, notes=list(win.values())))
        for ln in lines:
            ln["P"] = np.array([n["pitch"] for n in ln["notes"]], np.int64)
            ln["T"] = np.array([n["q0"] for n in ln["notes"]], np.float64)
            # collapsed variant: repeated pitches merged (structural tone re-struck or held)
            keep = [0] + [i for i in range(1, len(ln["notes"])) if ln["P"][i] != ln["P"][i - 1]] if len(ln["notes"]) else []
            ln["keep"] = np.array(keep, np.int64)
            ln["run_end"] = {k: (keep[j + 1] - 1 if j + 1 < len(keep) else len(ln["notes"]) - 1) for j, k in enumerate(keep)}
        return lines

MODES = {"exact": 0, "tonal": 1, "contour": 2, "octave": 3}

def transforms(iv, rh, which):
    out = []
    for t in which:
        if t == "P": out.append(("prime", list(iv), list(rh) if rh else None))
        if t == "I": out.append(("inversion", [-x for x in iv], list(rh) if rh else None))
        if t == "R": out.append(("retrograde", [-x for x in iv[::-1]], list(rh[::-1]) if rh else None))
        if t == "RI": out.append(("retrograde inversion", list(iv[::-1]), list(rh[::-1]) if rh else None))
    return out

def find(C, spec):
    if isinstance(spec, list):
        hits = []
        for k, sp in enumerate(spec):
            for h in find(C, sp): h["variant"] = k; hits.append(h)
        hits.sort(key=lambda h: h["q0"]); return hits
    return _find(C, spec)

def _find(C, spec):
    """spec: iv, rhythm (optional), modes, transforms, levels, sides, sources ('v','h','all'), gap, collapse, rtol,
    scale (min,max relative to the beat), maxcost, beatmin/beatmax (allowed spacing in beats), minspan/maxspan (beats)."""
    hits = []
    rtol = spec.get("rtol", 0.35); smin, smax = spec.get("scale", (0.05, 50)); maxcost = spec.get("maxcost", 1.5)
    for ln in C.lines:
        if ln["level"] not in spec.get("levels", ["surface"]) or ln["side"] not in spec.get("sides", ["top", "bottom"]): continue
        if ln["src"][0] not in spec.get("sources", ["v", "h"]): continue
        if spec.get("hands") and ln["src"][1] not in spec["hands"] and ln["src"][0] != "all": continue
        for collapse in spec.get("collapse", [False]):
            sel = ln["keep"] if collapse else np.arange(len(ln["notes"]))
            if len(sel) < len(spec["iv"]) + 1: continue
            P = ln["P"][sel]; T = ln["T"][sel]
            for tname, iv, rh in transforms(spec["iv"], spec.get("rhythm"), spec.get("transforms", ["P"])):
                for mname in spec.get("modes", ["exact"]):
                    L = len(iv) + 1; n = len(P)
                    oi = np.zeros((n, L), np.int64); oc = np.zeros(n); os_ = np.ones(n)
                    _search(P, T, np.array(iv, np.int64), np.array(rh if rh else [1.0] * (L - 1), np.float64), bool(rh), MODES[mname],
                            spec.get("gap", 0), rtol, 0.0, 1e9, maxcost, bool(spec.get("fold", False)), oi, oc, os_)
                    for i in np.where(oc < INF)[0]:
                        ids = [ln["notes"][sel[j]]["id"] for j in oi[i]]
                        runs = None
                        if collapse:   # every note of each held/re-struck structural tone belongs to the statement
                            runs = [[n["id"] for n in ln["notes"][sel[j]:ln["run_end"][sel[j]] + 1]] for j in oi[i]]
                        hits.append(dict(ids=ids, runs=runs, cost=float(oc[i]), mode=mname, form=tname, line=f"{ln['src'][0]}{ln['src'][1]}.{ln['src'][2]}",
                                         side=ln["side"], level=str(ln["level"]), collapse=collapse, rscale=float(os_[i])))
    if spec.get("chain"):
        global CHAIN_OVERLAP; CHAIN_OVERLAP = spec.get("chain_overlap", 99)
        hits = chain([h for h in hits if gaps_ok(C, spec, h)])     # a run never bridges a gap a single statement could not
    return postfilter(C, spec, hits)

CHAIN_OVERLAP = 99
def chain(hits, overlap=None):
    """Overlapping statements on the same line (same mode/level/transform) that continue each other merge into one run."""
    groups = collections.defaultdict(list)
    for h in hits: groups[(h["line"], h["side"], h["level"], h["collapse"], h["mode"], h["form"])].append(h)
    out = []
    for g in groups.values():
        g.sort(key=lambda h: h["ids"][0]); cur = None
        for h in g:
            if cur is not None and len(set(h["ids"]) & set(cur["ids"])) >= max(1, min(len(h["ids"]) - 1, CHAIN_OVERLAP)):
                new = [k for k, i in enumerate(h["ids"]) if i not in cur["ids"]]
                cur["ids"] = cur["ids"] + [h["ids"][k] for k in new]; cur["cost"] = min(cur["cost"], h["cost"])
                if cur.get("runs") is not None and h.get("runs") is not None: cur["runs"] = cur["runs"] + [h["runs"][k] for k in new]
            else:
                if cur: out.append(cur)
                cur = dict(h)
        if cur: out.append(cur)
    return out

RANK = {"exact": 0, "tonal": 1, "octave": 2, "contour": 3}
LEVEL_RANK = {"surface": 0, "0.5": 1, "1": 2, "2": 3, "bar": 4}

def gaps_ok(C, spec, h):
    ns = [C.by_id[i] for i in h["ids"]]; beat = C.beat[ns[0]["bar"] - 1]
    lasts = [C.by_id[r[-1]] for r in h["runs"]] if h.get("runs") else ns
    dts = [(ns[k + 1]["q0"] - lasts[k]["q0"]) / beat for k in range(len(ns) - 1)]
    return not (spec.get("beatmax") is not None and max(dts) > spec["beatmax"] + 1e-6)

def postfilter(C, spec, hits):
    out = []
    for h in hits:
        ns = [C.by_id[i] for i in h["ids"]]
        b0 = ns[0]["bar"]; beat = C.beat[b0 - 1]
        lasts = [C.by_id[r[-1]] for r in h["runs"]] if h.get("runs") else ns
        dts = [(ns[k + 1]["q0"] - lasts[k]["q0"]) / beat for k in range(len(ns) - 1)]
        if h.get("runs"):     # the statement starts at the last re-strike of its first tone
            h["runs"][0] = h["runs"][0][-1:]; h["ids"][0] = h["runs"][0][0]; ns[0] = C.by_id[h["ids"][0]]
        if spec.get("beatmin") is not None and min(dts) < spec["beatmin"] - 1e-6: continue
        if spec.get("beatmax") is not None and max(dts) > spec["beatmax"] + 1e-6: continue
        span = (ns[-1]["q0"] - ns[0]["q0"]) / beat
        if spec.get("maxspan") is not None and span > spec["maxspan"]: continue
        if spec.get("minspan") is not None and span < spec["minspan"]: continue
        if spec.get("rhythm") and h["rscale"]:
            sc = h["rscale"] / beat
            lo, hi = spec.get("scale", (0.05, 50))
            if not (lo <= sc <= hi): continue
            h["aug"] = "augmented" if sc > spec.get("aug_at", 1.6) * spec.get("base", 1.0) else ("diminished" if sc < spec.get("dim_at", 0.62) * spec.get("base", 1.0) else None)
        if spec.get("filter") and not spec["filter"](C, ns, h): continue
        h["q0"] = ns[0]["q0"]; h["q1"] = max(n["q1"] for n in ns); h["bar"] = b0; h["bar_end"] = ns[-1]["bar"]
        h["pitches"] = [n["pitch"] for n in ns]; h["hand"] = sorted({1 if n["layer"] <= 4 else 2 for n in ns})
        out.append(h)
    # merge statements sharing notes: keep the most literal (exact < tonal < octave < contour; surface before reductions)
    out.sort(key=lambda h: (RANK[h["mode"]], h["form"] != "prime", LEVEL_RANK[h["level"]], h["cost"]))
    kept = []; owner = {}
    for h in out:
        s = set(h["ids"]); clash = None
        for i in s:
            if i in owner: clash = owner[i]; break
        if clash is not None:
            k = kept[clash]; inter = len(s & set(k["ids"]))
            if inter >= max(2, min(len(s), len(k["ids"])) // 2): k.setdefault("also", set()).add((h["mode"], h["form"], h["level"])); continue
        kept.append(h)
        for i in s: owner.setdefault(i, len(kept) - 1)
    for k in kept: k["also"] = sorted(k.get("also", []))
    kept.sort(key=lambda h: h["q0"])
    return kept
