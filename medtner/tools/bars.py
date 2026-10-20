"""Per-bar note listing from the MIDI (for reading themes): python bars.py file.mid track a b [top|all]"""
import sys, warnings, pretty_midi as pm
warnings.filterwarnings("ignore")
def load(path):
    m = pm.PrettyMIDI(path); db = list(m.get_downbeats()); beats = list(m.get_beats())
    return m, db, beats
def bar_of(db, t):
    import bisect; return bisect.bisect_right(db, t + 1e-4)
if __name__ == "__main__":
    f, tr, a, b = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]); mode = sys.argv[5] if len(sys.argv) > 5 else "top"
    m, db, beats = load(f); import numpy as np
    for k in range(a, b + 1):
        t0 = db[k - 1]; t1 = db[k] if k < len(db) else 1e9
        ns = sorted([n for n in m.instruments[tr].notes if t0 - 1e-4 <= n.start < t1 - 1e-4], key=lambda n: (n.start, -n.pitch))
        bts = [x for x in beats if t0 - 1e-4 <= x < t1 - 1e-4]; spb = (t1 - t0) / max(1, len(bts))
        out = []; last = None
        for n in ns:
            if mode == "top":
                if last is not None and abs(n.start - last) < 0.02: continue
                last = n.start
            out.append(f"{(n.start - t0) / spb:.2f}:{pm.note_number_to_name(n.pitch)}")
        print(k, len(bts), " ".join(out))
