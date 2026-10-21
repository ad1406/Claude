# Night Wind: a synced walkthrough

Medtner, Sonata in E minor, Op. 25 No. 2 ("Night Wind"). **Open `nightwind/index.html` in Chrome or Edge.** The page plays
`Night Wind.mp3` from the repository root, so keep it there.

One page: the score (engraved from the MusicXML), the recording, and the analysis, kept in sync note by note.

- **Walkthrough.** About forty stations, from the Introduction to the last chord, follow the music. ↑/↓ moves between them.
- **Motifs.** Four families (the Call, the Song, the Descent, the Wind figures) and fifteen motifs, with about 1,300
  statements found in the score. Each statement is boxed per system, and its notes are ringed. A statement can be exact,
  tonal, contour-only or octave-equivalent, inverted, augmented or diminished, ornamented, or structural (found in the
  beat-by-beat skeleton of a figuration).
- **Wedges.** A rising Song statement and a falling Descent statement that sound together in different voices. Press W on a
  statement to see its partner.
- **Live cursor.** Yellow dots mark the notes sounding in the recording.
- **Keys.** Press ? for the full list. The main ones: Space, ←/→ (bars), ↑/↓ (stations), N/P (next/previous statement), M
  (select a motif), 1–4 (families), S (solo), Enter (play only this statement). Click a note to see every statement it belongs to.

## Rebuilding

Everything is generated from `src/night-wind.mxl` and the recording
(`pip install verovio librosa synctoolbox numba pretty_midi`; unzip the MXL to `src/mxl/` first):

1. `tools/render.py`: verovio renders the MXL into SVG pages with the original system and page breaks, plus MEI and a timemap.
2. `tools/notes.py`: a note table with verovio's ids, bar, staff, voice, onset and sounding pitch (octaves come from the MusicXML).
3. `tools/layout.py`: notehead positions, bar boxes and system boxes taken from the SVGs.
4. `tools/sync.py`: score-to-audio alignment. Multiscale DTW on chroma and onset features, then every onset is refined
   against a pitch-informed onset function (the energy rise in that onset's own pitches) and smoothed. Bar lines come from the
   score, not the MIDI.
5. `tools/match.py` + `tools/catalog.py`: the motif matcher and the motif definitions. It matches on surface lines (each
   voice, hand and texture, top and bottom) and on reduced lines (per half beat, beat, two beats and bar). It allows inserted
   embellishing notes, octave folding and collapsed repeated tones, and checks pitch, rhythm and transformation.
6. `tools/build.py` writes `data.js`. `tools/review.py` draws the boxes on PNG pages for checking.
