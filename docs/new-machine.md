# Starting a new machine

```
python3 ~/projects/synthkit/tools/new_synth.py Jupiter8 --kind synth \
        --brand Roland --model "Jupiter-8"
cd ~/projects/jupiter8
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

That is a building, testing, sounding plugin with the whole shared shell, the
clock gate, the differentiation probe and the control audit already wired, before
a single byte of the machine has been decoded. Everything after this is
replacing stand-ins with findings.

## What you have on minute one

- the panel, the session strip (CHORDS / ARP / CHOPPER or AUDITION), the Patch
  Explorer, the keyboard row and the master clock, all from `synthshell`, none
  of it in your repo;
- the effect blocks from `dspblocks`, likewise;
- a parameter table and a factory bank as CSVs with a provenance column, and a
  generator that turns them into the C++ the engine reads;
- a coefficient builder with the cutoff written as an explicit SUM of named
  terms, several of them empty and marked `[TODO/FW]`, because the terms you will
  miss are exactly the ones with no patch byte behind them;
- four probes and seven ctest gates, each with a negative control;
- `docs/before-you-call-it-finished.md`, which is the half of the audit that
  needs a person.

## The order of work that held up

Playbook section 17, and it has survived four machines.

1. **The parameter layer.** Exact mapper and tables, round-trip tests, frozen
   parameter ids, a plugin that makes no sound. Prove the loader touches every
   byte the machine stores and exports the same set back out BEFORE writing any
   engine code, half a patch is no patch, and one machine's extension record was
   parsed as zeros for weeks while every effect it carried was chased as a
   separate bug.
2. **The voice engine.** "Correct structure, not yet right." Implement the FULL
   modulation matrix the day the offsets land, even if the panel exposes part of
   it.
3. **The effects.** They carry most of the character.
4. **The UI**, on the final artwork.
5. **Integration**: CC/NRPN map, sysex in and out, preset format.
6. **Polish**: unison, drift, glide, verification green.

Commit at every step so regressions bisect. Write the "findings easy to get
wrong" list into `CLAUDE.md` on day one and keep it at the top of the repo.

## What to replace, in order

Every stand-in is marked `TODO(new machine)` and says which playbook section
tells you how to find the real answer.

| file | replace with |
|---|---|
| `data/params.csv` | the real parameter table, ranges from the FIRMWARE's own arithmetic |
| `data/factory.csv` | the real bank, decoded from sysex or the ROM |
| `src/patch/Program.{h,cpp}` | the machine's own record layout, on most of these the sysex layout IS the RAM layout |
| `src/engine/*` | the machine's blocks and its laws, each with a provenance tag |
| `src/ui/ShellAdapter.h` | this machine's sync destinations and its own category list |
| `panel/panel.json` | the real panel, then `npm run panel:native`, the panel component then paints it, with nothing to swap |
| `tools/tests_main.cpp` | the frozen id list, the day the real parameter table lands |
| `CLAUDE.md` | this machine's non-negotiables |

## What never comes into the repo

- the shell's code, `synthshell` is a dependency;
- the DSP blocks, `dspblocks` is a dependency;
- the panel renderer, `synthpanel-creator` owns it, and a panel fix is made
  there and re-rendered here;
- an arpeggiator of the machine's own. The hardware arp is reverse-engineered for
  the record and never implemented in the plugin: the session strip's ARP is the
  one arp for every synth.

If you find yourself copying a file out of any of those three, stop. That is the
exact mistake FIX ONCE exists to prevent, and it is why thirty per-unit
`SessionStrip.cpp` copies had to be deleted.
