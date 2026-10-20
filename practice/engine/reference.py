"""Synthetic reference features rendered from OMR notes (no MIDI, no audio synthesis: pitch activations directly)."""
import numpy as np, pandas as pd, warnings
warnings.filterwarnings("ignore")
from synctoolbox.feature.csv_tools import df_to_pitch_features, df_to_pitch_onset_features
from synctoolbox.feature.chroma import pitch_to_chroma, quantize_chroma, quantized_chroma_to_CENS
from synctoolbox.feature.dlnco import pitch_onset_features_to_DLNCO
from .features import FR
QS = 60 / 90.0          # nominal seconds per quarter note for the first pass (tempo-agnostic start)

def cens2(ch):
    """2 Hz CENS for coarse (structure-level) alignment."""
    c, _ = quantized_chroma_to_CENS(ch, 41, 25, FR); return c

def render(bars, durs, with_onsets=True):
    """bars: list of dicts {notes:[(off_q,dur_q,midi)], len_q, content_q}; durs: seconds per bar.
    Notes are scaled into each bar (OMR rhythm errors are squeezed to fit the bar)."""
    rows = []; starts = []; t = 0.0
    for b, D in zip(bars, durs):
        starts.append(t); L = max(b["len_q"], 1e-3)
        sc = D / L; sq = min(1.0, L / max(b.get("content_q", L), 1e-3))
        for o, d, p in b["notes"]:
            rows.append((t + o * sq * sc, max(d * sq * sc, 0.05), int(p), 80, "piano"))
        t += D
    n = int(np.ceil(t * FR)) + 1
    if rows:
        df = pd.DataFrame(rows, columns=["start", "duration", "pitch", "velocity", "instrument"]).sort_values("start")
        fp = df_to_pitch_features(df, feature_rate=FR); ch = quantize_chroma(pitch_to_chroma(fp))
    else:
        ch = np.zeros((12, n)); df = None
    if ch.shape[1] < n: ch = np.pad(ch, ((0, 0), (0, n - ch.shape[1])))
    dl = None
    if with_onsets:
        dl = pitch_onset_features_to_DLNCO(df_to_pitch_onset_features(df), feature_sequence_length=ch.shape[1], feature_rate=FR) if df is not None else np.zeros_like(ch)
    return np.array(starts + [t]), ch, dl
