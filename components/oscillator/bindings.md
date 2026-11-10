# Wiring the OSCILLATOR section

One line per local id. The gate reports per component, so a half-wired
instance fails the build instead of warning:

    OSCILLATOR (osc_)   11 of 11 bound   ok

| local id | wire it to | watch for |
|---|---|---|
| `osc1_wave` | a 7-position selector | the index order is firmware order, do not re-sort it for the panel |
| `osc1_ctrl1` | a knob | its LEGEND changes with the waveform, its parameter does not |
| `osc1_ctrl2` | a knob | same, and on noise this is resonance and not a level |
| `osc2_wave` | a 3-position selector | a fourth value clamps to the third, it does not wrap |
| `osc2_ctrl1` | a knob | dead on saw and triangle, by design, do not hide it |
| `osc2_ctrl2` | a knob | same |
| `osc2_range` | a semitone control | |
| `osc2_fine` | a knob | ONE byte carries fine and the WIDE switch together |
| `osc2_sync` | a switch with an LED | |
| `balance` | a knob | **its default is the CENTRE**, see below |
| `cross_mod` | a knob | ignore it when `osc1_wave` is SUPER SAW or NOISE |

## The balance default will cost you an afternoon if you skip this

`balance` defaults to 64, the centre, with both oscillators sounding. The first
oscillator sweeps made against this section's donor machine were all invalid
because OSC2 drowned OSC1 at that default, and the null result looked exactly
like a working measurement. If you are measuring OSC1, pin `balance` to 0
first, and prove it by watching OSC2's waveform character disappear.

## Turning the effects off is not where you think

On the donor the chorus and delay are silenced at their LEVEL parameters, not
at their TYPE and TIME. Two whole measurement runs were made against the wrong
pair, and the comparisons between waveforms survived it because every waveform
got the same wrong pins, while every absolute spectrum in those runs is
contaminated. If this section is being measured rather than played, pin the
levels.
