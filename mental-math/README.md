# Quant Mental Math Trainer

A single standalone file, `index.html`. No server or install; open it in any modern browser (double-click on Windows). Progress is saved in the browser's local storage; use **Settings → Export** for backups.

It targets the no-calculator numeracy sections used by Optiver, IMC, Five Rings, DRW, Flow Traders, Citadel, Virtu and similar firms.

## How it adapts

- **Placement** (~8 min) climbs each skill's difficulty ladder until you stumble, so practice starts at your edge.
- **23 skills** (times tables, squares, fraction↔decimal facts, +/−, multi-digit ×, division, decimals, percentages, fractions, estimation, series, expected value, plus technique drills) each have levels with realistic time targets (e.g. 80 questions in 8 min ≈ 6 s each).
- **Weakness model** combines accuracy and speed vs. target per skill. A skill's weakness also raises the priority of its prerequisites (slow 2-digit × 2-digit → check digit facts, addition, techniques), and wrong composite answers flag the digit facts inside them.
- **Item-level spaced repetition** for facts: each fact has its own schedule, so slow or missed ones return within minutes and fast ones go out for days or weeks.
- **Interleaved sessions**, misses re-asked a few questions later, error classification (carry slip, decimal place, transposition...), worked explanations after misses, short lessons for new techniques with cues that fade.
- **Mock tests** with no feedback until the end, Insights (diagnosis, fact heat-map, trends), and Esc to pause (the clock stops, and also when you leave the tab).

Keys: type the answer (exact answers auto-advance), Enter submits, Esc pauses, Tab skips in mock tests. Fractions can be typed as `3/8`.

Test-format notes are approximate; firms change their tests, so check the current instructions for yours.
