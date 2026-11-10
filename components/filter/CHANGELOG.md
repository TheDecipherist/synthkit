# Changelog

## 1.1.0 — 2026-09-27

**Changed**
- The MACRO handle no longer writes `slope`. It wrote seven controls; it writes six.

  The author, on Solera: *"when I drag the main morph point on the filter screen the 4-pole or
  2-pole if selected turns off. they shouldnt be affected."* The reason is the one
  already written here for `key_track`: pole count is a property of WHICH filter you are
  using rather than of the sound it makes. It is also the only parameter the macro
  touched that a panel gives DETENTED BUTTONS, so a drag left the slope at 18 dB/oct
  with both lamps dark - which reads as the morph having switched something off rather
  than as it having moved a continuous control.

  A consuming synth whose macro drags were expected to change the slope will notice;
  nothing else moves, and the SLOPE knob and the pole buttons are unaffected.

## 1.0.0 — 2026-09-26

First declared version. The code was lifted out of MegaSynth on 2026-09-26; this is the
release that makes it a component rather than two loose headers.

**Added**
- `component.json` — the twelve local ids with ranges, defaults and provenance, and the
  screen's six handles with what each one writes.
- `bindings.md` — the wiring checklist a consuming synth works down.
- `README.md`, this file.
- `ui/FilterScreen.h` committed. It had been on disk and untracked since the lift, which
  meant the registry claimed a screen that was not in the repository.

**Known gaps**
- **MegaSynth still owns a duplicate of the screen.** `PluginEditor.cpp:3740-3994` and
  `4160-4213` are line-for-line the same arithmetic against MegaSynth's own shipped ids
  (`f_shape`, `f_width`, `f_drive`). Moving it onto this component is the point of the
  component, and it has not been done. Until it is, a change here reaches nobody.
- **No live waveform under the curve.** `db()` is a display model of the magnitude
  response: it cannot show the drive folding a wave over or the resonance ringing. The
  waveform needs a tap of the real output, and where that tap enters without breaking the
  Get/Set seam is undecided. Specified in `solera/docs/filter-swap-checklist.md` section 6.
- **No bindings gate.** `bindings.md` is a checklist a person works down. The per-component
  report described in `synth-component-library.md` is not implemented.
