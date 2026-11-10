# Moving a unit onto the shared panel

Forty units carried their own copy of `src/ui/NativePanel.{h,cpp}`, the JUCE
component that paints synthpanel-creator's native export. They had drifted. Some
copies had fixes the others never got, and one unit (Ebbe und Flut) shipped a
finished panel spec with the placeholder still on screen because nobody wired a
consumer at all.

There is now one component, `synthkit::NativePanel`, in `panel/`. A unit links it
as `synthkitpanel` and carries no panel code of its own. Ebbe und Flut is the
worked example if you want to read a real diff.

This is a per-unit job and it is mechanical. Do NOT batch it while other agents
are live in those repos.

## What a unit does

1. **Re-copy the cmake helpers**, because `Siblings.cmake` is what finds the
   component:

        cp ~/projects/synthkit/cmake/Siblings.cmake <unit>/cmake/

2. **CMakeLists.txt.** Drop `src/ui/SpecPanel.cpp` (or `src/ui/NativePanel.cpp`)
   from the plugin sources. Add `synthkitpanel` beside `synthshell dspblocks`
   everywhere they appear, which is the plugin, every probe and the snapshot.
   Then put the export into the binary data if it is not there already:

        file(GLOB <UNIT>_PANEL_NATIVE CONFIGURE_DEPENDS
             ${CMAKE_CURRENT_SOURCE_DIR}/resources/native/*.json
             ${CMAKE_CURRENT_SOURCE_DIR}/resources/native/*.png
             ${CMAKE_CURRENT_SOURCE_DIR}/resources/native/sprites/*.png)
        juce_add_binary_data(<Unit>Assets SOURCES
            panel/panel.json
            panel/bindings.json
            ${<UNIT>_PANEL_NATIVE})

   `panel/panel.json` stays in there even when the export exists. It is what the
   component falls back to.

3. **PluginEditor.h.** `#include "ui/SpecPanel.h"` becomes `#include <NativePanel.h>`,
   and the three or four mentions of `SpecPanel` become `synthkit::NativePanel`.

4. **PluginEditor.cpp.** The panel now needs the unit's own binary data, because
   `BinaryData` is a per-target symbol that a shared library cannot see. It is one
   argument in the constructor's init list:

        : juce::AudioProcessorEditor (&p), proc (p), panel (SYNTHKIT_PANEL_ASSETS())

   `PluginEditor.cpp` already includes `BinaryData.h`, which is all that macro needs.

5. **Delete `src/ui/SpecPanel.{h,cpp}` and `src/ui/NativePanel.{h,cpp}`.** Both.
   A leftover copy is the whole problem coming back.

6. Build, `ctest`, then **render the panel and look at it**. This job exists
   because that step was skipped three times in one week.

## What changes for the unit

The event, the ids, the hit areas and the probe hooks are the same, so the
binding code does not move. Three things behave better than most of the old
copies did.

**The panel draws before the artwork exists.** No export, no problem: it reads
`panel/panel.json` and lays plain shapes out at the spec's own coordinates with
"panel artwork pending" written across the top. Same ids, same events, same hit
areas, so the controls probe and the snapshot both work on a unit whose panel has
not been authored yet. The day `npm run panel:native` runs, the same component
paints the real thing. Nothing is swapped and no code changes.

**Text goes to the biggest screen only.** The old whole-panel `setScreen` wrote
to every screen on the panel, which is how the Nord Lead 3 got its patch name
printed in the oscillator-2 pitch readout. Address a second display by id with
`setScreen (id, l1, l2)` or `setScreenText (id, line)`.

**A panel too big for its window scrolls instead of shrinking.** On-screen size
is `design x window / designWidth`, so a panel that grows takes its own legends
down with it and there is no arrangement of a fixed faceplate that survives it.
The megasynth hits this by adding modules and the HR-16 by being a box you look
down at rather than a rack front. Nothing changes for a panel that fits: width
still drives the scale, the same float it always was.

Two things a unit can turn on.

A **legibility floor**, `theme.minLegendPx` in the export or
`setLegibilityFloor (legendDesignUnits, screenPx)` from the editor. The scale
then stops falling at the point where the theme's smallest legend reaches that
many screen pixels, and the panel scrolls sideways instead of shrinking past it.
13 px is the recommended floor. Declare nothing and there is no floor, which is
why this is a no-op for all thirty-five units.

A **band**, which is the editor's half. Give the panel a rect shorter than its
own aspect and it scrolls vertically at the same scale rather than being
squashed. Both templates now work out once, from the display, whether this panel
can be fitted at all by choosing a width, and only cap the band when it cannot.
A machine deeper than it is wide is the case: the width is already floored at
920 and that floor alone puts the window off the bottom of the screen. Copy
`panelBand()` out of the template editor if your unit needs it.

`controlBounds (id)` returns viewport coordinates now. If you draw an LCD or a
scope into that rectangle you get the right pixels; if you cached it, stop.

**The background scales uniformly.** Rescaling it to `getHeight()` stretched the
printed matter to a different scale from the sprites drawn on top, and the labels
then sat under their own controls. Twelve copies had this fix and twenty-eight
did not.

## Renames you may hit

| the old per-unit call | now |
|---|---|
| `NativePanel()` | `NativePanel (SYNTHKIT_PANEL_ASSETS())` |
| `NativePanel ("fb1")` | `NativePanel (SYNTHKIT_PANEL_ASSETS(), "fb1")` |
| `setSegment (text)` (JD-800) | `setScreenText ("seg", text)` |
| `mainScreen()` returning an index (Nord Lead 3) | `mainScreenId()` |
| `isPlaceholder()` (TR-909) | `! hasArtwork()` |
| `screenBounds()` (CMI) | `controlBounds ("<the screen's id>")` |

Two per-unit habits are gone on purpose. `jassert (designW == 1840 && designH == 280)`
was the M1's numbers copied into twenty-four other units, where it fires on any
panel that is not the M1's; the size comes from the export now and there is
nothing to assert. And the hard-coded fallback background colour is read from the
export's own theme instead.

## What was merged in, and from where

Everything below was in one or a few copies and is now in all of them.

- the uniform background scale (ob8, jd800, tr909)
- write to the largest screen, not every screen (nordlead3, pcm90, superclassic)
- readout windows, and a readout that is pressed rather than turned (superclassic)
- the value arc on a knob, drawn live because knobs of one radius share a sprite
  (superclassic)
- horizontal faders, and a fader hit box that follows its actual slot (superclassic)
- `placements()` / `idAt()` / `devMoveHit()`, the draw-versus-click check
  (superclassic, andromeda)
- three-position toggles with a frame each, and the asset prefix for a repo that
  ships two machines (filterbank, filterfactory, ts22)
- rectangular lamps, and `static` printed matter that is not clickable (pcm90,
  andromeda)
- the seven-segment screen style (pcm90, tc2290)
- `controlBounds()`, for a menu that opens at the control the hardware puts it on
  (virus, mofx)
- a latching round button that exports both lamp states (mofx)
- surviving a missing export instead of failing to build (tr909, repeater)
- `setScreenLines()` for a display that is one four-field line (repeater)
- `debugDump()` (repeater, pcm80, pcm90, filterbank)

Dropped: the Andromeda's rewrite, which reached the same draw-versus-click check
by deleting faders, toggles, indicator rings, control groups, the mouse wheel and
the edit-echo timer. That is a loss, not a fix.

## Where this has got to

Thirty-five units are on the shared component. The current list of the ones that
are not is still `ls -d ~/projects/*/src/ui/NativePanel.h ~/projects/*/src/ui/SpecPanel.h`,
and as of 2026-09-10 it is ten:

- **andromeda** - tried, reverted, and worth reading before anyone tries again.
  Its own `controls_are_alive` gate fails on the shared component, and it is
  right to. The Andromeda's copy gave LEDs no hit area at all; the shared one
  makes them clickable, because on the MO-FX and others a lamp IS the button.
  Four controls then overlap: `osc1_saw` is drawn at 315,95 and a click there
  reaches `osc1.led.15`, and the same at `osc2_saw`. The LED row's own hit boxes
  overlap each other too, the lamps being about 13 design px apart with 5 px of
  expansion each side. That is the panel spec, not the component, and the panel
  was being authored at the time. Fix the spacing in the spec (or mark those
  lamps `static`), then migrate.
- **pcm90** - does not build before you touch it. `panel/` and `resources/` are
  both empty and untracked while CMakeLists still names
  `resources/native/background.png`, `layout.json` and `panel/bindings.json`, so
  `make` stops at PCM90Assets. Export the panel first.
- **filterfactory** - there is nothing to migrate onto. No CMakeLists.txt, no
  PluginEditor, just an orphan `src/ui/NativePanel.{h,cpp}` and two specs under
  `panel/ff` and `panel/fq`. Delete the copy when the unit gets its build.
- **cmi** - a bespoke read-only variant, not this lineage. Leave it.
- **superclassic** - uncommitted work in the tree (CMakeLists.txt, and an
  untracked `tools/drift_render.cpp`). Its readouts, live value arcs and
  horizontal faders are already merged in, so the migration itself should be
  ordinary once the tree is clean.
- **mofx, repeater, ts22, hr16, x0xb0x** - being worked on. Do not migrate a
  repo somebody is holding.

The recipe below covers the plain case. Six things came up often enough across
the thirty-five to be worth knowing before you start:

1. A unit whose `kPanelW`/`kPanelH` are not the spec's own aspect looks WORSE
   after the migration, not better, and the white band under the panel is the
   symptom. The old copy hid the mismatch by stretching the background to fill
   whatever rect it was given. Check `panel/panel.json`'s "size" against the
   editor's constants first: the K5 was laid out at 1840x364 with a 1800x176
   panel, the TX81Z at 1840x364 with a 1760x162 one, and both had legends
   stranded a hundred-odd pixels from their controls.
2. `PluginEditor.cpp` does not always include `BinaryData.h`. The deleted copy
   included it for itself on the TR-909 and the Filterbank.
3. The init list is not always on its own line, and the panel is not always
   default-constructed. The PCM 80 writes the whole list after the constructor's
   signature; the Filterbank already passes a machine prefix, so the assets go in
   front of it.
4. `src/ui/NativePanel.cpp` is not always on its own line in `target_sources`
   either. The M1's `M1TremDbg` and the PCM 91's sync probe both had it inline.
5. Delete the copy LAST and check that it went. The PCM 80's first migration
   stopped on the init list and left both files tracked, and everything built and
   passed anyway, which is precisely how a leftover copy survives.
6. A console app with no editor does not want `synthkitpanel`. The Emulator II's
   and the Synthex's clock gates link `synthshell` alone and should stay that way.

Two of the thirty-five were the fault in the flesh rather than a latent one. The
Synthex had a finished export in `resources/native/` that was never in the binary
data and a `SpecPanel` drawing the placeholder over it, which is how Ebbe und Flut
shipped. The K5 and the TX81Z had their printed matter at roughly twice the height
of the sprites drawn on it.
