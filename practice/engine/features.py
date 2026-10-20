"""Audio decoding and the features used for alignment (cached next to the audio)."""
import numpy as np, warnings, os
warnings.filterwarnings("ignore")
SR = 22050; FR = 50          # feature rate for fine alignment (Hz)

def decode(path, sr=SR):
    """Any audio/video file -> mono float32 at `sr` (PyAV, no external ffmpeg needed)."""
    import av
    c = av.open(str(path)); s = c.streams.audio[0]
    rs = av.AudioResampler(format="flt", layout="mono", rate=sr)
    out = []
    for fr in c.decode(s):
        for r in rs.resample(fr): out.append(r.to_ndarray().reshape(-1))
    for r in rs.resample(None): out.append(r.to_ndarray().reshape(-1))
    c.close()
    return np.concatenate(out).astype(np.float32) if out else np.zeros(0, np.float32)

def compute(path, cache_path, progress=lambda f, msg="": None):
    """chroma (12xN @50Hz), DLNCO onset features (12xN), loudness in dB rel. max (@10Hz), tuning (cents), duration."""
    if cache_path and os.path.exists(cache_path):
        z = np.load(cache_path); return {k: z[k] for k in z.files}
    import librosa
    from synctoolbox.feature.pitch import audio_to_pitch_features
    from synctoolbox.feature.pitch_onset import audio_to_pitch_onset_features
    from synctoolbox.feature.chroma import pitch_to_chroma, quantize_chroma
    from synctoolbox.feature.dlnco import pitch_onset_features_to_DLNCO
    progress(0.05, "decoding audio")
    y = decode(path)
    progress(0.2, "measuring tuning")
    tun = int(round(100 * librosa.estimate_tuning(y=y[:SR * 180], sr=SR))) if len(y) > SR else 0
    progress(0.3, "pitch features")
    chroma = []; step = SR * 300    # in 5-minute chunks to bound memory
    fp = audio_to_pitch_features(y, Fs=SR, tuning_offset=tun, feature_rate=FR)
    ch = quantize_chroma(pitch_to_chroma(fp)); del fp
    progress(0.6, "onset features")
    fo = audio_to_pitch_onset_features(y, Fs=SR, tuning_offset=tun)
    dl = pitch_onset_features_to_DLNCO(fo, feature_sequence_length=ch.shape[1], feature_rate=FR)
    progress(0.9, "loudness")
    hop = SR // 10
    rms = librosa.feature.rms(y=y, frame_length=2 * hop, hop_length=hop)[0]
    db = 20 * np.log10(rms + 1e-6); db -= db.max() if len(db) else 0
    out = dict(chroma=ch.astype(np.float32), dlnco=dl.astype(np.float32), db=db.astype(np.float32),
               tuning=np.array(tun), duration=np.array(len(y) / SR))
    if cache_path: np.savez(cache_path, **out)
    return out
