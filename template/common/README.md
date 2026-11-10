# {{DISPLAY}}

A native reimplementation of the {{BRAND}} {{MODEL}}, built from
`~/projects/synthkit`. VST3 and standalone, macOS and Windows, C++20 on JUCE.

**Status: v0.1, skeleton.** It builds, it makes a sound, and every gate is wired
— but the engine is a generic stand-in and no law in it is this machine's yet.

## Build

```
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

Needs `~/projects/synthshell` and `~/projects/dspblocks` beside this repo. They
are dependencies, not code to copy.

## Layout

```
data/            the parameter table and the factory bank, as CSVs with a
                 provenance column. The single source for every range.
tools/           gen_headers.py (CSV -> C++), and the four probes
src/generated/   what the generator writes. Never edited by hand.
src/patch/       the program record, and the sysex both ways
src/engine/      the blocks, and the coefficient builder that feeds them
src/ui/          the shell adapter and the panel
panel/           the panel spec and the control bindings
docs/            laws.md, research.md, provenance.md, the checklist, the gates
```

## Where the work goes

`docs/new-machine.md` for the order of work. `docs/before-you-call-it-finished.md`
before calling anything done. `CLAUDE.md` for the non-negotiables.
