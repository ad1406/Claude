"""Optional second opinion on bar counts from a vision model (DeepSeek V4.1 Flash, OpenAI-compatible API).

Most editions print the bar number at the start of each line. We send a small crop of every line's start and ask
only for that number (reading digits is what these models are reliable at; coordinates are not). Comparing the
printed numbers with our own bar labels shows which lines have a barline too many or too few, so the user checks
only those lines. Nothing is changed automatically. Needs an API key (Settings); costs a fraction of a cent per page.
"""
import base64, io, json, re, urllib.request
from PIL import Image
from . import store, piecemodel

DEFAULT_BASE = "https://api.deepseek.com"
DEFAULT_MODEL = "deepseek-flash"
PROMPT = ("Each image is the beginning of one line (system) of a piano score, in order: image 1 = line 1, and so on. "
          "Many editions print the bar (measure) number of the line's first bar in small digits above the top staff, "
          "at the very left. For each image, give that printed number, or null if no number is printed there. "
          "Do not guess or count bars. Answer only with JSON: {\"lines\": [{\"line\": 1, \"number\": 12}, ...]}")

def configured():
    return bool(store.setting("deepseek_key"))

def _crops(pid, page, systems):
    """JPEG data URLs of the start of each line: left margin to ~16% of the width, from a bit above the staff."""
    img = Image.open(store.piece_dir(pid) / "pages" / f"p{page:03d}.png").convert("L"); W, H = img.size
    out = []
    for s in systems:
        x0 = max(0, (s["x_left"] - 0.05) * W); x1 = min(W, (s["x_left"] + 0.16) * W)
        h = (s["y1"] - s["y0"]) * H
        y0 = max(0, s["y0"] * H - 0.25 * h); y1 = min(H, s["y0"] * H + 0.45 * h)
        c = img.crop((int(x0), int(y0), int(x1), int(y1)))
        if c.width < 400: c = c.resize((400, max(1, int(c.height * 400 / c.width))))
        buf = io.BytesIO(); c.save(buf, "JPEG", quality=85)
        out.append("data:image/jpeg;base64," + base64.b64encode(buf.getvalue()).decode())
    return out

def ask(images, timeout=90):
    key = store.setting("deepseek_key"); base = (store.setting("deepseek_base") or DEFAULT_BASE).rstrip("/")
    model = store.setting("deepseek_model") or DEFAULT_MODEL
    content = [{"type": "text", "text": PROMPT}] + [{"type": "image_url", "image_url": {"url": u}} for u in images]
    body = {"model": model, "messages": [{"role": "user", "content": content}], "temperature": 0,
            "response_format": {"type": "json_object"}}
    req = urllib.request.Request(base + "/chat/completions", data=json.dumps(body).encode(),
                                 headers={"Content-Type": "application/json", "Authorization": f"Bearer {key}"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        data = json.loads(r.read().decode())
    text = data["choices"][0]["message"]["content"]
    m = re.search(r"\{.*\}", text, re.S)
    return json.loads(m.group(0) if m else text)

def _num(label):
    m = re.match(r"(\d+)", str(label or "")); return int(m.group(1)) if m else None

def compare(score, structure, printed):
    """printed: {(page, system_idx): number}. Returns (flags, lines, reliable): flags [{page, system, found, expected,
    delta, msg}] for lines whose bar count disagrees with the printed numbering (a missing or extra barline).

    Editions without printed numbers make vision models invent them (tidy runs like 1, 9, 17, 25 that ignore how many
    bars each line has). So the printed numbers must first agree with our own bar counts between most neighbouring
    lines; if they don't, the edition is taken not to print bar numbers and nothing is flagged (reliable=False).
    A page whose numbers mostly disagree is dropped the same way."""
    labels = piecemodel.labels(score, structure); mv, _ = piecemodel.movement_of(score, structure)
    deleted = set(structure.get("deleted", []))
    lines = []
    for s in score["systems"]:
        bars = [b for b in score["bars"] if b["page"] == s["page"] and b["system"] == s["idx"] and b["id"] not in deleted]
        if not bars: continue
        first = bars[0]["id"]
        lines.append(dict(page=s["page"], system=s["idx"], first=first, mv=mv.get(first), n_bars=len(bars),
                          ours=_num(labels.get(first)), printed=printed.get((s["page"], s["idx"]))))
    for l in lines:
        l["off"] = (l["printed"] - l["ours"]) if l["printed"] is not None and l["ours"] is not None else None
    # ---- is the printed numbering real? neighbouring lines must mostly keep the same offset
    def pairs(ls):
        us = [l for l in ls if l["off"] is not None]
        return [(a, b) for a, b in zip(us, us[1:]) if a["mv"] == b["mv"]]
    ps = pairs(lines); agree = sum(a["off"] == b["off"] for a, b in ps)
    reliable = not (len(ps) >= 3 and agree < 0.5 * len(ps))
    if not reliable:
        return [], lines, False
    for pg in sorted({l["page"] for l in lines}):
        pp = pairs([l for l in lines if l["page"] == pg])
        if len(pp) >= 3 and sum(a["off"] == b["off"] for a, b in pp) < 0.34 * len(pp):
            for l in lines:
                if l["page"] == pg: l["off"] = None; l["dropped"] = True
    # offset between printed and our numbering, per line; a change between two lines = a barline count error in
    # the first of them. A single line whose offset differs from both neighbours is a misread digit, not an error.
    usable = [l for l in lines if l["off"] is not None]
    kept = []
    for i, b in enumerate(usable):
        a = kept[-1] if kept else None; c = usable[i + 1] if i + 1 < len(usable) else None
        if a and c and a["mv"] == b["mv"] == c["mv"] and a["off"] == c["off"] != b["off"]: continue   # misread
        kept.append(b)
    usable = kept
    flags = []
    for i in range(len(usable) - 1):
        a, b = usable[i], usable[i + 1]
        if a["mv"] != b["mv"] or a["off"] == b["off"]: continue
        d = a["off"] - b["off"]                       # > 0: we have more bars than the print between a and b
        if abs(d) > 8: continue                      # numbering restarts, etc.
        # the error lies somewhere from line a up to the line before b
        la = lines.index(a); lb = lines.index(b); cand = lines[la:lb]
        tgt = cand[0] if len(cand) == 1 else max(cand, key=lambda l: l["n_bars"] if d > 0 else -l["n_bars"])
        flags.append(dict(page=tgt["page"], system=tgt["system"], found=tgt["n_bars"], expected=tgt["n_bars"] - d, delta=d,
                          msg=f"the print has bar {b['printed']} where this has bar {b['ours']}: "
                              f"{abs(d)} barline{'s' if abs(d) > 1 else ''} too {'many' if d > 0 else 'few'}"
                              + (" in this line?" if len(cand) == 1 else f" in this line or the {len(cand) - 1} after it?")))
    return flags, lines, True

def run_check(pid, progress=lambda f, m="": None):
    score, st = piecemodel.load(pid)
    pages = sorted({s["page"] for s in score["systems"]}); printed = {}
    for i, pg in enumerate(pages):
        progress(i / len(pages), f"asking the vision model about page {pg}")
        systems = [s for s in score["systems"] if s["page"] == pg]
        try: ans = ask(_crops(pid, pg, systems))
        except Exception as e: raise RuntimeError(f"vision model request failed on page {pg}: {e}")
        for item in ans.get("lines", []):
            try: k = int(item.get("line")) - 1; n = item.get("number")
            except Exception: continue
            if 0 <= k < len(systems) and isinstance(n, (int, float)) and n > 0: printed[(pg, systems[k]["idx"])] = int(n)
    flags, lines, reliable = compare(score, st, printed)
    res = dict(flags=flags, printed=[dict(page=p, system=s, number=n) for (p, s), n in printed.items()],
               lines=len(lines), read=len(printed), reliable=reliable, created=store.now())
    store.write_json(store.piece_dir(pid) / "check.json", res)
    return res
