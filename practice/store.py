"""Where everything lives. Index tables in SQLite; bulky per-piece / per-recording documents as JSON files."""
import json, sqlite3, time, threading, shutil
from pathlib import Path
from . import DATA_DIR

DB = DATA_DIR / "library.sqlite"
_local = threading.local()

SCHEMA = """
CREATE TABLE IF NOT EXISTS pieces(id INTEGER PRIMARY KEY, title TEXT, composer TEXT, pdf TEXT, npages INTEGER DEFAULT 0,
  status TEXT DEFAULT 'new', message TEXT, created REAL, updated REAL);
CREATE TABLE IF NOT EXISTS recordings(id INTEGER PRIMARY KEY, piece_id INTEGER, name TEXT, kind TEXT DEFAULT 'reference',
  source TEXT, url TEXT, path TEXT, duration REAL, range_start REAL, range_end REAL, status TEXT DEFAULT 'new', message TEXT,
  created REAL, sort INTEGER DEFAULT 0);
CREATE TABLE IF NOT EXISTS jobs(id INTEGER PRIMARY KEY, kind TEXT, piece_id INTEGER, recording_id INTEGER, params TEXT,
  status TEXT DEFAULT 'queued', progress REAL DEFAULT 0, message TEXT, created REAL, started REAL, finished REAL, error TEXT);
CREATE TABLE IF NOT EXISTS passages(id INTEGER PRIMARY KEY, piece_id INTEGER, a TEXT, b TEXT, name TEXT,
  state TEXT DEFAULT 'working', priority INTEGER DEFAULT 2, theme TEXT, goal_tempo REAL, best_tempo REAL, minutes REAL,
  notes TEXT, problem TEXT, stage TEXT, helper_state TEXT, card TEXT, due REAL, last_worked REAL, last_tested REAL,
  sessions INTEGER DEFAULT 0, created REAL, archived INTEGER DEFAULT 0);
CREATE TABLE IF NOT EXISTS practice_log(id INTEGER PRIMARY KEY, passage_id INTEGER, piece_id INTEGER, t REAL, kind TEXT,
  rating INTEGER, minutes REAL, technique TEXT, problem TEXT, tempo REAL, helped INTEGER, note TEXT);
CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY, value TEXT);
CREATE TABLE IF NOT EXISTS flags(id INTEGER PRIMARY KEY, piece_id INTEGER, recording_id INTEGER, t REAL, bar TEXT, pass INTEGER,
  note TEXT, done INTEGER DEFAULT 0, created REAL);
CREATE TABLE IF NOT EXISTS cues(id INTEGER PRIMARY KEY, piece_id INTEGER, bar TEXT, kind TEXT, text TEXT, created REAL);
"""

MIGRATIONS = ["ALTER TABLE pieces ADD COLUMN page_from INTEGER", "ALTER TABLE pieces ADD COLUMN page_to INTEGER",
              "ALTER TABLE pieces ADD COLUMN perf_date TEXT",
              # musicality-first practice: intent, diagnosis, the metronome's last tempo per passage
              "ALTER TABLE passages ADD COLUMN intent TEXT", "ALTER TABLE passages ADD COLUMN diag TEXT",
              "ALTER TABLE passages ADD COLUMN bpm REAL",
              # experiments and takes in the log
              "ALTER TABLE practice_log ADD COLUMN take_id INTEGER", "ALTER TABLE practice_log ADD COLUMN cause TEXT",
              "ALTER TABLE practice_log ADD COLUMN prediction TEXT", "ALTER TABLE practice_log ADD COLUMN stage TEXT",
              # takes tied to passages, with labels
              "ALTER TABLE recordings ADD COLUMN passage_id INTEGER", "ALTER TABLE recordings ADD COLUMN label TEXT",
              "ALTER TABLE recordings ADD COLUMN rating INTEGER", "ALTER TABLE recordings ADD COLUMN piano TEXT",
              "ALTER TABLE recordings ADD COLUMN technique TEXT", "ALTER TABLE recordings ADD COLUMN rerating INTEGER",
              # a take of one section: the bars it covers (it is lined up with those bars only)
              "ALTER TABLE recordings ADD COLUMN sec_a TEXT", "ALTER TABLE recordings ADD COLUMN sec_b TEXT",
              # typed marks
              "ALTER TABLE flags ADD COLUMN kind TEXT", "ALTER TABLE flags ADD COLUMN frac REAL"]

def db():
    c = getattr(_local, "c", None)
    if c is None:
        DATA_DIR.mkdir(parents=True, exist_ok=True)
        c = sqlite3.connect(str(DB), timeout=30, check_same_thread=False)
        c.row_factory = sqlite3.Row
        c.execute("PRAGMA journal_mode=WAL"); c.executescript(SCHEMA)
        for m in MIGRATIONS:
            try: c.execute(m)
            except sqlite3.OperationalError: pass
        # passage takes recorded before takes knew their bars: they cover their passage
        c.execute("UPDATE recordings SET sec_a=(SELECT a FROM passages WHERE passages.id=recordings.passage_id), "
                  "sec_b=(SELECT b FROM passages WHERE passages.id=recordings.passage_id) WHERE passage_id IS NOT NULL AND sec_a IS NULL")
        c.commit()
        _local.c = c
    return c

def q(sql, args=()):
    return [dict(r) for r in db().execute(sql, args).fetchall()]

def q1(sql, args=()):
    r = db().execute(sql, args).fetchone(); return dict(r) if r else None

def ex(sql, args=()):
    c = db(); cur = c.execute(sql, args); c.commit(); return cur.lastrowid

def piece_dir(pid): p = DATA_DIR / "pieces" / str(pid); p.mkdir(parents=True, exist_ok=True); return p
def rec_dir(rid): p = DATA_DIR / "recordings" / str(rid); p.mkdir(parents=True, exist_ok=True); return p

def read_json(path, default=None):
    try: return json.loads(Path(path).read_text(encoding="utf-8"))
    except Exception: return default

def write_json(path, obj):
    path = Path(path); tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(json.dumps(obj, separators=(",", ":")), encoding="utf-8"); tmp.replace(path)

def now(): return time.time()

def setting(key, default=None):
    r = q1("SELECT value FROM settings WHERE key=?", (key,))
    return json.loads(r["value"]) if r else default

def set_setting(key, value):
    ex("INSERT INTO settings(key,value) VALUES(?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value", (key, json.dumps(value)))

def delete_piece(pid):
    ex("DELETE FROM cues WHERE piece_id=?", (pid,)); ex("DELETE FROM flags WHERE piece_id=?", (pid,)); ex("DELETE FROM practice_log WHERE piece_id=?", (pid,)); ex("DELETE FROM passages WHERE piece_id=?", (pid,))
    for r in q("SELECT id FROM recordings WHERE piece_id=?", (pid,)): delete_recording(r["id"])
    ex("DELETE FROM pieces WHERE id=?", (pid,)); ex("DELETE FROM jobs WHERE piece_id=?", (pid,))
    shutil.rmtree(DATA_DIR / "pieces" / str(pid), ignore_errors=True)

def delete_recording(rid):
    ex("DELETE FROM flags WHERE recording_id=?", (rid,))
    ex("DELETE FROM recordings WHERE id=?", (rid,)); ex("DELETE FROM jobs WHERE recording_id=?", (rid,))
    shutil.rmtree(DATA_DIR / "recordings" / str(rid), ignore_errors=True)
