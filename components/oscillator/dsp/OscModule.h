#pragma once
// THE OSCILLATOR SECTION's own arithmetic. Not the blocks.
//
// The waveform generators live in dspblocks (`dsp/JpOsc.h`), which is the
// declared owner of DSP. What lives here is the half that is specific to this
// module and is not a reusable block: the laws that turn two 0..127 panel
// controls into the WORDS the machine's sound chip is actually given, the
// two-oscillator carrier with its sync edge, and the balance. That is the same
// split the FILTER component makes, where the block is the filter and the
// component owns the cutoff sum and the coefficient struct.
//
// NO ROM BYTES PASS THROUGH HERE, and the one table this section needs is
// therefore handed in rather than stored. `~/projects/eproms` holds the dumps;
// a derived table belongs to the unit that derived it, and a formula belongs
// here. Embedding 128 words would break that and would also pin this component
// to one machine's firmware revision.
//
// PROVENANCE, because a name without one corrupts everything downstream:
//   [FW 0xA5B0]  super saw     detune word = (table[c1] >> 1) + 2, mix = 64*c2 + 400
//   [FW 0xA5E8]  triangle mod  word A = (127 - c1) << 8, word B = c2
//   [FW 0xA610]  noise         colour = 100*c1 - 1292 cents, word B from the
//                              RESONANCE table (it is resonance, not a level)
//   [FW 0x17]    balance       crossfades OSC1 against whichever of OSC2 or the
//                              ring product the switch selected, so ring
//                              REPLACES OSC2 rather than adding to it
//   [EMU]        the two words per waveform were confirmed a second time on
//                2026-10-07 by counting the firmware's own control reads, five
//                strict ctrl1/ctrl2 pairs in waveform-index order
//   [MAN]        cross modulation is unavailable when OSC1 is SUPER SAW or NOISE

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <dsp/JpOsc.h>

namespace synthkit::osc
{

/** OSC1's waveforms, in the order the patch byte numbers them. The order is not
    cosmetic: the firmware's per-waveform cases appear in exactly this sequence,
    which is what anchors the two confirmed readings to their cases. */
enum Osc1Wave { SuperSaw = 0, TriangleMod = 1, Noise = 2, FeedbackOsc = 3,
                SqrPwm = 4, Saw = 5, Tri = 6 };

/** OSC2's waveforms. THREE of them, and a fourth value clamps to the third
    rather than wrapping - measured, by rendering value 3 and finding it
    identical to value 2 on every measure to within 0.1 dB and 2 Hz. */
enum Osc2Wave { Osc2Saw = 0, Osc2Square = 1, Osc2Triangle = 2 };

inline int clamp127 (int v) noexcept { return std::min (127, std::max (0, v)); }

/** The words the chip is given, per waveform. A control's MEANING changes with
    the waveform and its curve does not: one knob carries thirteen names across
    the seven waveforms and only two bytes ever reach the chip. */
struct Words
{
    double a = 0.0;          // from ctrl1
    double b = 0.0;          // from ctrl2

    /** `detuneTable` is the unit's own 128-entry table, `int16_t` because that is
        how the donor stores it and a cast at every call site is worse than
        matching it. The ROM words are small (the largest is 1280) so the
        signedness never bites, and `>> 1` on a positive value is the same shift
        either way. Pass nullptr on any
        waveform but SUPER SAW; pass it there and keep its entry-103 dip, which
        is in the ROM and is not a transcription error. */
    static Words from (int wave, int c1, int c2, const std::int16_t* detuneTable) noexcept
    {
        c1 = clamp127 (c1); c2 = clamp127 (c2);
        Words w {};
        switch (wave)
        {
            case SuperSaw:                                        // [FW 0xA5B0]
                w.a = detuneTable != nullptr ? double ((detuneTable[c1] >> 1) + 2) : 0.0;
                w.b = 64.0 * c2 + 400.0;
                break;
            case TriangleMod:                                     // [FW 0xA5E8]
                w.a = double ((127 - c1) << 8);
                w.b = double (c2);
                break;
            case Noise:                                           // [FW 0xA610]
                w.a = 100.0 * c1 - 1292.0;       // CENTS, and measured at 100.5/step
                w.b = double (c2);               // resonance, NOT a level
                break;
            default:
                w.a = double (c1);
                w.b = double (c2);
                break;
        }
        return w;
    }
};

/** OSC1's noise colour in cents, which is the one law on this section measured
    against an independent implementation of the same firmware and agreeing to
    0.5%: the table says 100 cents per step and a ten-point sweep read 100.5. */
inline double noiseColourCents (int ctrl1) noexcept
{
    return 100.0 * double (clamp127 (ctrl1)) - 1292.0;
}

/** The balance. 0 is OSC1 alone and 127 is OSC2 alone, measured by isolating
    each oscillator at the two ends and watching the other's waveform character
    disappear. `ring` REPLACES osc2 when the switch selects it [FW 0x17]. */
inline double mix (double osc1, double osc2OrRing, int balance) noexcept
{
    const double t = double (clamp127 (balance)) / 127.0;
    return osc1 * (1.0 - t) + osc2OrRing * t;
}

/** Cross modulation is unavailable on two of OSC1's waveforms [MAN]. A panel
    may still show the knob; the section must ignore it, because a knob that
    does something the machine does not do is worse than a knob that is dead. */
inline bool crossModAvailable (int osc1Wave) noexcept
{
    return osc1Wave != SuperSaw && osc1Wave != Noise;
}

/** The two-oscillator carrier. OSC2's sync edge is OSC1's wrap, which is why
    the wrap flag is threaded through both calls in this order and not reset
    between them. */
struct Section
{
    dsp::jp::Osc one, two;
    bool wrapped = false;

    void reset() noexcept { one.reset(); two.reset(); wrapped = false; }

    struct Setup
    {
        int osc1Wave = SuperSaw, osc1Ctrl1 = 0, osc1Ctrl2 = 0;
        int osc2Wave = Osc2Saw,  osc2Ctrl1 = 0, osc2Ctrl2 = 0;
        int balance = 0;
        bool osc2Sync = false;
        double osc1Hz = 440.0, osc2Hz = 440.0, sampleRate = 44100.0;
        double lfo1 = 0.0;
        const std::int16_t* detuneTable = nullptr;
    };

    double sample (const Setup& s) noexcept
    {
        const Words w = Words::from (s.osc1Wave, s.osc1Ctrl1, s.osc1Ctrl2, s.detuneTable);
        const double a = one.sample (s.osc1Wave, s.osc1Ctrl1, s.osc1Ctrl2, s.osc1Hz,
                                     s.sampleRate, s.lfo1, true, false, wrapped, w.a);
        const double b = two.sample (s.osc2Wave, s.osc2Ctrl1, s.osc2Ctrl2, s.osc2Hz,
                                     s.sampleRate, s.lfo1, false, s.osc2Sync, wrapped, 0.0);
        return mix (a, b, s.balance);
    }
};

} // namespace synthkit::osc
