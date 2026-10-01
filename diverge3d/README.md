# Diverge E5 Workshop (desktop app)

A native desktop app for learning how a Specialized Diverge E5 gravel bike works and how to fix it. It's written in plain C99 and draws everything itself with OpenGL. A 3D model of the bike sits in a white void: orbit around it, pedal it, shift it, brake it, and take each mechanism apart on its own bench.

![modes](#) <!-- Explore · Systems · Benches · Fix it -->

## What's in it

- **Explore**: a procedurally built 3D bike with 22 clickable parts: frame, fork, headset, cockpit, levers, cables, seatpost, saddle, bottom bracket, crankset, pedals, chain, both derailleurs, hanger, cassette, hubs, rims and spokes, tyres, thru-axles, rotors and calipers. Hover shows a part's name; clicking opens it in the inspector (what it does, the spec on this bike, what it works with, a quick check, and the problems it causes). Double-click flies the camera to it. The drivetrain is live: the chain is laid link by link along the real path around the chainring, cassette and jockey wheels, and it changes when you shift. Stop pedalling while moving and the freehub lets the wheels coast while the cranks and chain stop.
- **Systems**: five guided runs (pedal power, a rear shift, braking, steering, your weight and comfort). Each one steps through the bike part by part while the camera follows and the active part is highlighted.
- **Benches**: six mechanism benches (gearing, rear derailleur, front derailleur, brakes, wheels and tyres, headset and bottom bracket). Each has adjusters (barrel adjuster, limit screws, bent hanger, trim, pad wear, cable stretch, air in the hydraulic line, oil on the pads, bent rotor, tyre pressure, headset preload), a live 2D schematic, and plain-language status messages. The 3D bike responds too: the jockey wheels move off the cog, the rotor wobbles, and the contact patch grows.
- **Fix it**: 21 symptoms, searchable and filterable. Each has likely causes in order, a quick check, numbered fix steps, tools, and when to go to a shop. **Recreate this fault** sets up the matching bench so you can see the fault and practise the fix.
- **Bike setup**: base (Claris 2×8, mechanical discs) or Elite-type (GRX 2×10, hydraulic), plus your actual chainrings and cassette. The 3D drivetrain is rebuilt to match.

## Run it on Windows

A prebuilt 64-bit Windows binary is at `bin/DivergeE5Workshop.exe`. It is a single self-contained file (about 300 KB) that needs only DLLs that ship with Windows (`opengl32`, `gdi32`, `user32`, `kernel32`, `msvcrt`). Double-click it.

Because the file is unsigned, Windows SmartScreen may warn the first time. Choose **More info → Run anyway**, or build it yourself as below.

### Build it on Windows

Run `build.bat` from this folder. It uses whichever compiler it finds:

- **Visual Studio / Build Tools** (free, "Desktop development with C++"): open *Developer Command Prompt for VS*, `cd` here, run `build.bat`.
- **MinGW-w64** (for example from MSYS2: `pacman -S mingw-w64-ucrt-x86_64-gcc`), with `gcc` on `PATH`: run `build.bat`.

Or by hand:

```bat
:: MSVC
cl /O2 /utf-8 /Fe:DivergeE5Workshop.exe src\*.c /link /SUBSYSTEM:WINDOWS opengl32.lib gdi32.lib user32.lib
:: MinGW-w64
gcc -O2 -std=c99 -mwindows -o DivergeE5Workshop.exe src\app.c src\bike.c src\content.c src\diagrams.c src\font.c src\mem.c src\mesh.c src\sim.c src\ui.c src\platform_win32.c -lopengl32 -lgdi32 -luser32 -lm
```

(`platform_x11.c` compiles to nothing on Windows, so `src\*.c` is fine with MSVC.)

### Build on Linux, or cross-compile for Windows

```sh
make            # Linux build: ./diverge-e5 (needs libx11-dev and libgl-dev)
make windows    # bin/DivergeE5Workshop.exe via x86_64-w64-mingw32-gcc
make debug      # Linux build with AddressSanitizer + UBSan + LeakSanitizer
```

## Controls

| | |
|---|---|
| Drag | Orbit around the bike |
| Right-drag or middle-drag | Pan |
| Scroll | Zoom |
| Click / double-click a part | Inspect it / fly to it |
| Space | Start or stop pedalling |
| B (hold) | Brake |
| `[` and `]` | Easier / harder rear cog |
| `1` and `2` | Small / big chainring |
| L, R, F, Esc | Labels, reset view, focus selection, clear |

## Memory

- Every heap allocation goes through `src/mem.c`, which counts live blocks and bytes.
- The 3D geometry is generated once into OpenGL display lists, and the CPU-side vertex buffers are freed immediately. The font file is freed as soon as its glyphs are packed into a texture.
- After start-up the app holds **no heap memory of its own**: the frame loop never allocates. Everything per-frame (chain path, UI state, text) uses fixed-size static or stack buffers.
- The display lists and the glyph texture are deleted at exit. Changing the chainrings or cassette deletes the old lists before building new ones.

The self-test proves this. It drives every mode and bench, rebuilds the drivetrain repeatedly, checks 3D picking, saves screenshots, and fails with exit code 2 if a single block is still allocated at exit:

```sh
./diverge-e5 --selftest out_dir          # Linux
DivergeE5Workshop.exe --selftest out_dir # Windows
```

It has also been run clean (no errors, no leak reports) under AddressSanitizer + UBSan + LeakSanitizer (full self-test) and Valgrind memcheck (start-up, drivetrain rebuilds, picking, rendering and shutdown: 0 errors, 0 bytes definitely or indirectly lost).

## Code layout

| File | What it does |
|---|---|
| `src/platform_win32.c`, `src/platform_x11.c` | Window, OpenGL context (multisampled when available), input, timing, system font lookup |
| `src/mesh.c` | Procedural meshes: tubes swept along splines, lathed rims, toothed gears, tori, boxes, compiled to display lists |
| `src/bike.c` | The bike: geometry in millimetres, part drawing, chain routing (tangent lines between sprockets), picking, ground shadow |
| `src/sim.c` | Ride physics (cadence, gearing, freewheel, braking) and the mechanism models behind each bench |
| `src/diagrams.c` | 2D schematics and status messages for the benches |
| `src/ui.c`, `src/font.c` | Immediate-mode UI toolkit and anti-aliased text (stb_truetype, using the system's Segoe UI or Arial) |
| `src/content.c` | All the explanatory text: parts, problems, guided runs, bench notes |
| `src/app.c` | Layout, camera, modes, self-test |
| `src/vendor/stb_truetype.h` | Sean Barrett's public-domain TrueType rasteriser |

Specs and torque values vary by model year: a number printed on a part always wins over anything in this app. This is an independent study aid, not a Specialized publication.
