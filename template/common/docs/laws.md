# Laws

Every derived law, formula and constant, with **provenance** and a code
reference, so the document and the code can be diffed. `docs/provenance.md`
explains the tags.

Check this file before deriving or changing any parameter law. It has already
caught wrong conclusions on other machines; the research behind it is in
`docs/research.md` and is not to be deleted.

---

## 1. Mapping and tables

*(nothing yet)*

<!-- The shape this takes, from another machine:

### 1.1 The mapper divisor is 253

`mapParam(v) = v * range / 253`, not 255 and not 256. `[FW]` at 0x4A12, and
every factory patch depends on it. `src/param/Mapper.cpp`. There are tests that
fail if you change it; that is deliberate.
-->

## 2. Oscillators

*(nothing yet)*

## 3. Filter

### 3.1 The cutoff is a sum

`src/engine/Engine.h`, `CutoffTerms`. Currently:

| term | value | provenance |
|---|---|---|
| base | the patch's own cutoff | `[REF]` |
| patch key track | 1 semitone per key at full | `[REF]` |
| hardwired key follow | **0 — unmodelled** | `[TODO/FW]` |
| envelope | depth x env x 60 semitones | `[REF]` |
| velocity into EG intensity | **0 — unmodelled** | `[TODO/FW]` |
| LFO | depth x lfo x 24 semitones | `[REF]` |
| zone trim | **0 — unmodelled** | `[TODO/FW]` |
| controllers | **0 — unmodelled** | `[TODO/FW]` |

A zero in an unmodelled row is a CLAIM, not a default. The slope
(`kSemitonesPerOctave`) and the anchor (`kAnchorHz`) are two separate claims and
fail differently: do not let a settled slope imply a settled anchor.

## 4. Envelopes

*(nothing yet)*

## 5. LFOs and modulation

*(nothing yet)*

## 6. Clock and sync

The division ladder, the 24 PPQN grid and the three-state model are all
`shell::TempoSync` and `shell::ShellHost` — shared, and documented in
`~/projects/eproms/docs/midi-law.md`. What belongs HERE is only what is this
machine's.

**TWO ANSWERS, AND THEY ARE DIFFERENT QUESTIONS.** Fill in both. Read
`midi-law.md` §0 before you do: a digital unit has an AUDIO clock (44.1/48 kHz,
Word Clock, Digital In) and a TEMPO clock (MIDI `F8` at 24 PPQN), nearly every
"Clock" in a 90s manual is the audio one, and conflating them has cost this
project real time in both directions.

### 6a. What the HARDWARE did

| question | answer | provenance |
|---|---|---|
| Does it receive MIDI real-time (`F8`)? | not yet checked | |
| Named tempo source or sync switch? | not yet checked | |
| Any parameter in NOTE VALUES (1/4, 1/8T)? | not yet checked | |
| What did the machine itself slave? | not yet checked | |

Do not search the manual for "clock". Search the MIDI implementation chart's
System Real Time row, **read off a rendered page and never from OCR**, and
search the firmware for `Tempo`, `BPM`, `SYNC`, `MIDI CLK` and note values. A
box with tempo sync always has at least one parameter in note values or a named
tempo source; a box without one never does. `strings` on the ROM answers this in
one call, and on the SDE-330 it would have found `MIDICLK DELAY` on day one
instead of after the unit shipped.

If this section still says "not yet checked", the machine has not been checked.
It has been assumed.

### 6b. What OUR unit slaves

Section 6 of `midi-law.md` is the law: every unit follows an external clock
whatever its hardware could do. So this table is usually LONGER than 6a, and
that is correct and deliberate. It is not evidence about the hardware.

| destination | kind | machine's own name | range | provenance |
|---|---|---|---|---|
| — | — | — | — | not yet decoded |

## 7. Effects

*(nothing yet)*

## 8. Withdrawn

Corrections stay in place, marked withdrawn rather than deleted. A withdrawal is
an attribution too and needs the same evidence as the claim it replaces.

*(none yet)*
