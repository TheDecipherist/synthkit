// The FILTER component's module arithmetic: the cutoff SUM and the coefficient
// struct. MOVED OUT OF MEGASYNTH VERBATIM, 2026-09-26, with the namespace changed
// from `megasynth` to `dsp::filter` and nothing else touched.
//
// WHY THESE TWO ARE HERE AND NOT IN dspblocks. The filter BLOCK
// (dsp/SuperFilter.h) deliberately owns no parameters: "no patch, no parameters,
// no branches on a program - the processor computes every coefficient and this
// holds blocks, which is the split the hardware had." So stages 1-4 of the
// filter's seven, the arithmetic on the way IN, have always lived outside the
// block. They are the MODULE, which is what a component is, so they live with the
// component. Stages 5-7 are the block and live in dspblocks.
//
// WHAT STAYS IN THE HOST SYNTH. Reading a patch is not the component's business.
// A machine fills `FilterCoeffs` from its own program format - megasynth does it
// in `makeCoeffs (const Program&)` - and the component never learns what a patch
// looks like. That is the seam that lets two synths with completely different
// patch formats share one filter.
#pragma once
#include <cmath>

namespace dsp { namespace filter {

/** THE CUTOFF IS A SUM, and the term you miss has no parameter behind it.
    (Playbook 17b; the single most valuable finding of the M1 work.)

    A hardware filter's cutoff is not a parameter. It is a running sum of terms,
    computed per note, and only SOME of them are things a user can set. On the M1
    the firmware summed five words and our engine modelled three; the missing one
    was an unconditional keyboard follow of 71 counts per key that no program
    byte can switch off, and it was worth 2.3 OCTAVES at middle C.

    The terms you will miss are exactly the ones with no patch byte behind them,
    because nothing in the record ever points at them. So this struct makes the
    sum EXPLICIT and an unaccounted term a visible gap rather than an absence.
    Every field is in semitones, so the whole sum is one exponential away from a
    frequency, and every field has to be named, sourced and tagged.

    BUILD-SPEC section 9: what makes a filter feel alive under the hands is HOW
    MANY THINGS ADD INTO THE CUTOFF, not the slope. The Matrix-1000's is a
    seven-term sum built by a firmware compiler. This one has five terms live and
    three still owed. */
struct CutoffTerms
{
    // EVERY TERM IS IN CENTS, AND THE SUM CONVERTS ONCE. [FW] JP-8000
    // 0xB6A0-0xB830 sums every pitch contribution as one 32-bit CENTS total and
    // converts it exactly once; [FW] its filter does the same thing at 0x1D754,
    // where the summed cents become `clamp(32 * 2^(cents/1200), 0x21, 0x7FFF)`
    // and the 0x7FFF IS Nyquist. Working in cents to the last moment is what
    // makes every modulation musical - an envelope worth "two octaves" is 2400,
    // key tracking is a ratio, and an LFO sweeps intervals rather than hertz -
    // and converting early throws the arithmetic away. `src/engine/Pitch.h`
    // carries the same discipline for the oscillators; this is the filter's half
    // of it, and the two now differ in nothing but which sum they are.
    float baseCents       = 0.0f;   ///< the patch's own cutoff, in cents above kAnchorHz
    float patchKeyTrack   = 0.0f;   ///< the key-track control the USER can set
    float hardwiredKeyFollow = 0.0f;///< [TODO/FW] the one with no parameter. The M1's was
                                    ///  71 counts per key, three instructions away from and
                                    ///  entirely separate from the control above. A 0 here
                                    ///  is a CLAIM, not a default.
    float envelope        = 0.0f;   ///< filter envelope x its depth
    float velocity        = 0.0f;   ///< [TODO/FW] velocity into the EG's INTENSITY
    float lfo             = 0.0f;   ///< sums in at zero, so it is invisible at rest
    float zoneTrim        = 0.0f;   ///< [TODO/FW] a per-zone signed trim
    float controllers     = 0.0f;   ///< [TODO/FW] wheel / aftertouch / breath sweeps

    [[nodiscard]] float total() const noexcept
    {
        return baseCents + patchKeyTrack + hardwiredKeyFollow + envelope
             + velocity + lfo + zoneTrim + controllers;
    }

    /** THE CUTOFF CONTROL IS A NOTE NUMBER, and that is stage 1 of the super
        filter. [FW] Clavia Nord Lead 3: its cutoff knob is a MIDI note number
        offset by eight semitones, `f(v) = 13.75 * 2^((v-1)/12)`, which fits the
        machine's own 128-entry table to 4.0e-07 over all 128 entries. So count
        zero here is `midiToHz(8)` = 12.978 Hz - which is what the Nord's panel
        legend prints as "12Hz", rounded.

        Playbook non-negotiable 3 in its strongest form: cutoff does not merely
        share the SHAPE of the pitch curve, it IS the pitch curve. Give the
        filter its own anchor or its own span and it stops tracking the
        oscillators the way the original does.

        THE ANCHOR AND THE SLOPE ARE TWO DIFFERENT CLAIMS AND THEY FAIL
        DIFFERENTLY: how many counts make an octave, and what frequency count
        zero means. */
    static constexpr float kAnchorHz = 12.9782717f;        // [FW] Nord = midiToHz(8)
    static constexpr float kCentsPerOctave = 1200.0f;      // [FW] JP-8000
    /** [FW] JP-8000: the total is converted in EXACT INTEGERS. One cent is
        0.058 per cent of a frequency, so the rounding is inaudible - and it is
        kept because it is the machine's arithmetic, and because a sum that is
        rounded at one place cannot pick up a different error at another. */
    [[nodiscard]] float hz() const noexcept
    {
        return kAnchorHz * std::exp2 (std::round (total()) / kCentsPerOctave);
    }
};

/** ONE FILTER'S RESOLVED CONTROLS.

    Filter 1 is the TRUNK and its controls are the unprefixed parameters this
    struct's owner also carries flat - `cutoffCents`, `resonance` and the rest -
    because those ids are frozen and every stored program uses them. `filt[0]` is
    FILLED FROM those fields in one place in Coeffs.cpp and is never written
    anywhere else, so the trunk stays the single authority for filter 1 and this
    array is the one shape the render loop can walk. Filters 2 and 3 have no
    trunk fields; `f2_*` and `f3_*` are their only home. */
struct FilterCoeffs
{
    float cutoffCents = 0.0f;
    float resonance = 0.0f;
    float keyTrack = 0.0f;
    float envAmount = 0.0f;       ///< filter 2 and 3 take ENV 2 through the matrix
    float slope = 24.0f;
    float shape = 0.0f;
    float width = 0.5f;
    float drive = 0.0f;
    /** HOW MUCH OF THIS FILTER REACHES THE OUTPUT. One filter never needed a
        level because it WAS the voice; three summing need a balance. 1.0 is
        unity and `x * 1.0f` is exactly x, which is what lets a program that
        only uses filter 1 be bit-identical to the machine before this existed. */
    float level = 1.0f;
    /** WHERE THIS FILTER SITS IN THE IMAGE. [MAN] Serum puts PAN in the filter
        strip, and once a filter can be fed by one oscillator that is the only
        place it can live: a routed lane has already been summed with whatever
        else the filter takes, so the LANE cannot place it and the FILTER can.
        The machine's own BALANCE law, unity at the centre. */
    float pan = 0.5f;
    int type = 0;
    bool bypass = false;
};

/** THE AUDIBLE RANGE the cutoff control is presented over, ten octaves.
    HERE AND NOT ON THE SCREEN, because it is a property of the filter rather than of its
    picture: an adapter converting a host's 0..1 knob needs it and must not have to include
    the UI header to get it, and a second copy of 20/20000 anywhere is a copy that can
    drift. `Screen::kLo`/`kHi` alias these. */
inline constexpr float kCutoffLoHz = 20.0f;
inline constexpr float kCutoffHiHz = 20000.0f;

} } // namespace dsp::filter
