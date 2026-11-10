# {{DISPLAY}} — {{BRAND}} {{MODEL}}, reimplemented by The Decipherist

Built from `~/projects/synthkit`. **Read `docs/before-you-call-it-finished.md`
before saying anything here is done**, and `docs/laws.md` before changing any
constant.

## Non-negotiables

These apply to every machine. They are findings, not opinions: each one is
invisible until something sounds subtly wrong, and each is easy to "correct" into
a bug.

1. **Round-trip unmapped patch bytes VERBATIM.** Most of a real record is unnamed
   on day one. Discarding what you cannot yet read breaks sysex export and makes
   later attribution impossible — and the loss is silent. One machine's extension
   record was parsed as zeros for weeks and every effects knob, pan, comb and EQ
   default it carried was chased as a separate bug.

2. **Two sources or it is a guess.** The corpus tells you the neutral value, the
   manual tells you the direction, the firmware settles the law. One source is a
   hypothesis. Tag everything (`docs/provenance.md`).

3. **A wrong law that sounds good is worse than a known gap.** A gap is on the
   ledger in `todo.md` and someone will close it. A plausible wrong law gets
   built on.

4. **Never trust a build you have not run.** Not a report, not a summary, not a
   test's exit code when the test prints numbers. Build it, run it, read the
   output, look at the picture.

5. **Never ear-calibrate a unit.** The ear accepted a wheel-to-pitch law at 1/4.7
   of its true strength and an envelope maximum at 1/5, and each felt fine in
   isolation. Units come from the DSP consumer of the slot or an absolute number
   in the manual; the ear only ORDERS the remaining debts.

6. **The cutoff is a SUM, and the term you miss has no parameter behind it.**
   `src/engine/Coeffs.*` writes it out term by term for that reason. If your
   engine sums fewer terms than the firmware, the difference is your bug and it
   is measurable. See `docs/before-you-call-it-finished.md` §3.

7. **Freeze parameter IDs at v0.1 and never renumber.** Hosts persist automation
   by ID. `tools/tests_main.cpp` asserts the list; that assertion is deliberate.

8. **Sync everything that moves, and default it OFF.** This is a plugin in a
   host. Offering the option falsifies no law: it sets a time or a rate from the
   clock instead of from a knob, and it is off until the player asks. What the
   hardware could do governs the SOUND LAWS; it does not govern whether the
   plugin can take a tempo.

9. **The shell, the DSP blocks and the panel renderer are DEPENDENCIES.** If you
   are about to copy a file out of `~/projects/synthshell`, `~/projects/dspblocks`
   or `~/projects/synthpanel-creator`, stop. Fix it there; every machine gets the
   fix on its next build.

10. **Log everything as it happens.** `docs/research.md` for findings,
    `bugs.md` one line per bug, `todo.md` for the debt. Not at the end.

## This machine's own non-negotiables

<!-- Write them the day you find them. The ones that are invisible until
     something sounds subtly wrong: a divisor that is not the obvious number, a
     table that is two tables, a filter that shares the oscillator's curve, a
     byte that is a mode flag rather than a value. -->

*(nothing yet — this machine has not been decoded)*

## Build

```
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

Standalone for listening: `cmake --build build --target {{NAME}}_Standalone`, then
`open build/{{NAME}}_artefacts/RelWithDebInfo/Standalone/{{DISPLAY}}.app`. For the
tight edit-build-listen loop use JUCE's AudioPluginHost rather than a DAW — no
scan, no cache.

`docs/the-gates.md` says what each test catches and how it is proved to work.

## Panel

`panel/panel.json` is the spec; `~/projects/synthpanel-creator` renders it.
**Panel drawing, themes and layout are fixed THERE, never patched here.** A local
patch forks the renderer and every other machine misses the fix.

```
npm run panel:validate     # the spec is well formed
npm run panel:check        # legends fit, nothing overlaps
npm run panel:png          # look at it
npm run panel:native       # export to resources/native for the plugin
```

**There is no panel code in this repo and there never will be.** The component
that paints the export is `synthkit::NativePanel`, linked in as `synthkitpanel`
from `~/projects/synthkit/panel/`. Until the export exists it draws the SPEC —
plain shapes at the spec's own coordinates, with "panel artwork pending" across
its own face — so the unit is playable and the bindings are testable from day
one. **Nothing is swapped when the artwork lands**: run `npm run panel:native`,
rebuild, and the same component paints the real panel with the same ids, the
same events and the same hit areas.

Then LOOK AT IT, at the width the window opens rather than the width of the
render. `ExampleSynthSnapshot` writes the PNG; a green build says nothing about
a picture nobody looked at, and three units shipped the placeholder that way.

## Status

v0.1 — skeleton from synthkit. The engine is a stand-in and every law in it is
tagged `[REF]`. Nothing here is {{BRAND}}'s yet.
