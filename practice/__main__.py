import warnings; warnings.filterwarnings("ignore")
import argparse, logging, os, shutil, subprocess, sys, threading, time, webbrowser
from . import DATA_DIR, LOG_DIR

def find_browser():
    """Chrome or Edge, so the app can open in its own window (no tabs, no address bar)."""
    cands = []
    for base in [os.environ.get("PROGRAMFILES", ""), os.environ.get("PROGRAMFILES(X86)", ""), os.environ.get("LOCALAPPDATA", "")]:
        if base:
            cands += [os.path.join(base, "Google", "Chrome", "Application", "chrome.exe"),
                      os.path.join(base, "Microsoft", "Edge", "Application", "msedge.exe")]
    for c in cands:
        if os.path.exists(c): return c
    for name in ("google-chrome", "chromium", "chromium-browser", "microsoft-edge"):
        p = shutil.which(name)
        if p: return p
    return None

def open_app_window(url):
    b = find_browser()
    if b:
        prof = DATA_DIR / "browser-profile"
        subprocess.Popen([b, f"--app={url}", "--window-size=1440,900", f"--user-data-dir={prof}", "--autoplay-policy=no-user-gesture-required"])
    else:
        webbrowser.open(url)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--no-open", action="store_true")
    a = ap.parse_args()
    DATA_DIR.mkdir(exist_ok=True); LOG_DIR.mkdir(exist_ok=True)
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s",
                        handlers=[logging.FileHandler(LOG_DIR / "app.log", encoding="utf-8"), logging.StreamHandler(sys.stdout)])
    import uvicorn
    from .server import app
    url = f"http://127.0.0.1:{a.port}/"
    if not a.no_open:
        threading.Thread(target=lambda: (time.sleep(1.5), open_app_window(url)), daemon=True).start()
    uvicorn.run(app, host="127.0.0.1", port=a.port, log_level="warning")

if __name__ == "__main__":
    main()
