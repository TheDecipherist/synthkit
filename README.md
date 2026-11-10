# synthkit

The skeleton every new machine is created from.

A synth or effects unit that the author starts today already gets its whole bottom
half, the session strip, the Patch Explorer, the keyboard row, the audition
source and the master clock, from `~/projects/synthshell`, and its effect blocks
from `~/projects/dspblocks`. What it did NOT get was everything around them: the
CMake, the directory layout, the probes, the test gates, the docs structure and
the discipline. Every machine reinvented those, and the same faults kept arriving
in the same order.

This is that other half. **A new machine starts with the clock gate, the
differentiation probe and the control audit ALREADY WIRED, so today's failures
are structurally impossible rather than merely documented.**

```
python3 ~/projects/synthkit/tools/new_synth.py Jupiter8 --kind synth \
        --brand Roland --model "Jupiter-8"
cd ~/projects/jupiter8
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build
ctest --test-dir build --output-on-failure       # 11 gates, all green
```

## What synthkit is NOT

It is not the shell. `~/projects/synthshell` is the bottom module and synthkit
**depends** on it; so does every unit synthkit creates. If you find yourself
copying a file out of `synthshell`, `dspblocks` or `synthpanel-creator`, stop:
that is the exact mistake FIX ONCE exists to prevent, and it is why thirty
per-unit `SessionStrip.cpp` copies had to be deleted.

| package | owns |
|---|---|
| `synthshell` | the session strip, transport header, Patch Explorer, keyboard row, audition source, master clock, clock conformance gate |
| `dspblocks` | delay, pitch, modulation, filters, drive, reverb, dynamics, sampler, and `dsp/Measure.h` |
| `synthpanel-creator` | panel rendering: JSON spec in, SVG/PNG/native export out |
| **synthkit** | the CMake, the layout, the probes, the gates, the docs, `new_synth.py`, and the JUCE component that PAINTS the panel export |

## What is in it

**One panel component, `panel/NativePanel.{h,cpp}`.** It paints
synthpanel-creator's native export, and until that export exists it paints the
SPEC instead, plain shapes at the spec's own coordinates with "panel artwork
pending" across its face. Same ids, same events, same hit areas either way, so a
unit is playable and its bindings are testable from the first commit and nothing
is swapped when the artwork lands. It used to be forty per-unit copies that had
drifted apart, and one unit shipped a finished panel spec with the placeholder
still on screen because nobody wired a consumer at all. `MIGRATION.md` says what
a unit does to adopt it and what was merged in from which copy.

**Two working template units.** `template/synth` (ExampleSynth) and
`template/effect` (ExampleFx). Both build, both sound, both pass all their gates
as they stand, a skeleton nobody can run rots. The two differ in exactly the
three things the shell cares about: `SessionStrip::Layout`, whether there is a
keyboard row, and `offersSyncOff()`.

**Four probes**, each generalised so a new unit only fills in specifics:

- `sync_probe`, measures the clock in AUDIO at 120/90/140 BPM, the raw-`0xF8`
  path, the internal-versus-host check WITH its mirror (120 and 150 against a
  host at 90), and an absolute beat-alignment assertion for rate destinations.
- `spread_probe`, renders the whole bank and reports effective distinct sounds,
  twins and the median difference, with a regression floor.
- `controls_probe`, drives the real editor, moves every control end to end,
  renders audio at both ends and compares. Catches dead controls and the inverse.
- `snapshot`, renders the panel to a PNG.

**Four things the Synthex found in these templates (2026-09-09), now fixed
here:** the controls probe rendered ONE note, so every control about what
happens BETWEEN notes - portamento, glide mode, an LFO's restart-on-note -
measured as dead wiring while working perfectly; it now plays a second note. The
frozen parameter-id list is the template's until somebody replaces it, and now
says so where it is defined. The sync probe assumes the machine HAS a delay, and
an analogue polysynth may have no time parameter but an LFO delay - the Synthex's
onset measurement is written up there as the worked alternative. And the
bank-darkness check assumes a LOW-PASS: on a multimode filter a low cutoff in
high-pass is the filter wide open, which reported four correct programs as a
missing cutoff term.

**Every probe fails when it should.** Each ships a `--selftest` that injects the
defect it exists to catch, in process, and requires itself to report it. A check
that cannot fail is worse than no check: ours silently inverted once into passing
nothing and failing eleven healthy units, with every build green.

**The gates, wired into ctest from commit one:** `shell::conformance::checkClock`
run against a known-good host and known-bad ones; the
stored-values-reach-the-engine assertion (every field of every program against
the engine's copy after load); the four probes; and a regenerate-and-diff check
on the generated headers.

**`tools/new_synth.py`**, takes a name and a kind, copies the skeleton, renames
everything consistently, writes the docs, makes the first commit and prints the
next steps.

**The docs skeleton**, pre-filled with structure and the provenance tag table:
`docs/laws.md`, `docs/research.md`, `bugs.md`, `todo.md`, and a `CLAUDE.md`
carrying the non-negotiables that apply to every machine.

**`docs/before-you-call-it-finished.md`**, the checklist. The audit at
`~/projects/eproms/tools/audit_units.py` is the machine-checkable half; this is
the half that needs a person.

**`docs/sound-design.md`**, the sound-design laws that are not one machine's.
Distortion (why a soft clipper's knob is finished before its travel is, and why an
asymmetry has to be a bias rather than a curve), exclusive control groups and the
phantom selection a default-on member makes, the law that every module declares whether each of its
time bases follows the clock, and how to measure a block instead of auditioning it. Everything in it was got wrong on a real unit first, measured, and
promoted here so the next machine starts past it. A machine's own constants stay in
its `docs/laws.md`; this holds the rules above them.

## The template encodes no machine's specifics

No Supernova constants, no Lexicon vocabulary. Every place a real unit would put
a machine fact, the template has a clearly-marked `TODO(new machine)` with a
pointer to the playbook section that says how to find it. One of them is
structural rather than a comment: `CutoffTerms` in `src/engine/Engine.h` writes
the filter cutoff out as an explicit SUM of named terms, several of them empty
and tagged `[TODO/FW]`, because a hardware cutoff is a running sum computed per
note, and **the terms you miss are exactly the ones with no patch byte behind
them.** On the M1 the missing one was worth 2.3 octaves at middle C.

## Layout

```
CMakeLists.txt        builds both templates and runs their 22 gates
cmake/                Siblings.cmake (find the dependencies), GenVersion,
                      CheckGenerated - copied into every new unit
panel/                NativePanel.{h,cpp} - the ONE panel consumer, linked as
                      `synthkitpanel`, never copied into a unit
tools/new_synth.py    the thing you actually run
docs/
  new-machine.md      the 20-minute start, and the order of work
  the-gates.md        what each gate catches and how it is proved to work
  before-you-call-it-finished.md    the checklist
  provenance.md       the tag table and the rules that go with it
template/
  common/             CLAUDE.md, README, bugs.md, todo.md, docs/laws.md,
                      docs/research.md, .gitignore, package.json
  synth/              ExampleSynth, complete
  effect/             ExampleFx, complete
```

## Building synthkit itself

```
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

22 tests, eleven per template. Needs `~/projects/synthshell` and
`~/projects/dspblocks` in the workspace. JUCE is fetched.

## Where the reasoning lives

`~/projects/eproms/reverse-engineer.md` is the playbook this encodes, sections
14-15 (gates and deliverables), 16 and 19 (the panel and the shared shell),
17/17a/17b (engine lessons, the control audit, the cutoff-is-a-sum rule) and
18/18a/18b (the clock). `~/projects/eproms/docs/midi-law.md` is clock and sync in
full. Read those for WHY; this repo is the HOW, already wired.

GPL-3.0, to match its siblings.
