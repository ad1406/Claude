"""Setup self-check. Results go to logs/doctor.json so problems on the user's machine can be diagnosed remotely."""
import importlib, json, platform, shutil, sys, time, traceback, subprocess
from . import LOG_DIR, ROOT_DIR, DATA_DIR
STATE = {"started": time.time(), "checks": {}, "done": False}

def _check(name, fn):
    t = time.time()
    try:
        STATE["checks"][name] = {"ok": True, "detail": fn(), "sec": round(time.time() - t, 1)}
    except Exception as e:
        STATE["checks"][name] = {"ok": False, "detail": f"{type(e).__name__}: {e}", "trace": traceback.format_exc()[-1500:], "sec": round(time.time() - t, 1)}
    _save()

def _save():
    try: (LOG_DIR / "doctor.json").write_text(json.dumps(STATE, indent=1))
    except Exception: pass

def deno_path():
    try:
        import deno  # pip package shipping the deno binary
        p = deno.find_deno_bin()
        return str(p)
    except Exception:
        return shutil.which("deno")

def run():
    _check("python", lambda: f"{sys.version.split()[0]} on {platform.platform()}")
    for mod in ["numpy", "scipy", "librosa", "synctoolbox", "pymupdf", "music21", "av", "yt_dlp", "homr", "onnxruntime", "cv2"]:
        _check(f"import {mod}", lambda m=mod: getattr(importlib.import_module(m), "__version__", "ok"))
    _check("deno (for YouTube)", lambda: deno_path() or (_ for _ in ()).throw(RuntimeError("deno not found")))
    def omr_models():
        r = subprocess.run([sys.executable, "-W", "ignore", "-c", "import sys; from homr.main import main; sys.argv=['homr','--init']; main()"],
                           capture_output=True, text=True, timeout=900)
        if r.returncode: raise RuntimeError((r.stderr or r.stdout)[-400:])
        return "models ready"
    _check("OMR models", omr_models)
    def decode():
        import av, numpy as np
        f = next((p for p in ROOT_DIR.glob("*.m4a")), None) or next((p for p in ROOT_DIR.glob("*.mp3")), None)
        if not f: return "no audio file to test"
        c = av.open(str(f)); s = c.streams.audio[0]; n = 0
        for i, fr in enumerate(c.decode(s)):
            n += fr.samples
            if i > 50: break
        return f"decoded {n} samples from {f.name}"
    _check("audio decoding", decode)
    def youtube():
        import yt_dlp
        opts = {"quiet": True, "skip_download": True, "noplaylist": True}
        d = deno_path()
        if d: opts["js_runtimes"] = {"deno": {"path": d}}
        with yt_dlp.YoutubeDL(opts) as y:
            info = y.extract_info("https://www.youtube.com/watch?v=_OEaXf8uZUg", download=False)
        fmts = [f for f in info.get("formats", []) if f.get("acodec") not in (None, "none") and f.get("vcodec") in (None, "none")]
        return f"'{info.get('title','?')[:50]}' - {len(fmts)} audio formats"
    _check("YouTube access", youtube)
    STATE["done"] = True; _save()
