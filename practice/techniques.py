"""The technique library, built from the practice taxonomy (taxonomy.py): problems -> causes -> techniques,
the quick tests that tell causes apart, and every technique's how / listen-for / evidence grade / origin.

Alex's own 20 techniques are part of it (marked `yours`, wording kept, helpers kept).
DATA_DIR/techniques.json can override or add entries by id: {"techniques": [...], "problems": [...]}.
Old problem ids (from the first technique list) are mapped to the new problems, so history keeps counting.
"""
import copy, json
from . import DATA_DIR, taxonomy

# problem ids used before the taxonomy -> the closest new problem
OLD_PROBLEMS = {"notes": "notes", "fingering": "wrongnote", "uneven": "uneven", "speed": "speed", "jumps": "leaps",
                "memory": "slip", "seams": "hitch", "coordination": "coordination", "sound": "voicing",
                "tension": "tension", "pedal": "pedal"}

def problem_id(pid):
    """A problem id as stored (possibly an old one) -> the current id."""
    if not pid: return pid
    ids = {p["id"] for p in taxonomy.PROBLEMS}
    return pid if pid in ids else OLD_PROBLEMS.get(pid, pid)

def load():
    """The whole library: {groups, families, problems (with causes), tests, techniques}, overrides merged in."""
    probs = copy.deepcopy(taxonomy.PROBLEMS); techs = copy.deepcopy(taxonomy.TECHNIQUES)
    f = DATA_DIR / "techniques.json"
    if f.exists():
        try:
            extra = json.loads(f.read_text(encoding="utf-8"))
            for key, lst in (("problems", probs), ("techniques", techs)):
                for item in extra.get(key, []):
                    i = next((k for k, x in enumerate(lst) if x["id"] == item.get("id")), None)
                    if i is None: lst.append(item)
                    else: lst[i] = {**lst[i], **item}
        except Exception:
            pass
    # which problems each technique is suggested for (for browsing by technique)
    use = {}
    for p in probs:
        for c in p.get("causes", []):
            for t in c.get("techniques", []): use.setdefault(t, []).append(p["id"])
    for t in techs:
        t["problems"] = sorted(set(use.get(t["id"], []) + t.get("problems", [])))
    return dict(groups=[dict(id=g, name=n) for g, n in taxonomy.GROUPS],
                families=[dict(id=g, name=n) for g, n in taxonomy.FAMILIES],
                problems=probs, techniques=techs,
                tests=[dict(id=t[0], name=t[1], clean=t[2], fails=t[3]) for t in taxonomy.TESTS])
