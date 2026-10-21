"""Print each voice (staff/layer) of bars a..b: beat:pitch(dur) with chords as a+b+c (top first)."""
import json, os, sys
import pretty_midi as pm
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = json.load(open(os.path.join(ROOT, "build", "notes.json"))); M = D["measures"]
nm = lambda p: pm.note_number_to_name(p).replace("#", "♯")
def show(a, b):
    for bar in range(a, b + 1):
        q0 = M[bar - 1]["q"]; ns = [n for n in D["notes"] if n["bar"] == bar]
        print(f"— bar {bar}")
        for key in sorted({(n["layer"], n["staff"]) for n in ns}):
            v = sorted([n for n in ns if (n["layer"], n["staff"]) == key], key=lambda n: (n["q0"], -n["pitch"]))
            out = []; i = 0
            while i < len(v):
                j = i
                while j < len(v) and abs(v[j]["q0"] - v[i]["q0"]) < 1e-3: j += 1
                ch = v[i:j]; d = round(ch[0]["q1"] - ch[0]["q0"], 2)
                tie = "~" if ch[0]["tie"] else ""; g = "g" if ch[0]["grace"] else ""
                if COMPACT: out.append(f"{round(ch[0]['q0'] - q0, 2):g}:{tie}{g}{nm(ch[0]['pitch'])}" + (f"/{nm(ch[-1]['pitch'])}" if len(ch) > 1 else ""))
                else: out.append(f"{round(ch[0]['q0'] - q0, 2)}:{tie}{g}{'+'.join(nm(n['pitch']) for n in ch)}({d})")
                i = j
            print(f"  L{key[0]}/s{key[1]}: " + "  ".join(out))
COMPACT = "-c" in sys.argv
if __name__ == "__main__":
    sys.argv = [a for a in sys.argv if a != "-c"]
    show(int(sys.argv[1]), int(sys.argv[2]))
