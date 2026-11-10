# OSCILLATOR component changelog

What a synth gains by rebuilding against this component today.

## 1.0.0 - 2026-10-07

First extraction, from the JP-8000.

- Two oscillators, 7 and 3 waveforms, eleven local ids. The generators stay in
  `dspblocks` (`dsp/JpOsc.h`); this component owns the word laws, the carrier
  with its sync edge, and the balance.
- The two words per waveform are firmware-exact and each read twice. The second
  reading was a COUNT of the firmware's own control reads, not a decode, after a
  linear disassembly from an arbitrary address desynced and produced a plausible
  dispatch that was fiction.
- The super saw detune table is handed in, never stored here, so no ROM bytes
  enter `synthkit` and the component is not pinned to one firmware revision.
- Carries two withdrawn findings in place rather than silently corrected: noise
  `ctrl2` is resonance and not a level, and SAW ignores both controls.
