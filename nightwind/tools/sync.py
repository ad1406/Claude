"""Recording <-> score, note by note.

1. The MXL notes (verovio note table, nominal seconds from the score's tempo marks) are rendered to chroma + onset (DLNCO)
   features and aligned to the recording with multiscale DTW (synctoolbox).
2. Every score onset (a chord or single note) is then refined against a pitch-informed onset function: the rise of energy in
   that onset's own pitches (CQT, ~12 ms hop), searched near the DTW estimate. A refined time is kept only if it is a clear
   peak; the correction curve is median-filtered so a single bad snap cannot pull a bar away from its neighbours.
3. Output: an audio time for every score onset, every note and every bar."""
import json, os, warnings, numpy as np, pandas as pd, librosa
warnings.filterwarnings("ignore")
from scipy.ndimage import median_filter, maximum_filter1d
from synctoolbox.feature.csv_tools import df_to_pitch_features, df_to_pitch_onset_features
from synctoolbox.feature.pitch import audio_to_pitch_features
from synctoolbox.feature.pitch_onset import audio_to_pitch_onset_features
from synctoolbox.feature.chroma import pitch_to_chroma, quantize_chroma
from synctoolbox.feature.dlnco import pitch_onset_features_to_DLNCO
from synctoolbox.dtw.mrmsdtw import sync_via_mrmsdtw
from synctoolbox.dtw.utils import make_path_strictly_monotonic
HERE = os.path.dirname(os.path.abspath(__file__)); ROOT = os.path.dirname(HERE)
SR, FR, HOP = 22050, 50, 256

def score_events(notes):
    """Onset events: unique q0 of sounding (not tied-into) notes; nominal time from verovio's tstamp (ms)."""
    ev = {}
    for n in notes:
        if n["tie"]: continue
        e = ev.setdefault(round(n["q0"], 4), dict(q=round(n["q0"], 4), t=n["t0"] / 1000.0, pitches=set(), grace=True))
        e["pitches"].add(n["pitch"]); e["grace"] = e["grace"] and bool(n["grace"])
    return sorted(ev.values(), key=lambda e: e["q"])

def render(notes):
    rows = []
    for n in notes:
        if n["tie"]: continue
        # duration: follow ties to the last tied note
        rows.append((n["t0"] / 1000.0, max(0.05, n["dur_s"]), n["pitch"], 80, "piano"))
    df = pd.DataFrame(rows, columns=["start", "duration", "pitch", "velocity", "instrument"]).sort_values("start")
    ch = quantize_chroma(pitch_to_chroma(df_to_pitch_features(df, feature_rate=FR)))
    dl = pitch_onset_features_to_DLNCO(df_to_pitch_onset_features(df), feature_sequence_length=ch.shape[1], feature_rate=FR)
    return ch, dl

def main(audio_path, out):
    D = json.load(open(os.path.join(ROOT, "build", "notes.json"))); notes = D["notes"]
    # durations in seconds (via tempo: tstamp of the note's end = interpolate qstamp->tstamp)
    q = np.array([n["q0"] for n in notes]); t = np.array([n["t0"] for n in notes]) / 1000.0
    o = np.argsort(q); qs, ts = q[o], t[o]
    nxt = {n["tie"]: n for n in notes if n["tie"]}
    for n in notes:
        end = n
        while end["id"] in nxt: end = nxt[end["id"]]
        n["dur_s"] = float(np.interp(end["q1"], qs, ts) - n["t0"] / 1000.0)
    sch, sdl = render(notes)
    y, _ = librosa.load(audio_path, sr=SR, mono=True)
    hop = SR // 20; db = librosa.amplitude_to_db(librosa.feature.rms(y=y, frame_length=2 * hop, hop_length=hop)[0], ref=np.max)
    on = np.where(db > -40)[0]; a = max(0, on[0] * hop - SR // 20); b = min(len(y), on[-1] * hop + SR); off = a / SR
    yy = y[a:b]
    tun = int(round(100 * librosa.estimate_tuning(y=yy[:SR * 180], sr=SR)))
    ach = quantize_chroma(pitch_to_chroma(audio_to_pitch_features(yy, Fs=SR, tuning_offset=tun, feature_rate=FR)))
    adl = pitch_onset_features_to_DLNCO(audio_to_pitch_onset_features(yy, Fs=SR, tuning_offset=tun), feature_sequence_length=ach.shape[1], feature_rate=FR)
    p = make_path_strictly_monotonic(sync_via_mrmsdtw(sch, ach, sdl, adl, input_feature_rate=FR, verbose=False)).astype(float) / FR
    p[1] += off
    ev = score_events(notes)
    et = np.array([e["t"] for e in ev]); dtw = np.interp(et, p[0], p[1])
    # pitch-informed onset function: CQT from A0, 1 bin per semitone, log-magnitude rise (half-wave rectified)
    C = np.abs(librosa.cqt(y, sr=SR, hop_length=HOP, fmin=librosa.note_to_hz("A0"), n_bins=88, bins_per_octave=12, tuning=tun / 100.0))
    L = np.log1p(100 * C); flux = np.maximum(0, L[:, 2:] - L[:, :-2]); flux = np.pad(flux, ((0, 0), (1, 1)))
    flux = maximum_filter1d(flux, 3, axis=0)                       # +-1 semitone tolerance (tuning, inharmonicity)
    fr = lambda x: int(round(x * SR / HOP))
    W = fr(0.12)
    refined = dtw.copy(); conf = np.zeros(len(ev))
    for i, e in enumerate(ev):
        ps = [p_ - 21 for p_ in e["pitches"] if 0 <= p_ - 21 < 88]
        if not ps: continue
        rows = ps + [x + 12 for x in ps if x + 12 < 88]
        c = fr(dtw[i]); lo, hi = max(0, c - W), min(flux.shape[1], c + W + 1)
        f = flux[rows, lo:hi].sum(0) / len(rows)
        if len(f) < 3: continue
        k = int(np.argmax(f)); base = np.median(flux[rows, max(0, c - 4 * W):c + 4 * W].sum(0) / len(rows)) + 1e-6
        conf[i] = f[k] / base
        if conf[i] > 2.5 and not e["grace"]: refined[i] = (lo + k) * HOP / SR
    corr = refined - dtw
    corr = np.where(conf > 2.5, corr, np.nan)
    # smooth the correction: median over neighbouring confident events, then use the event's own snap only if it agrees
    idx = np.where(~np.isnan(corr))[0]
    smooth = np.interp(np.arange(len(ev)), idx, median_filter(corr[idx], size=9, mode="nearest")) if len(idx) else np.zeros(len(ev))
    final = dtw + smooth
    own = ~np.isnan(corr) & (np.abs(corr - smooth) < 0.06)
    final[own] = refined[own]
    final = np.maximum.accumulate(final)                             # monotonic
    evt = {e["q"]: float(x) for e, x in zip(ev, final)}
    # bars: the time of the event at the bar's start; if none (rest / tie), interpolate in score time
    eq = np.array([e["q"] for e in ev])
    def at(qv):
        if round(qv, 4) in evt: return evt[round(qv, 4)]
        return float(np.interp(qv, eq, final))
    bars = [at(m["q"]) for m in D["measures"]]
    end_q = max(n["q1"] for n in notes)
    note_t = {n["id"]: round(at(n["q0"]), 3) for n in notes}
    stats = dict(events=len(ev), confident=int((conf > 2.5).sum()), own=int(own.sum()),
                 median_abs_corr_ms=float(np.nanmedian(np.abs(corr)) * 1000), duration=len(y) / SR)
    json.dump(dict(bars=[round(x, 3) for x in bars], end=round(at(end_q), 3), events=[[e["q"], round(float(x), 3)] for e, x in zip(ev, final)],
                   notes=note_t, stats=stats), open(out, "w"))
    print(stats)

if __name__ == "__main__":
    main(os.path.join(ROOT, "..", "Night Wind.mp3"), os.path.join(ROOT, "build", "sync.json"))
