"""Recording <-> score sync: MIDI notes rendered straight to pitch/onset features, aligned to the recording with
memory-restricted multiscale DTW (synctoolbox, as in practice/engine). Output: start time of every bar in the audio."""
import sys, json, warnings, numpy as np, pandas as pd, librosa, pretty_midi
warnings.filterwarnings("ignore")
from synctoolbox.feature.csv_tools import df_to_pitch_features, df_to_pitch_onset_features
from synctoolbox.feature.pitch import audio_to_pitch_features
from synctoolbox.feature.pitch_onset import audio_to_pitch_onset_features
from synctoolbox.feature.chroma import pitch_to_chroma, quantize_chroma
from synctoolbox.feature.dlnco import pitch_onset_features_to_DLNCO
from synctoolbox.dtw.mrmsdtw import sync_via_mrmsdtw
from synctoolbox.dtw.utils import make_path_strictly_monotonic
SR, FR = 22050, 50

def midi_feats(path):
    pm = pretty_midi.PrettyMIDI(path)
    rows = [(n.start, n.end - n.start, n.pitch, n.velocity, "piano") for i in pm.instruments for n in i.notes]
    df = pd.DataFrame(rows, columns=["start", "duration", "pitch", "velocity", "instrument"]).sort_values("start")
    ch = quantize_chroma(pitch_to_chroma(df_to_pitch_features(df, feature_rate=FR)))
    dl = pitch_onset_features_to_DLNCO(df_to_pitch_onset_features(df), feature_sequence_length=ch.shape[1], feature_rate=FR)
    return pm, ch, dl

def audio_feats(path):
    y, _ = librosa.load(path, sr=SR, mono=True)
    # leading/trailing silence belongs to no bar: a global DTW would otherwise stretch bar 1 / the last bar over it
    hop = SR // 20; db = librosa.amplitude_to_db(librosa.feature.rms(y=y, frame_length=2 * hop, hop_length=hop)[0], ref=np.max)
    on = np.where(db > -40)[0]; a = max(0, on[0] * hop - SR // 20); b = min(len(y), on[-1] * hop + SR)
    full = len(y) / SR; y = y[a:b]
    tun = int(round(100 * librosa.estimate_tuning(y=y[:SR * 180], sr=SR)))
    ch = quantize_chroma(pitch_to_chroma(audio_to_pitch_features(y, Fs=SR, tuning_offset=tun, feature_rate=FR)))
    dl = pitch_onset_features_to_DLNCO(audio_to_pitch_onset_features(y, Fs=SR, tuning_offset=tun), feature_sequence_length=ch.shape[1], feature_rate=FR)
    return ch, dl, full, tun, a / SR

if __name__ == "__main__":
    mid, mp3, out = sys.argv[1:4]
    pm, mch, mdl = midi_feats(mid)
    ach, adl, dur, tun, off = audio_feats(mp3)
    p = make_path_strictly_monotonic(sync_via_mrmsdtw(mch, ach, mdl, adl, input_feature_rate=FR, verbose=False)).astype(float) / FR
    p[1] += off
    downs = pm.get_downbeats()
    end = max(n.end for i in pm.instruments for n in i.notes)
    t = np.interp(np.append(downs, end), p[0], p[1])
    json.dump(dict(bars=[round(float(x), 3) for x in t[:-1]], end=round(float(t[-1]), 3), duration=round(dur, 2), tuning_cents=float(tun),
                   midi_bars=[round(float(x), 3) for x in downs], path=[[round(a, 2), round(b, 2)] for a, b in p[:, ::25].T]), open(out, "w"))
    print(len(downs), "bars; first", t[:3], "end", t[-1], "of", dur)
