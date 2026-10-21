# Night Wind walkthrough → MP4

Renders what the walkthrough page shows into a video, using the recording as the soundtrack:
- **Score:** two or three systems at a time, turning with the music. The bar being played is outlined in yellow, the notes
  sounding now get yellow dots, every motif statement is boxed with its notes ringed, and statements that are sounding light
  up and show their name.
- **Right panel:** the walkthrough text for the current station, and the list of statements sounding now.
- **Bottom:** the timeline of the four motif families.

It runs entirely on your laptop. No browser or internet connection is needed after installing the packages.

## Windows

Double-click **`make_video.bat`**. It installs the three Python packages it needs (`pillow`, `numpy`, `imageio-ffmpeg`;
the last one includes ffmpeg) and writes `night-wind.mp4` next to it. You need Python 3.9 or newer from python.org.

## Any system, with options

```
pip install pillow numpy imageio-ffmpeg
python make_video.py                              # the whole sonata (about 32 minutes of video)
python make_video.py --start 285 --end 371        # an excerpt, by bar numbers
python make_video.py --preview 496                # one still frame at bar 496 -> preview.png
python make_video.py --families song,descent      # only some families (call, song, descent, wind)
python make_video.py --motifs call,chrom,counter  # only some motifs (ids as in tools/catalog.py)
python make_video.py --size 1280x720 --fps 30 --crf 18 --out excerpt.mp4
```

From Command Prompt the options also work with the .bat: `make_video.bat --start 372 --end 441`.

At 1920×1080 the encoder runs at roughly real-time speed on a typical laptop, so the full sonata takes about half an hour.
Use `--size 1280x720` to halve that.

## What it reads

`../data.js` (motifs, sync and layout), `../build/png/` (the engraved pages), `../build/text.json` (the walkthrough text,
exported from `analysis.js` by `node tools/export_text.js`) and `Night Wind.mp3` in the repository root. If you change the
analysis, rerun `tools/build.py` and `tools/export_text.js`. If you re-render the score, re-create the PNGs from the SVGs too.
