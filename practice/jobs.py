"""A tiny persistent job queue (SQLite). The server enqueues; a separate worker process runs jobs one at a time."""
import json
from . import store

ORDER = {"score": 0, "redetect": 0, "addline": 0, "check": 1, "download": 1, "features": 2, "align": 3, "consensus": 4}

def enqueue(kind, piece_id=None, recording_id=None, params=None, dedupe=True):
    if dedupe:
        ex = store.q1("SELECT id FROM jobs WHERE kind=? AND IFNULL(piece_id,-1)=IFNULL(?,-1) AND IFNULL(recording_id,-1)=IFNULL(?,-1) AND status='queued'",
                      (kind, piece_id, recording_id))
        if ex: return ex["id"]
    return store.ex("INSERT INTO jobs(kind,piece_id,recording_id,params,created) VALUES(?,?,?,?,?)",
                    (kind, piece_id, recording_id, json.dumps(params or {}), store.now()))

def claim():
    rows = store.q("SELECT * FROM jobs WHERE status='queued' ORDER BY created")
    if not rows: return None
    rows.sort(key=lambda r: (ORDER.get(r["kind"], 9), r["created"]))
    j = rows[0]
    store.ex("UPDATE jobs SET status='running', started=?, progress=0 WHERE id=?", (store.now(), j["id"]))
    j["params"] = json.loads(j["params"] or "{}"); return j

def progress(jid, frac, msg=""):
    store.ex("UPDATE jobs SET progress=?, message=? WHERE id=?", (float(frac), msg, jid))

def done(jid): store.ex("UPDATE jobs SET status='done', progress=1, finished=? WHERE id=?", (store.now(), jid))
def fail(jid, err): store.ex("UPDATE jobs SET status='failed', error=?, finished=? WHERE id=?", (str(err)[-2000:], store.now(), jid))

def active():
    return store.q("SELECT id,kind,piece_id,recording_id,status,progress,message,error FROM jobs WHERE status IN ('queued','running') "
                   "OR (status='failed' AND finished>?) ORDER BY created", (store.now() - 600,))

def reset_stale():
    store.ex("UPDATE jobs SET status='queued' WHERE status='running'")
