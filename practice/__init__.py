"""Music practice tool."""
import os
from pathlib import Path
APP_DIR = Path(__file__).resolve().parent.parent          # .../Music Practice/app
ROOT_DIR = APP_DIR.parent                                 # the user's Music Practice folder
DATA_DIR = Path(os.environ["PRACTICE_DATA"]) if os.environ.get("PRACTICE_DATA") else ROOT_DIR / "data"
LOG_DIR = APP_DIR / "logs"
WEB_DIR = Path(__file__).resolve().parent / "web"
