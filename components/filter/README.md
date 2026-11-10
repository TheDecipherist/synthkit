# The FILTER component

One filter section, assembled from six machines' best ideas and usable by any synth on the
rack. The DSP lives in `dspblocks`; this directory owns the parameters, the cutoff
arithmetic and the response screen.

```
component.json     the twelve local ids, with ranges and provenance
dsp/FilterModule.h CutoffTerms (the cents sum, converted once) and FilterCoeffs
ui/FilterScreen.h  the response curve and its six drag points, host-agnostic
bindings.md        the wiring checklist - work down it, tick every row
CHANGELOG.md       what a synth gains by updating its link
```

## Why it is one filter and not six

The six machines' signatures are at six DIFFERENT STAGES of a filter, so they stack instead
of competing: the control domain is the Nord Lead 3's, the seven-term sum is the
Matrix-1000's, the single conversion is the JP-8000's, the shared exponent curve is the
Supernova's, the mode weights are the Synthex's, the slope-as-gain-shift is the Nord's, and
the asymmetric limiter on the feedback term is the Virus's. That is why there is no model
selector: the machines are INGREDIENTS, not options.

What remains selectable is the **topology**, four of them, because they answer the one
question shape, width and slope cannot reach — what resonance does to the body — and they
answer it 7 to 22 dB apart.

| | |
|---|---|
| 0 SUPER | ours, the fused filter |
| 1 SVF | `[FW]` Nord Lead 2, two 2-pole state-variable stages in series |
| 2 CASCADE | `[SCH]`+`[DS]` the SSM2044, four one-poles with feedback to the input node |
| 3 TWIN | the Alesis Andromeda A6 pair, a 4-pole lowpass beside a 2-pole multimode |

## The seam

The screen knows no host: no JUCE, no parameter tree, just a getter and a setter keyed by
the component's own local names. That is what lets two machines with different patch
formats share one filter. **Cutoff is in Hertz at that seam** — a synth whose own control is
0..1 converts in its adapter, with its own law, and nothing in here changes.

## Reading further

`solera/docs/super-filter.md` is the design record with every law and its provenance.
`bindings.md` is what you actually work through.
