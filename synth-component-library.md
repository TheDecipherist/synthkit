# The synth component library

A component is a whole module of a synth, lifted out of the machine it was
built in so the next machine links it instead of rebuilding it. Not a DSP
block and not a panel widget. The FILTER, with its filter types, its knobs,
its response screen, its drag points and the law behind all of them.

The author, 2026-09-26, asking for this:

> "When we use it in another synth it should be as easy as pull it in and wire
> up the new synths buttons and it just works. And if we are modifying it in
> another synth, it should be added to the global module itself so all other
> synths that are using it would be able to update their filter link and
> instantly on their next build have the exact same new features."

So a component is not a copy. A synth LINKS it, the way it already links
`synthshell` and `dspblocks`, and an improvement made while working on any one
machine reaches every machine on its next build. There is no version to pick
and nothing to opt into. That is the same doctrine `cmake/Siblings.cmake`
already states as DEPEND, NEVER COPY, applied one level up: a block is a
filter, a component is the FILTER SECTION.

## The registry

Adding a component is a ROW. Everything under the table is generic, which is
the rule `synthshell/cmake/SynthShellFxUnits.cmake` proved first and said in
its own header: if adding one needs code below the table, the table is wrong.

| component | what it is | DSP owned by | panel and UI owned by | local ids | in |
|---|---|---|---|---|---|
| `filter` | Four topologies (super, svf, cascade, twin), a continuous LP/BP/HP/notch morph, width decoupled from resonance, a Virus limiter in the feedback path, a response screen with six drag points | `dspblocks` (`dsp/SuperFilter.h`, `dsp/TwinFilter.h`) | `synthkit/components/filter/` | 12 | megasynth, solera |
| `piano` | A multisampled acoustic piano: the loader with its five recording laws (onset trim, one calibration gain, a common cap with a fade, outlier retirement, outward search for a missing key) and nine controls over the samples - stretch tuning, decay, damper, hammer hardness and position, soundboard, unison, two-stage release | the loader is the component's own (`dsp/PianoSamples.{h,cpp}`); the MODELLED piano is `dspblocks` (`dsp/PianoString.h`) and a different instrument | `synthkit/components/piano/` | 9 | solera |
| `oscillator` | Two oscillators, 7 waveforms on the first and 3 on the second, two controls whose MEANING changes with the waveform (thirteen names across two bytes), hard sync, a balance, and cross modulation that two waveforms do not have | `dspblocks` (`dsp/JpOsc.h`) | `synthkit/components/oscillator/` | 11 | jp8000 (donor, not yet wired) |
| `envelope` | **NOT A COMPONENT YET - the SCREEN only** (`ui/EnvScreen.h`): no `component.json`, no `README.md`, no probe, and deliberately NOT in `Siblings.cmake`'s component list, so there is no `synthkit_envelope` target and nothing links one. It is reached through the include path another component's target provides, which works and is exactly how the filter looked before it became a component. Finishing it is in `todo.md` | n/a | `synthkit/components/envelope/` | 0 | solera (`Source/SoleraEditor.h`) |

## What one looks like on disk

```
synthkit/components/filter/
  component.json     the module TYPE: params, wells, rows, items, local ids
  dsp/               the coefficient half that is NOT a reusable block
  ui/                the screen and its drag points
  bindings.md        the wiring checklist, one line per local id
  README.md          what it is, what it sounds like, where its law came from
  CHANGELOG.md       what a synth gains by rebuilding against it today
```

The pure DSP does not live here. It lives in `dspblocks`, which is the declared
owner of DSP, and the component references it. Putting a second copy in
`synthkit` would be the mistake that cost thirty `SessionStrip.cpp` copies.
What DOES live in a component's `dsp/` is the arithmetic that is specific to
the module but is not a block: for the filter that is the cutoff sum and the
coefficient struct, two things the filter block deliberately does not own.

## The ID law, and the mistake it exists to prevent

An id is the only thing joining the panel plan, the parameter tree, and the
host's saved automation in every project a player has ever made. Rename one and
nothing fails. The plate still renders, the export still writes, every gate
stays green, and the fault arrives days later as "this knob does nothing".

So a component declares **local** ids, and each instance says what those map to
in the synth that placed it.

```json
{ "type": "filter", "index": 2, "prefix": "f2" }
```

`prefix` generates the map: local `cutoff` becomes `f2_cutoff`, local
`resonance` becomes `f2_resonance`, and two instances in one synth cannot
collide. That is all a NEW synth ever writes.

A synth that has already SHIPPED its ids pins them instead:

```json
{ "type": "filter", "index": 1, "map": {
    "cutoff": "cutoff", "resonance": "resonance", "key_track": "key_track",
    "shape": "f_shape", "width": "f_width", "drive": "f_drive" } }
```

This half is not a convenience. It is the only correct answer, because a
parameter id is persisted and must never be re-spelled once it has shipped.
MegaSynth's filter carries bare ids (`cutoff`, not `f_cutoff`) and it has to
keep them: its stored programs are keyed by parameter id, so a rename is a bank
migration, and its saved automation is keyed by parameter INDEX, where the
damage is worse. That has been measured three times in this codebase. A commit
on 2026-09-10 moved 95 parameters, 178 moved on 2026-09-22 and made every
stored program load onto the wrong controls, and 970 of 1017 moved when six
kill switches were declared inside a module that is instantiated three times.

Two rules follow, and the generator enforces both:

1. **An instance carries a `prefix` or a `map`, never neither.** The old
   behaviour, an empty prefix meaning bare ids, is what let a second instance of
   the same module redefine the first.
2. **Every local id the component declares appears in the resolved map.** An
   unmapped one is a build error naming it. That is the checklist.

### Where the prefix goes, and the one place it must not

| namespace | prefixed | why |
|---|---|---|
| plugin parameter id | yes | JUCE looks up by string; two `cutoff` parameters is a duplicate-ID assert |
| panel control id | yes | one flat set per spec, no module scope (`validate.js:22`) |
| panel furniture id | yes | `name_cutoff`, `tie_cutoff`, `blk_filter`. Two FILTERs give two `name_cutoff`, and this is the collision that shows up first |
| bindings map key | yes | same string as the control, by construction |
| generated C++ identifier | yes | `kFields_` with no prefix means the second module redefines the first |
| asset filename shipped into `BinaryData` | yes | JUCE discards the path and mangles the basename, so two `layout.json` files cannot coexist in one target |
| **baked sprite name** | **no** | keyed by APPEARANCE on purpose (`native.js:322`), so two identical knobs share one image. Prefixing gives every instance its own copy of the same picture |

## The bindings checklist

`component.json` knows every control the component declares, so the generator
writes a `bindings.expected.json` per instance and the gate reports per
component rather than per control:

```
FILTER (f_)     12 of 12 bound   ok
FILTER (f2_)     9 of 12 bound   MISSING: f2_key_track, f2_drive, f2_width
```

It fails the build. A warning is what "every gate was green" looked like the
last three times.

## Adding a component

1. Pick the machine that has the best version of the module. The best one, not
   the most recent.
2. Move the pure DSP to `dspblocks` and leave the module-specific arithmetic
   in the component's `dsp/`.
3. Write `component.json` with LOCAL ids, the ones that read right with no
   prefix on them.
4. Give the donor machine an instance with an explicit `map` pinning the ids it
   already shipped, and prove the audio is bit-identical.
5. Add the row to the table above, and a line to `CHANGELOG.md`.

Step 4 is the one that matters. A component that cannot be put back into the
machine it came from without changing its sound has not been extracted, it has
been rewritten.

## Consuming one

Two lines of CMake, and the component's own `bindings.md` to work through:

```cmake
include(cmake/Siblings.cmake)
target_link_libraries(MySynth PRIVATE synthkit_filter)
```

Then an instance in the unit's module declaration, and the checklist gate tells
you when the wiring is done.
