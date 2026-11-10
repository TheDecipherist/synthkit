# Wiring the FILTER component into a synth

Work down the table. When every row has a tick the component is wired, and not before.
This exists because "it builds and the tests are green" has twice meant "four knobs move
a parameter that reaches no audio".

## 1. Build

```cmake
include(cmake/Siblings.cmake)                       # finds synthkit and dspblocks
target_link_libraries(MySynth PRIVATE synthkit_filter)
```

`synthkit_filter` carries the include path and links `dspblocks`, so headers are spelled
`<filter/dsp/FilterModule.h>` and `<filter/ui/FilterScreen.h>`.

**Link it on every target that compiles the engine**, not just the plugin. MegaSynth found
this the hard way and reached for directory-scope `include_directories` instead, because its
Engine.h is compiled by the plugin, the standalone, the snapshot and about twenty probes,
"several of which get their include paths from neither synthkitpanel nor a hand-written
dspblocks line, so naming them one at a time missed six of them."

## 2. The twelve ids

Give the instance a `prefix` (a new synth: one line, every id free) or an explicit `map`
(a synth that has already shipped: pin what it shipped). **Never neither.** An id is
persisted in saved state, and re-spelling one silently drops the value out of every saved
patch and every host automation lane.

| local id | what the host must provide | done |
|---|---|---|
| `cutoff` | a control, **in Hertz** at this seam. A 0..1 knob converts in the adapter. | |
| `resonance` | 0..1. It is dB of lift at the corner: 0 dB to 24 dB. | |
| `shape` | 0..3 continuous. Response buttons JUMP this parameter, they are not their own. | |
| `width` | 0..1 | |
| `drive` | 0..1 | |
| `slope` | 12..24 continuous. Pole buttons JUMP this parameter to 24 and 12. | |
| `env_amount` | -2..2, bipolar | |
| `key_track` | 0..1. A key-track ON/OFF button zeroes or restores it. | |
| `type` | choice of 4: Super, SVF, Cascade, Twin | |
| `level` | unity at 1.0 | |
| `pan` | unity at 0.5 | |
| `bypass` | bool | |

## 3. The engine

```cpp
dsp::filter::CutoffTerms terms;      // sum everything IN CENTS, convert ONCE
terms.baseCents  = …;                // the patch's own cutoff
terms.patchKeyTrack = …;             // key_track * (note - 60) * 100
terms.envelope   = …;                // env_amount * env * 6000
terms.lfo        = …;                // + matrix, + anything else

if (fc.type == dsp::filter::kTypeTwin) { configureTwin (twin, sr, terms.hz(), …); … }
else                                   { filter.set (terms.hz(), res, shape, width,
                                                     drive, slope, fc.type);       }
```

- **Everything adds into the CUTOFF, never into the slope**, and the sum is converted once.
  Converting per term compounds rounding and loses the exactness the `std::round` is there
  for.
- **TWIN is a different call.** `configureTwin` maps the same seven knobs onto the A6 pair,
  and the host applies its own slope trim at the call site — on that topology alone SLOPE
  also raises level, because filter 2 sums on top of filter 1's taps.
- Gain staging between the summing node and the VCA is the HOST's, not the component's, and
  it must stay on the bypass path too or pressing bypass changes the level.

## 4. The screen

```cpp
dsp::filter::Screen screen { getter, setter };   // keyed by the LOCAL names above
screen.fillCurve   (dest, max);      // 256 points, 0..1 up the glass
screen.fillHandles (out,  max);      // six, the macro last and at 2.2x
screen.dragged     (name, x, y);     // x,y are 0..1 across and up
```

The getter and setter speak the component's units: **`cutoff` is Hertz here**, whatever the
host's parameter is. Convert in the lambdas, in one place, or the handles will not sit on
the curve.

Hit-test the handles by radius and give the rest of the glass **no** hit rect. A screen with
a default corner rect covers every control declared before it, and a click on the glass
flips a state nothing reads.

## 5. Prove it

- [ ] Every one of the six handles moves the parameter it names.
- [ ] The curve responds to all six controls it reads.
- [ ] Every control on the panel reaches a parameter — and the gate **fails** when one does
      not. A gate that classifies an unbound control as "not a parameter" and returns 0 is
      how dead controls ship.
- [ ] Audio is unchanged if you are putting the component back into the machine it came
      from. A component that cannot be put back without changing the sound has not been
      extracted, it has been rewritten.
