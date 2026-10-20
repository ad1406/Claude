"""Find every statement of a motif in the MIDI: each hand is split into an upper line (highest note of each
onset), a lower line (lowest note) and, for wide chords, the full set; a motif is an interval pattern
(exact semitones or interval classes, so diatonic transpositions count), optionally with a rhythm profile.
Prime, inversion (and, where rhythm is given, augmentation/diminution) are recognised."""
import bisect, json, sys, warnings, numpy as np, pretty_midi as pm
warnings.filterwarnings("ignore")

def ic(i):
    """interval class with direction: 0 unison, 1 step (1-2), 2 third (3-4), 3 fourth, 4 tritone, 5 fifth, 6 sixth (8-9), 7 seventh (10-11), 8 octave+"""
    a = abs(i); c = 0 if a == 0 else 1 if a <= 2 else 2 if a <= 4 else 3 if a == 5 else 4 if a == 6 else 5 if a == 7 else 6 if a <= 9 else 7 if a <= 11 else 8
    return c if i >= 0 else -c

class Piece:
    def __init__(self, path, first_bar):
        m = pm.PrettyMIDI(path); self.db = list(m.get_downbeats()); self.beats = list(m.get_beats()); self.first = first_bar
        self.end = max(n.end for i in m.instruments for n in i.notes)
        self.hands = []
        for inst in m.instruments:
            ns = sorted(inst.notes, key=lambda n: (n.start, n.pitch)); groups = []
            for n in ns:
                if groups and n.start - groups[-1][0].start < 0.03: groups[-1].append(n)
                else: groups.append([n])
            self.hands.append(groups)
    def where(self, t):
        k = bisect.bisect_right(self.db, t + 1e-4) - 1; k = max(0, k)
        t0 = self.db[k]; t1 = self.db[k + 1] if k + 1 < len(self.db) else self.end
        nb = max(1, len([b for b in self.beats if t0 - 1e-4 <= b < t1 - 1e-4]))
        return k + self.first, round(1 + (t - t0) / ((t1 - t0) / nb), 2)
    def beatlen(self, t):
        j = max(0, bisect.bisect_right(self.beats, t + 1e-4) - 1)
        return (self.beats[j + 1] - self.beats[j]) if j + 1 < len(self.beats) else 0.5
    def lines(self, hand):
        g = self.hands[hand]
        return {"upper": [max(x, key=lambda n: n.pitch) for x in g], "lower": [min(x, key=lambda n: n.pitch) for x in g]}

def match(line, mot, piece):
    iv = mot["iv"]; L = (mot["n"] if iv is None else len(iv) + 1); exact = mot.get("exact", False); rh = mot.get("rhythm"); out = []
    p = np.array([n.pitch for n in line]); t = np.array([n.start for n in line])
    forms = [("prime", 1)] + ([("inversion", -1)] if mot.get("inv", True) else [])
    for i in range(len(line) - L + 1):
        d = np.diff(p[i:i + L])
        for name, s in (forms if iv is not None else [("rhythm", 1)]):
            if iv is not None:
                want = [s * x for x in iv]
                ok = all(a == b for a, b in zip(d, want)) if exact else all(ic(a) == ic(b) for a, b in zip(d, want))
                if not ok: continue
                q = "exact" if all(a == b for a, b in zip(d, want)) else "tonal"
            else: q = "rhythm"
            ioi = np.diff(t[i:i + L]); bl = piece.beatlen(t[i]); scale = None
            if rh:
                r = np.array(rh[:L - 1], float)
                if np.any(ioi <= 0.01): continue
                k = ioi / r; scale = float(np.median(k)) / bl
                if np.max(np.abs(np.log(k / np.median(k)))) > mot.get("rtol", 0.35): continue
                lo, hi = mot.get("scale", (0.45, 2.2))
                if not (lo <= scale <= hi): continue
            if mot.get("maxioi") and np.max(ioi) / bl > mot["maxioi"]: continue
            if mot.get("minioi") and np.min(ioi) / bl < mot["minioi"]: continue
            if mot.get("tailgap"):
                nx = t[i + L] if i + L < len(t) else 1e9
                if nx - t[i + L - 1] < mot["tailgap"] * max(ioi.max(), 1e-3): continue
            if mot.get("span") and t[i + L - 1] - t[i] > mot["span"] * bl: continue
            out.append(dict(i=i, form=name, q=q, t0=float(t[i]), t1=float(line[i + L - 1].end), pitch=pm.note_number_to_name(int(p[i])),
                            notes=[[float(line[j].start), float(line[j].end), int(p[j])] for j in range(i, i + L)],
                            aug=None if scale is None else ("augmented" if scale > 1.6 else "diminished" if scale < 0.6 else None)))
    return out

def find(piece, mot):
    hits = []
    for h in mot.get("hands", (0, 1)):
        for ln, line in piece.lines(h).items():
            if ln not in mot.get("lines", ("upper", "lower")): continue
            for x in match(line, mot, piece): x["hand"] = "RH" if h == 0 else "LH"; x["line"] = ln; hits.append(x)
    hits.sort(key=lambda x: (x["t0"], x["hand"]))
    keep = []
    for x in hits:          # one statement = one entry (upper/lower line of the same hand often coincide)
        if any(abs(x["t0"] - y["t0"]) < 0.05 and x["hand"] == y["hand"] for y in keep): continue
        if mot.get("gap") and keep and x["t0"] - keep[-1]["t0"] < mot["gap"] * piece.beatlen(x["t0"]) and x["hand"] == keep[-1]["hand"]: continue
        keep.append(x)
    for x in keep:
        x["bar"], x["beat"] = piece.where(x["t0"]); x["bar_end"], _ = piece.where(x["notes"][-1][0])
    return keep
