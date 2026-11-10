# Before you call it finished

`~/projects/eproms/tools/audit_units.py` is the machine-checkable half of this.
Run it. This is the other half, the questions that need a person, because the
answer is a judgement about the machine rather than a pattern in the source.

Every line below is here because it actually happened, on a real unit, and was
invisible to a green test suite. Work down it once, honestly, and write what you
find in `bugs.md` as you go rather than at the end.

---

## 1. The controls

- [ ] **The panel MATCHES THE ORIGINAL, exactly.** Same sections, same buttons,
      knobs, sliders and screens, in the same positions. This is a definition of
      done and not a preference: the panel is how you A/B against the real
      machine, so a rearranged one cannot verify anything, and a unit that
      cannot be verified is a guess with a build system. Measure each control's
      centre off the reference as a FRACTION of panel width and height and place
      ours at those fractions - proportional coordinates survive any window size
      and turn "does it match" into a number. Playbook 16.0.
- [ ] **Sections are SHADED**, with their own darker ground, border or titled
      tab, so a control's membership is visible without reading a label. While
      comparing against the real machine you are hunting one knob among forty,
      and grouping is what lets the eye skip three quarters of them. A flat
      field of evenly spaced controls is the hardest thing to navigate and it is
      what you get by default.
- [ ] **You RENDERED it and LOOKED at it**, at the width the window opens to and
      at a narrow one, and compared it against the reference control by control.
      SuperClassic shipped a panel whose own header read "placeholder" with a
      green build throughout, because nobody rendered it.
- [ ] **Every control on the panel moves the audio.** Not "is bound", MOVES THE
      AUDIO, measured end to end. `ctest -R controls_are_alive` does it; read its
      output rather than its exit code. 68 of the Reverb 4000's 189 rows were
      silent, 36 % of the machine, because its `bindings.json` was compiled into
      the binary and read by no code at all.
- [ ] **Every engine parameter is reachable from the panel**, or is on the
      declared unreachable list with a reason. The same unit had 11 that were
      not, including Attack, Hold and Release, which were already tempo-sync
      destinations, so the clock could drive a gate the user had no way to set.
- [ ] **A control that LIGHTS is not a control that CLICKS.** Check the write
      path separately from the read path. The TC 2290's five EXT EFFECTS buttons
      were lit from the engine state by the panel refresh and had no click
      handler: lamps correct, buttons dead, and the panel looked alive.
- [ ] **A control that visibly works is not evidence that the parameter behind
      it is wired.** The Supernova's Arp Speed knob had been re-bound straight to
      the tempo parameter, so turning it moved the tempo while the machine's own
      parameter, the one a host automates, a CC targets and a program stores,       stayed inert for a year. Check the PARAMETER, not the knob.
- [ ] **An off-by-one hits a RUN of controls, not one.** Find one wrong binding
      and check its neighbours immediately. The Reverb 4000's Decay page ran
      Lo / HiMid / Hi, skipping LoMid, in a four-multiplier sequence.
- [ ] **Ask of every global: does the front panel have this control?** If not, it
      must not exist. The H3500 had an invented global `mix` at 0.5 on a machine
      where each algorithm carries its own wet percentage, every preset played
      half wet, and fixing it moved the musical difference between presets by 11x.

## 2. The data

- [ ] **Every field of every stored program matches the engine's copy after
      load.** Assert the opposite of what you believe and print the count. 272 of
      the H3500's 625 presets loaded their algorithm's DEFAULTS instead of their
      stored values; on the PCM 91 the same assertion returned 0 and caught 383
      clamped values on the way past.
- [ ] **No stored value is clamped by its own parameter's range.** Take ranges
      from the FIRMWARE's arithmetic, never from a manual's rounded printed
      numbers. The Reverb 4000's grid is `20*2^(n/24)`, so its "500 Hz" is
      507.968; typing the printed numbers clamped 91 values across 174 programs.
- [ ] **Nothing decides behaviour from prose.** If the machine has a structured
      field for a property, a keyword, a category, a flag bit, use that field.
      The PCM 91's preset generator matched the user guide's English, and the
      sentence saying the pre-echoes were OFF is what switched them ON: 93 of 433
      reverbs measured as delays.
- [ ] **No value lives in two places.** A range in both a spec table and a
      parameter registration will disagree, and you cannot tell by reading which
      of the two is decorative. Third occurrence in one repo.
- [ ] **Every default computes EXACTLY its own neutral value.** Grep each one
      against its own curve. The PSR-730 had three at once, all documented
      neutral: brightness defaulted to 64 and computed 0.905, attack to 0 and
      computed 0.15, release to 64 and computed 2.70.
- [ ] **Every control is stored, live, or a selector, and it is written down
      which.** Only program data resets on load. Session state (solo, FX bypass)
      either auto-clears on a program change or is deliberately sticky, and the
      choice is recorded.
- [ ] **Unmapped bytes round-trip verbatim.** Export a program and re-import it
      and compare bytes. Most of a real record is unnamed on day one.
- [ ] **Index is not identity.** Find a program by NAME in every test. Program 0
      is the Init Prog on more machines than you expect, and a probe that indexes
      by number measures the wrong patch and reports confidently about it.

## 3. The filter, the cutoff is a SUM

The newest of these and the least internalised. It will be true of every machine
with a filter.

- [ ] **List every term in the cutoff sum, name it, and tag it.** A hardware
      filter's cutoff is not a parameter: it is a running sum computed per note,
      and only some terms are things a user can set. The M1's firmware summed
      FIVE words and our engine modelled three; the missing one was worth **2.3
      octaves at middle C**.
- [ ] **The terms you will miss are the ones with NO PATCH BYTE behind them.**
      Nothing in the record ever points at them. Hunt specifically for: a
      hardwired key follow separate from the patch's own key-track control (the
      M1's was 71 counts per key, three instructions away from the one users can
      set); a per-sample or per-zone signed trim; velocity into the EG's
      INTENSITY rather than its level; controller sweeps that sum in at zero and
      are therefore invisible at rest.
- [ ] **Read the COEFFICIENT BUILDER, not the parameter handlers.** Find the
      routine that writes the filter coefficient and account for every
      instruction that reaches its accumulator. If your engine sums fewer terms
      than the firmware, the difference is your bug and it is measurable.
- [ ] **Render the whole bank and look at where each cutoff settles.** This needs
      no disassembly. Factory banks do not ship silent programs, so if a
      meaningful share sit implausibly dark, or bunch against a limit, a term
      is missing. 47 of the M1's 126 active oscillator halves rested below 400 Hz
      at C4 under the three-term sum, and three sat on the filter's floor.
- [ ] **Check the SLOPE separately from the ANCHOR.** How many counts make an
      octave, and what frequency count zero means, are two different claims and
      they fail differently. Do not let a settled slope imply a settled anchor.

## 4. The clock

- [ ] **The unit declares its sync destinations, and `ctest -R follows_the_clock`
      measures them in the AUDIO.** Nine units shipped with correct division
      arithmetic and no clock reaching the engine, and every test was green
      throughout, because the tests checked the maths.
- [ ] **The check that catches a dead clock is a TEMPO change, never a division
      change.** The 2290 had 1/4, 1/8, 1/16, 1/8. and 1/8T all exact while
      reading 500 ms at 120 BPM and still 500 ms at 90.
- [ ] **Nothing writes the resolved tempo back into the unit's own BPM field.**
      `bpm.store(tempo)` each block makes SYNC INTERNAL the host's tempo. It looks
      perfect under every external measurement, because the write-back is what
      makes external work. Found in three units at once.
- [ ] **Two tempo pairs on the internal check, never one.** 120-against-90 passes
      a unit whose internal tempo happens to BE 90. Add the mirror at 150.
- [ ] **Every rate destination has an ABSOLUTE beat-alignment assertion as well
      as a period one.** A spacing measurement cannot see a phase error: a
      constant offset cancels out of the arithmetic entirely. The D-50's LFOs
      were green across the board and a block ahead of the music.
- [ ] **A stub playhead has an ADVANCING ppq.** A frozen position pins the clock.
      It is invisible to a delay, which wants a length, and obvious to anything
      wanting a position, so it presents as "the modulation is broken and the
      delay is fine".
- [ ] **The raw-pulse path is tested separately.** No playhead, just `0xF8` at 24
      per quarter, and Stop releases the lock.
- [ ] **A destination that cannot reach a division says so.** The Reverb 4000's
      pre-delay tops out at 300 ms, so a quarter at 120 BPM silently clamped
      while the header read SYNC 1/4.
- [ ] **The meter is checked against an absolute value.** Peak-picking an
      envelope reads about 2 % fast at every tempo; a meter wrong by a consistent
      percentage survives every cross-check that compares measurements to each
      other.
- [ ] **A machine with genuinely nothing that moves in time skips the clock gate
      DELIBERATELY, with the evidence written down**, never by accident.

## 5. The sound

- [ ] **`ctest -R bank_still_spreads` reports EFFECTIVE DISTINCT SOUNDS, and the
      number is close to the bank size.** 625 H3500 presets sat at 146; 707
      PSR-730 voices at 38. Both looked like DSP problems and both were data.
- [ ] **The excitation suits the machine.** A click gave a 2290 preset-to-preset
      difference of 0.0001 and a strings loop gave 0.0104, a hundred times,       because 15 of its 20 presets are under 60 ms and every modulation effect
      needs a signal to act ON. An instrument needs note patterns, not audio: one
      long held note, a sparse line, and one held chord are what discriminate.
- [ ] **Is it the right KIND of effect?** A phaser is a chain of allpasses with
      unevenly spaced notches and no delay anywhere; a flanger is a short swept
      delay whose notches are harmonically spaced; a chorus is longer and gentler
      with little feedback. The parameter layer looks perfect while the block is
      the wrong one.
- [ ] **Is the machine's envelope YOUR envelope?** The M1's pitch EG rises to a
      level and falls back to the played pitch, one descent. Mapped onto a
      shared four-stage envelope it climbed and then parked at its extreme for a
      whole extra segment: 4.2 seconds an octave sharp on one program, longer
      than the note. Model the shape; do not approximate it with the nearest
      available.
- [ ] **One byte feeding three consumers is a bug waiting.** All three of the
      M1's envelopes read the VDA's EG-time-velocity byte. Check every "shared"
      field is genuinely shared in the record.
- [ ] **The modulation matrix is implemented in full**, even if the panel exposes
      only part of it. Shipping one global depth where the hardware has five
      destinations by six sources produced "the sounds sound the same", "sounds
      weird" and "the mod wheel does very little" as three separate bug reports.

## 6. The gates themselves

- [ ] **Every check has been made to FAIL.** `ctest -R probes_fail_when_they_should`
      injects the defect each probe exists to catch, in process, and requires the
      probe to report it. A check that cannot fail is worse than no check: ours
      silently inverted once into passing nothing and failing eleven healthy
      units, with every build green.
- [ ] **The shared clock gate is run against a known-good host AND known-bad
      ones.** `ctest -R clock_gate_is_itself_checked`. A shared check that is not
      itself checked is a shared bug.
- [ ] **Injections never touch the tree.** Inject in process, or inject and
      revert from a saved copy with nothing between the two steps. A session that
      dies in the middle leaves a bug that looks identical to a real failure.
      That happened, and cost a diagnosis.
- [ ] **`ctest -R generated_headers_are_current` passes.** A CSV and a generated
      header disagree in silence: the Access Virus shipped for weeks with most of
      its modes unreachable because a header had not been regenerated.

## 7. The panel and the shell

- [ ] **Render the shell on the WIDEST and the NARROWEST machine.** Four shell
      bugs were invisible until something that is not a 1,720 px synth drew it.
- [ ] **Judge the panel at the width the window OPENS to**, not at the width of
      the render. Half the JP-8000's legends collided at the real width and none
      of them did in the big PNG.
- [ ] **The panel fix goes in `~/projects/synthpanel-creator`, never in this
      repo.** A local patch forks the renderer and every other machine misses it.
- [ ] **Nothing in this repo has its own SessionStrip, KeyboardRow, explorer, arp
      or audition source.** If you are about to copy a file out of `synthshell`
      or `dspblocks`, stop.
- [ ] **No performance control takes keyboard focus.** A BPM slider once silenced
      a whole synth by stealing focus from the on-screen keyboard.
- [ ] **The window-level key listener swallows every plain key.** On macOS an
      unconsumed keyDown reaches Cocoa and plays the system alert sound before
      every note.

## 8. What you leave behind

- [ ] `docs/laws.md`, every law with its provenance tag and a code reference.
- [ ] `docs/research.md`, the trail, with corrections marked withdrawn rather
      than deleted.
- [ ] `bugs.md`, one line per bug, written as it happened.
- [ ] `todo.md`, the debt ledger: every `[EAR]` constant with a firmware lead.
- [ ] `CLAUDE.md`, the non-negotiables for THIS machine: the findings that are
      invisible until something sounds subtly wrong and are easy to "correct"
      into a bug. Write yours the day you find them.
