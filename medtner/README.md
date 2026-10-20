# Medtner: Night Wind and Sonata minacciosa

An analysis of Medtner's Sonata in E minor, Op. 25 No. 2 ("Night Wind") and the Sonata minacciosa, Op. 53 No. 2. The
analysis is synced to the recordings and the scores.

**Open `medtner/index.html` in Chrome or Edge.** No server needed. The page plays `Night Wind.mp3` and `Minacciosa.mp3`
from the repository root.

- **Score**: every printed bar is clickable and plays the recording from that bar. The current bar is highlighted and the
  pages turn by themselves (untick *follow* to browse freely). Coloured strips under the bars mark motif statements.
- **Timeline**: sections, the recording's tempo relative to the MIDI, and motif statements. Click it to seek.
- **Form**: a section-by-section analysis. Every `b. 123` is a link.
- **Motifs**: each motif with every statement listed (bar, hand, prime or inversion, augmentation or diminution, pitches). ▶ plays just
  that statement.
- **Themes · Harmony · Rhythm**: essays for each piece.
- **Compare**: proportions, chromatic saturation, where the motif statements fall, and a written comparison.
- Keys: space plays and pauses, ←/→ move by bar, PageUp/PageDown turn pages.

## How the sync works

`tools/` rebuilds everything from the files in the repository root (`pip install pymupdf pretty_midi librosa synctoolbox`, then
`python medtner/tools/build.py`):

- `pdfbars.py`: the PDFs are vector engravings, so the bar boxes come straight from the drawing. Staff lines are the long
  thin strokes, and barlines are the strokes that start on the top staff line and end on the bottom one. Checked against the printed measure
  number on every page of both scores.
- `sync.py`: the MIDI is rendered to pitch-chroma and onset (DLNCO) features and aligned to the recording with multiscale DTW
  (synctoolbox, the same approach as `practice/engine`). Each MIDI downbeat is mapped to a time in the recording.
- `motifs.py` + `catalog.py`: each motif is an interval pattern (exact, or by interval class for diatonic variants), optionally
  with a rhythm profile, searched in the upper and lower line of each hand, in prime and inversion.
- `build.py`: writes `data.js` (bars, times, motif statements, tempo, chromatic saturation) and the page images in `pages/`.
