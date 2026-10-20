"""Practice: passages, the daily plan, cold tests (FSRS), the practice log and technique ranking.

Lifecycle: a passage is Working (practised daily, ordered by priority) until the user says it is ready;
then it is in Maintenance, where it only comes back as a cold test on the day FSRS schedules it.
A failed test (rating 1) sends it back to Working. Passages that share a Theme are rotated:
at most one of them is scheduled for work per day.
"""
import json, math, time
from datetime import datetime, timedelta, timezone, date
from starlette.responses import JSONResponse
from starlette.routing import Route
from fsrs import Scheduler, Card, Rating
from . import store, techniques, piecemodel

DAY_START_HOUR = 4          # a practice "day" runs 4am to 4am local time
TEST_MIN = 2.0              # minutes a cold test takes, for the plan
WORK_MIN = 10.0             # default minutes of work on a passage
FIELDS = ("name", "a", "b", "priority", "theme", "goal_tempo", "best_tempo", "minutes", "notes", "problem", "stage",
          "helper_state", "state", "archived", "intent", "diag", "bpm")
JSON_FIELDS = ("helper_state", "diag")
STAGES = ("learning", "shaping", "securing", "performing")
OLD_STAGE = {"new": "learning", "build": "shaping", "keep": "securing"}

def J(obj, status=200): return JSONResponse(obj, status_code=status)

# ------------------------------------------------------------------ time helpers
def day_key(t=None):
    t = time.time() if t is None else t
    return (datetime.fromtimestamp(t) - timedelta(hours=DAY_START_HOUR)).date().isoformat()

def day_end(t=None):
    """Timestamp of the next day boundary (4am local)."""
    t = time.time() if t is None else t
    d = datetime.fromtimestamp(t) - timedelta(hours=DAY_START_HOUR)
    nxt = datetime(d.year, d.month, d.day) + timedelta(days=1, hours=DAY_START_HOUR)
    return nxt.timestamp()

def days_until(iso, t=None):
    try: return (date.fromisoformat(iso) - date.fromisoformat(day_key(t))).days
    except Exception: return None

# ------------------------------------------------------------------ FSRS
def retention_for(piece):
    d = days_until(piece.get("perf_date")) if piece else None
    return 0.95 if d is not None and 0 <= d <= 45 else 0.9

def scheduler(retention):
    return Scheduler(desired_retention=retention, learning_steps=(), relearning_steps=(), enable_fuzzing=True)

def card_of(p):
    return Card.from_dict(json.loads(p["card"])) if p.get("card") else None

def retrievability(p, t=None):
    c = card_of(p)
    if c is None or c.last_review is None: return None
    now = datetime.fromtimestamp(t or time.time(), tz=timezone.utc)
    return float(scheduler(0.9).get_card_retrievability(c, current_datetime=now))

# ------------------------------------------------------------------ views
_LBL = {}
def _labeler(pid):
    """bar id -> 'm. 12' / 'II · 12' style labels for a piece (cached on the structure file's mtime)."""
    f = store.piece_dir(pid) / "structure.json"
    key = (pid, f.stat().st_mtime if f.exists() else 0)
    if key in _LBL: return _LBL[key]
    score, st = piecemodel.load(pid)
    if not score or not st: return None
    lab = piecemodel.labels(score, st); mv, _ = piecemodel.movement_of(score, st)
    names = [m.get("name") or "" for m in st["movements"]]
    def roman(k):
        import re; m = re.match(r"^([IVXLC]+)\b", names[k] or ""); return m.group(1) if m else str(k + 1)
    def rng(a, b):
        la, lb = lab.get(a, "?"), lab.get(b, "?")
        if len(names) > 1 and a in mv:
            if a == b: return f"{roman(mv[a])} · {la}"
            if mv.get(b) == mv[a]: return f"{roman(mv[a])} · {la}–{lb}"
            return f"{roman(mv[a])} · {la} – {roman(mv[b])} · {lb}" if b in mv else f"{roman(mv[a])} · {la}"
        return f"m. {la}" if a == b else f"m. {la}–{lb}"
    for k in [k for k in _LBL if k[0] == pid]: del _LBL[k]
    _LBL[key] = rng
    return rng

def _piece_map():
    return {p["id"]: p for p in store.q("SELECT id,title,composer,perf_date FROM pieces")}

def view(p, pieces=None, t=None):
    pieces = pieces or _piece_map(); pc = pieces.get(p["piece_id"]) or {}
    v = {k: p.get(k) for k in ("id", "piece_id", "a", "b", "name", "state", "priority", "theme", "goal_tempo", "best_tempo",
                               "minutes", "notes", "problem", "stage", "due", "last_worked", "last_tested", "sessions", "archived",
                               "created", "intent", "bpm")}
    v["helper_state"] = json.loads(p["helper_state"]) if p.get("helper_state") else {}
    v["diag"] = json.loads(p["diag"]) if p.get("diag") else {}
    v["problem"] = techniques.problem_id(p.get("problem"))
    v["stage"] = OLD_STAGE.get(p.get("stage"), p.get("stage"))
    v["piece_title"] = pc.get("title"); v["composer"] = pc.get("composer")
    try: rng = _labeler(p["piece_id"]); v["label"] = rng(p["a"], p["b"]) if rng else None
    except Exception: v["label"] = None
    v["retrievability"] = retrievability(p, t)
    v["worked_today"] = bool(p["last_worked"]) and day_key(p["last_worked"]) == day_key(t)
    v["tested_today"] = bool(p["last_tested"]) and day_key(p["last_tested"]) == day_key(t)
    v["auto_stage"] = "securing" if p["state"] == "maintenance" else ("learning" if (p["sessions"] or 0) < 3 else "shaping")
    return v

# ------------------------------------------------------------------ the plan for today
PRIORITY_W = {1: 1.0, 2: 1.6, 3: 2.4}

def plan(budget=None, t=None):
    t = t or time.time(); today = day_key(t); end = day_end(t)
    budget = budget if budget is not None else store.setting("budget", 40)
    pieces = _piece_map()
    ps = [p for p in store.q("SELECT * FROM passages WHERE archived=0") if p["piece_id"] in pieces]
    tests, works, done, rested = [], [], [], []
    for p in ps:
        v = view(p, pieces, t)
        if p["state"] == "maintenance":
            if v["tested_today"]: done.append(dict(v, kind="test")); continue
            if p["due"] is not None and p["due"] <= end:
                r = v["retrievability"]
                late = max(0, (date.fromisoformat(today) - date.fromisoformat(day_key(p["due"]))).days)
                reason = "cold test" + (f", {late} day{'s' if late != 1 else ''} overdue" if late else "")
                tests.append(dict(v, kind="test", reason=reason, est=TEST_MIN, _key=(r if r is not None else 0)))
        elif p["state"] == "working":
            if v["worked_today"]: done.append(dict(v, kind="work")); continue
            works.append(v)
    # theme rotation: one instance per (piece, theme) per day, least recently worked first
    groups = {}
    for v in works:
        k = (v["piece_id"], (v["theme"] or "").strip().lower()) if (v["theme"] or "").strip() else ("_", v["id"])
        groups.setdefault(k, []).append(v)
    theme_done = {(d["piece_id"], (d["theme"] or "").strip().lower()) for d in done if d["kind"] == "work" and (d["theme"] or "").strip()}
    chosen = []
    for k, vs in groups.items():
        if k[0] != "_" and k in theme_done:
            rested += [dict(v, reason=f"theme “{v['theme']}” already practised today") for v in vs]; continue
        vs.sort(key=lambda v: v["last_worked"] or 0)
        chosen.append(vs[0]); rested += [dict(v, reason=f"theme “{v['theme']}”: {vs[0]['name'] or vs[0]['label']} is today's turn") for v in vs[1:]]
    for v in chosen:
        pc = pieces.get(v["piece_id"]) or {}
        dp = days_until(pc.get("perf_date"), t)
        boost = 1 + 2 * math.exp(-dp / 21) if dp is not None and dp >= 0 else 1
        idle = (t - v["last_worked"]) / 86400 if v["last_worked"] else 3
        score = PRIORITY_W.get(v["priority"] or 2, 1.6) * boost * (1 + min(idle, 7) / 7)
        reasons = []
        if dp is not None and 0 <= dp <= 60: reasons.append(f"performance in {dp} day{'s' if dp != 1 else ''}")
        if not v["last_worked"]: reasons.append("new")
        elif idle >= 2: reasons.append(f"last worked {int(idle)} days ago")
        if (v["priority"] or 2) == 3: reasons.append("high priority")
        pk = (v.get("diag") or {}).get("park_t")
        if pk and 1 <= (date.fromisoformat(today) - date.fromisoformat(day_key(pk))).days <= 2:
            score *= 6; reasons.insert(0, "parked yesterday: start with it, cold")
        v.update(kind="work", reason=", ".join(reasons) or "daily work", est=v["minutes"] or WORK_MIN, _key=-score)
    tests.sort(key=lambda v: v["_key"]); chosen.sort(key=lambda v: v["_key"])
    items, total = [], 0.0
    for v in tests + chosen:                   # cold tests first: they must be cold
        total += v["est"]; v["in_budget"] = total <= budget + 1e-6 or not items; v.pop("_key", None); items.append(v)
    return {"day": today, "budget": budget, "total": round(total, 1), "items": items, "done": done, "rested": rested,
            "stats": stats(t), "suggestions": join_suggestions(ps, pieces, t)}

def _order(pid):
    score, st = piecemodel.load(pid)
    if not score or not st: return {}
    _, lists = piecemodel.movement_of(score, st)
    return {b: (mi, i) for mi, ids in enumerate(lists) for i, b in enumerate(ids)}

def join_suggestions(ps, pieces, t=None):
    """Neighbouring passages that are both secure (in cold tests, recall >= 0.85, tested at least twice) can be
    practised as one: suggest the join once, unless a passage already covers both."""
    out = []; by_piece = {}
    for p in ps: by_piece.setdefault(p["piece_id"], []).append(p)
    for pid, lst in by_piece.items():
        if len(lst) < 2: continue
        order = _order(pid)
        span = lambda p: (order.get(p["a"]), order.get(p["b"]))
        spans = [(p, *span(p)) for p in lst if None not in span(p)]
        def secure(p):
            if p["state"] != "maintenance": return False
            r = retrievability(p, t); c = card_of(p)
            return r is not None and r >= 0.85 and c is not None and (c.stability or 0) >= 3
        for p, a1, b1 in spans:
            for q, a2, b2 in spans:
                if p["id"] == q["id"] or a1[0] != a2[0] or b1[0] != a2[0] or a2[1] != b1[1] + 1: continue   # q starts right after p
                if not (secure(p) and secure(q)): continue
                if any(a3 <= a1 and b3 >= b2 for r, a3, b3 in spans if r["id"] not in (p["id"], q["id"])): continue
                lab = _labeler(pid)
                out.append(dict(kind="join", piece_id=pid, piece_title=(pieces.get(pid) or {}).get("title"), ids=[p["id"], q["id"]],
                                a=p["a"], b=q["b"], name=" + ".join((x["name"] or (lab(x["a"], x["b"]) if lab else "")) for x in (p, q)),
                                label=lab(p["a"], q["b"]) if lab else None))
    return out[:3]

def stats(t=None):
    """Minutes practised today and in the last 7 days, and the streak of consecutive days with practice."""
    t = t or time.time(); today = date.fromisoformat(day_key(t))
    per_day = {}
    for l in store.q("SELECT t, kind, minutes FROM practice_log WHERE t > ?", (t - 400 * 86400,)):
        m = l["minutes"] or (TEST_MIN if l["kind"] == "test" else 0)
        d = day_key(l["t"]); per_day[d] = per_day.get(d, 0) + m
    week = sum(v for d, v in per_day.items() if 0 <= (today - date.fromisoformat(d)).days < 7)
    streak, d = 0, today
    if today.isoformat() not in per_day: d = today - timedelta(days=1)       # today not started yet: count up to yesterday
    while d.isoformat() in per_day: streak += 1; d -= timedelta(days=1)
    return {"today": round(per_day.get(today.isoformat(), 0)), "week": round(week), "streak": streak}

# ------------------------------------------------------------------ technique ranking
def rank_techniques():
    """{problem or "problem/cause": {technique: {"score": 0..1, "n": uses}}}: a Beta(1,1) mean over experiment verdicts
    (better counts as helped, same as half, worse as not) and over the next cold test after a work session or
    experiment that used the technique (rating >= 3 counts as helped). Old problem ids are mapped to the new ones."""
    logs = store.q("SELECT * FROM practice_log ORDER BY t")
    stats, last_work = {}, {}
    def add(prob, cause, tech, ok):
        if not tech: return
        prob = techniques.problem_id(prob) or ""
        for key in ([prob, f"{prob}/{cause}"] if cause else [prob]):
            s = stats.setdefault(key, {}).setdefault(tech, [0.0, 0]); s[0] += ok; s[1] += 1
    for l in logs:
        if l["kind"] in ("work", "try"):
            if l["helped"] is not None: add(l["problem"], l.get("cause"), l["technique"], 1.0 if l["helped"] > 0 else 0.5 if l["helped"] == 0 else 0.0)
            if l["technique"]: last_work[l["passage_id"]] = l
        elif l["kind"] == "test" and l["rating"]:
            w = last_work.pop(l["passage_id"], None)
            if w: add(w["problem"], w.get("cause"), w["technique"], 1.0 if l["rating"] >= 3 else 0.0)
    return {p: {k: {"score": (s[0] + 1) / (s[1] + 2), "n": s[1]} for k, s in d.items()} for p, d in stats.items()}

# ------------------------------------------------------------------ routes
async def api_today(req):
    b = req.query_params.get("budget")
    if b: store.set_setting("budget", float(b))
    return J(plan(float(b) if b else None))

async def api_list(req):
    pid = int(req.path_params["pid"]); pieces = _piece_map()
    return J([view(p, pieces) for p in store.q("SELECT * FROM passages WHERE piece_id=? AND archived=0 ORDER BY created", (pid,))])

async def api_create(req):
    pid = int(req.path_params["pid"]); body = await req.json()
    if not body.get("a"): return J({"error": "no bars"}, 400)
    a, b = body["a"], body.get("b") or body["a"]
    i = store.ex("INSERT INTO passages(piece_id,a,b,name,state,priority,theme,created) VALUES(?,?,?,?,?,?,?,?)",
                 (pid, a, b, body.get("name") or "", "working", int(body.get("priority") or 2), body.get("theme"), store.now()))
    return J(view(store.q1("SELECT * FROM passages WHERE id=?", (i,))))

async def api_update(req):
    i = int(req.path_params["id"]); body = await req.json()
    p = store.q1("SELECT * FROM passages WHERE id=?", (i,))
    if not p: return J({"error": "no such passage"}, 404)
    for k in FIELDS:
        if k in body:
            v = body[k]
            if k in JSON_FIELDS: v = json.dumps(v) if v is not None else None
            if k == "stage" and v not in STAGES + (None,): continue
            store.ex(f"UPDATE passages SET {k}=? WHERE id=?", (v, i))
    return J(view(store.q1("SELECT * FROM passages WHERE id=?", (i,))))

async def api_delete(req):
    i = int(req.path_params["id"])
    store.ex("DELETE FROM practice_log WHERE passage_id=?", (i,)); store.ex("DELETE FROM passages WHERE id=?", (i,))
    store.ex("UPDATE recordings SET passage_id=NULL WHERE passage_id=?", (i,))
    return J({"ok": True})

def _apply(p, piece, e, upd):
    """Apply one log event e (a dict like a practice_log row) to the passage state in upd (in place)."""
    t = e["t"]; kind = e["kind"]
    if kind == "work":
        # automatic time logging sends several work events a day: a session is a day with work on it
        new_day = not upd.get("last_worked") or day_key(upd["last_worked"]) != day_key(t)
        upd.update(last_worked=t, sessions=(upd.get("sessions") or 0) + (1 if new_day else 0))
    elif kind == "test":
        rating = int(e.get("rating") or 0)
        if rating not in (1, 2, 3, 4): raise ValueError("rating must be 1-4")
        c = Card.from_dict(json.loads(upd["card"])) if upd.get("card") else Card()
        c, _ = scheduler(retention_for(piece)).review_card(c, Rating(rating), review_datetime=datetime.fromtimestamp(t, tz=timezone.utc))
        upd.update(card=json.dumps(c.to_dict()), due=c.due.timestamp(), last_tested=t)
        if rating == 1: upd["state"] = "working"
    elif kind == "graduate":
        upd["state"] = "maintenance"
        c = Card.from_dict(json.loads(upd["card"])) if upd.get("card") else None
        upd["due"] = max(day_end(t) + 60, c.due.timestamp() if c else 0)    # first cold test: tomorrow
    elif kind == "unlearn":
        upd["state"] = "working"
    elif kind in ("try", "stage", "note"):
        pass                                          # experiments and stage changes don't change the schedule
    else:
        raise ValueError("unknown kind")

LOG_COLS = ("passage_id", "piece_id", "t", "kind", "rating", "minutes", "technique", "problem", "tempo", "helped", "note",
            "take_id", "cause", "prediction", "stage")

def log_event(i, body, t=None):
    """Apply one practice event to passage i and record it. Returns the updated view."""
    t = t or store.now()
    p = store.q1("SELECT * FROM passages WHERE id=?", (i,))
    if not p: return None
    pieces = _piece_map(); piece = pieces.get(p["piece_id"])
    e = dict(body, t=t)
    upd = {k: p[k] for k in ("state", "card", "due", "last_tested", "last_worked", "sessions")}
    _apply(p, piece, e, upd)
    if body.get("kind") == "work":
        tempo = body.get("tempo")
        if tempo and (not p["best_tempo"] or tempo > p["best_tempo"]): upd["best_tempo"] = tempo
    if body.get("kind") == "stage" and body.get("note") in STAGES: upd["stage"] = body["note"]
    cur = OLD_STAGE.get(p.get("stage"), p.get("stage"))
    if body.get("kind") == "graduate" and cur in (None, "learning", "shaping"): upd["stage"] = "securing"
    if body.get("kind") == "test" and int(body.get("rating") or 0) == 1 and cur in ("securing", "performing"): upd["stage"] = "shaping"
    for k, v in upd.items(): store.ex(f"UPDATE passages SET {k}=? WHERE id=?", (v, i))
    row = dict(passage_id=i, piece_id=p["piece_id"], t=t, kind=body.get("kind"), rating=body.get("rating"), minutes=body.get("minutes"),
               technique=body.get("technique"), problem=body.get("problem") or p["problem"], tempo=body.get("tempo"),
               helped=body.get("helped"), note=body.get("note"), take_id=body.get("take_id"), cause=body.get("cause"),
               prediction=body.get("prediction"), stage=OLD_STAGE.get(p.get("stage"), p.get("stage")))
    store.ex(f"INSERT INTO practice_log({','.join(LOG_COLS)}) VALUES({','.join('?' * len(LOG_COLS))})", tuple(row[k] for k in LOG_COLS))
    if body.get("kind") == "test" and body.get("take_id"):
        store.ex("UPDATE recordings SET rating=?, label='cold' WHERE id=?", (body.get("rating"), body["take_id"]))
    return view(store.q1("SELECT * FROM passages WHERE id=?", (i,)), pieces)

def replay(i):
    """Rebuild a passage's schedule (state, FSRS card, due date, sessions) from its remaining log, in order: used
    after deleting an event, so the result is as if it never happened."""
    p = store.q1("SELECT * FROM passages WHERE id=?", (i,))
    if not p: return None
    piece = _piece_map().get(p["piece_id"])
    upd = dict(state="working", card=None, due=None, last_tested=None, last_worked=None, sessions=0)
    for e in store.q("SELECT * FROM practice_log WHERE passage_id=? ORDER BY t, id", (i,)):
        try: _apply(p, piece, e, upd)
        except ValueError: pass
    for k, v in upd.items(): store.ex(f"UPDATE passages SET {k}=? WHERE id=?", (v, i))
    return view(store.q1("SELECT * FROM passages WHERE id=?", (i,)))

async def api_delete_log(req):
    """Delete one history entry (a false cold test, a mis-logged session) and replay the rest."""
    lid = int(req.path_params["lid"]); l = store.q1("SELECT * FROM practice_log WHERE id=?", (lid,))
    if not l: return J({"error": "no such entry"}, 404)
    store.ex("DELETE FROM practice_log WHERE id=?", (lid,))
    if l["kind"] == "test" and l.get("take_id"): store.ex("UPDATE recordings SET rating=NULL WHERE id=?", (l["take_id"],))
    return J(replay(l["passage_id"]) or {"ok": True})

async def api_log(req):
    i = int(req.path_params["id"]); body = await req.json()
    try: v = log_event(i, body)
    except ValueError as e: return J({"error": str(e)}, 400)
    return J(v) if v else J({"error": "no such passage"}, 404)

async def api_history(req):
    i = int(req.path_params["id"])
    return J(store.q("SELECT * FROM practice_log WHERE passage_id=? ORDER BY t DESC LIMIT 200", (i,)))

async def api_techniques(req):
    return J(dict(techniques.load(), ranks=rank_techniques()))

SECRET = ("deepseek_key",)

async def api_settings(req):
    if req.method == "PUT":
        for k, v in (await req.json()).items():
            if k in SECRET and v == "": store.ex("DELETE FROM settings WHERE key=?", (k,))
            else: store.set_setting(k, v)
    out = {r["key"]: json.loads(r["value"]) for r in store.q("SELECT key,value FROM settings")}
    for k in SECRET:                               # never sent back to the page
        if k in out: out[k] = "set"
    return J(out)

ROUTES = [
    Route("/api/practice/today", api_today),
    Route("/api/practice/techniques", api_techniques),
    Route("/api/settings", api_settings, methods=["GET", "PUT"]),
    Route("/api/pieces/{pid:int}/passages", api_list),
    Route("/api/pieces/{pid:int}/passages", api_create, methods=["POST"]),
    Route("/api/passages/{id:int}", api_update, methods=["PATCH"]),
    Route("/api/passages/{id:int}", api_delete, methods=["DELETE"]),
    Route("/api/passages/{id:int}/log", api_log, methods=["POST"]),
    Route("/api/passages/{id:int}/history", api_history),
    Route("/api/log/{lid:int}", api_delete_log, methods=["DELETE"]),
]
