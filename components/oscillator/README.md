# The OSCILLATOR section

Two oscillators with seven waveforms on the first and three on the second, two
controls whose MEANING changes with the waveform, hard sync, a balance, and
cross modulation that two of the waveforms do not have. Lifted out of the
JP-8000, whose oscillator is the best one in this workspace because it is the
one that has been read out of firmware rather than fitted by ear.

The waveform generators are in `dspblocks` (`dsp/JpOsc.h`). What is here is the
section's own arithmetic: the laws that turn two 0..127 panel controls into the
words the chip is given, the two-oscillator carrier with its sync edge, and the
balance.

## The one thing to understand before wiring it

**A control's meaning changes with the waveform and its curve does not.**
`ctrl1` and `ctrl2` are two bytes carrying thirteen names between them:

    DETUNE / MIX                super saw
    OFFSET / SHAPE              triangle mod
    FLT FREQ / FLT RESO         noise
    FEEDBK AMT / HARMONICS      feedback oscillator
    PULSE WIDTH / PWM DEPTH     square

Declaring those as thirteen parameters is the mistake. The machine has two, and
a panel that shows a different legend per waveform is showing the same
parameter. That is why this component declares eleven ids and not twenty-three.

## Where the law came from

The two words per waveform are firmware-exact, and each has been read twice.
The second reading was made by counting the firmware's own control reads rather
than decoding it, after a linear disassembly from an arbitrary address desynced
within a few instructions and produced a plausible dispatch that was fiction.
Counting needs no decode: five strict `ctrl1`-then-`ctrl2` pairs appear in
waveform-index order, and the third pair is anchored by carrying the machine's
own cutoff and resonance tables, which makes it NOISE, which is index 2.

| waveform | ctrl1 | ctrl2 | address |
|---|---|---|---|
| super saw | `(table[c1] >> 1) + 2` | `64*c2 + 400` | `0xA5B0` |
| triangle mod | `(127 - c1) << 8` | `c2` | `0xA5E8` |
| noise | `100*c1 - 1292` cents | from the RESONANCE table | `0xA610` |

The noise colour law is the one measured against an independent implementation
of the same firmware: the table says 100 cents per step and a ten-point sweep
read 100.5, agreeing to 0.5%.

Two results that were WITHDRAWN rather than quietly fixed, because the next
reader needs to know the ground moved. Noise `ctrl2` was called a level and it
is RESONANCE: level rises 17.7 dB while spectral flatness halves, 0.0065 to
0.0026, and a gain cannot do that. And SAW was reported as responding to both
controls, measured at +2.7 dB and +9.6 dB, from a rig that had turned the
effects off at the wrong two offsets and parked the filter on its clamp. Under
clean isolation SAW ignores both.

## The table stays with the unit

The super saw needs a 128-entry detune table and it is **handed in**, never
stored here:

```cpp
synkit_osc_setup.detuneTable = tables::supersaw_detune;   // the unit's own
```

`~/projects/eproms` holds the dumps, a derived table belongs to the unit that
derived it, and a formula belongs here. Embedding the 128 words would break that
rule and would also pin this component to one firmware revision. Keep the
entry-103 dip if you regenerate it: it is in the ROM and it is not a
transcription error.

## Consuming it

```cmake
include(cmake/Siblings.cmake)
target_link_libraries(MySynth PRIVATE synthkit_oscillator)
```

Then an instance, and `bindings.md` is the checklist. A new synth writes a
prefix; a synth that has already shipped its ids pins them with a map, because
a parameter id is persisted and must never be re-spelled.

## What it does NOT cover

The pitch envelope and the LFOs. They reach the oscillator and they are not of
it, and the ENVELOPE component is the right owner for the first. Ring
modulation is declared only as far as the balance: ring replaces OSC2 in the
mix rather than adding to it, and the ring product itself is the unit's.
