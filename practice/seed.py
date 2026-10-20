"""First run: put the pieces and recordings that are already in the Music Practice folder into the library,
using score readings / structure markup prepared in advance (app/seed/<slug>/...)."""
import json, shutil
from pathlib import Path
from . import store, jobs, ROOT_DIR, APP_DIR

def run():
    seed = APP_DIR / "seed" / "seed.json"
    if not seed.exists() or store.setting("seeded") or store.q1("SELECT id FROM pieces LIMIT 1"): return
    store.set_setting("seeded", True)             # only ever once: deleting the examples must not bring them back
    spec = json.loads(seed.read_text(encoding="utf-8"))
    for p in spec["pieces"]:
        pdf = ROOT_DIR / p["pdf"]
        if not pdf.exists(): continue
        pid = store.ex("INSERT INTO pieces(title,composer,created,updated,status,page_from,page_to) VALUES(?,?,?,?,?,?,?)",
                       (p["title"], p.get("composer", ""), store.now(), store.now(), "queued", p.get("page_from"), p.get("page_to")))
        d = store.piece_dir(pid); shutil.copy(pdf, d / "score.pdf")
        store.ex("UPDATE pieces SET pdf=? WHERE id=?", (str(d / "score.pdf"), pid))
        src = APP_DIR / "seed" / p["slug"]
        if (src / "omr").exists(): shutil.copytree(src / "omr", d / "omr", dirs_exist_ok=True)
        if (src / "structure.json").exists(): shutil.copy(src / "structure.json", d / "structure.json")
        jobs.enqueue("score", pid)
        for k, r in enumerate(p.get("recordings", [])):
            f = ROOT_DIR / r["path"] if r.get("path") else None
            if f is None or not f.exists(): continue
            rid = store.ex("INSERT INTO recordings(piece_id,name,kind,source,url,path,range_start,range_end,created,status,sort) VALUES(?,?,?,?,?,?,?,?,?,?,?)",
                           (pid, r["name"], r.get("kind", "reference"), "youtube" if r.get("url") else "file", r.get("url"), str(f),
                            r.get("range_start"), r.get("range_end"), store.now(), "queued", k))
            jobs.enqueue("features", pid, rid)
