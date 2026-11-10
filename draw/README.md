# synthkit/draw: the shared drawing package

A hardware panel drawn with `juce::Graphics` and nothing else: no PNG, no sprite,
no `drawImage`. Every piece takes a `Pen` (the graphics, the synth's `Tokens`, an
optional `Ledger`) and draws in the caller's DESIGN units; the caller scales the
graphics once, so the same calls are sharp at 1x, 2x and any window size.

Linked as `synthkitdraw` through `cmake/Siblings.cmake` (an INTERFACE library with
its sources, like `synthkitpanel`). First user: Sub37 (build.md R1, 2026-11-01).
The vocabulary is lifted from Stellix's `lib/painter/src/draw.h`: the Pen, the
Ledger the probes read, `col`, `text`, `glowDot`, `glowStroke`, `chevron`.

| header | what |
|---|---|
| `Pen.h` | the Pen, the Ledger (what was drawn, part by part), `col` |
| `Tokens.h` | the token table a synth fills: every colour, ratio and size the pieces use |
| `Type.h` | the text law: one family, a floor under every size, wrap to two lines before shrinking |
| `Light.h` | a light with exactly two looks, lit and unlit; `glowDot`, `glowStroke` |
| `Knob.h` | the knob (skirt, brushed cap, pointer) and its printed dial (ticks, numerals) |
| `Caps.h` | the cap, the round button, the lens and the click-the-choice selector |
| `Frames.h` | title bar, well, rule, display frame, reversed legend |
| `Glyphs.h` | the printed waveform and stage glyphs, as paths |
| `Counting.h` | a software renderer that counts `drawImage` calls, for gates |

The ledger is the point of the package as much as the drawing: a gate asks it what
was drawn where (a section title, a knob, the smallest text) instead of trusting a
spec file, which is how a panel that drew the wrong layout passed every check.
