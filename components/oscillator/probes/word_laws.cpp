// Does this component compute the words the firmware computes?
//
// A probe and not a unit test, because what it checks is a TRANSCRIPTION: every
// law in dsp/OscModule.h was copied out of a reading of the ROM, and a digit
// lost on the way in is silent. The generator itself is dspblocks' and is not
// re-implemented here, so it is not what can drift.
//
//   c++ -std=c++17 -I../.. -I../../../../dspblocks/include word_laws.cpp -o word_laws && ./word_laws
#include "oscillator/dsp/OscModule.h"
#include <cstdio>
#include <cstdint>

using namespace synthkit::osc;

static int fails = 0;
static void check (bool ok, const char* what, double got, double want)
{
    if (! ok) { std::printf ("  FAIL  %-34s got %.4f want %.4f\n", what, got, want); ++fails; }
}

int main()
{
    // A synthetic table. The point is the arithmetic around it, so it only has
    // to be known: 128 entries with every low bit set, so a missing >> 1 or a
    // missing + 2 both show up rather than cancelling.
    std::int16_t table[128];
    for (int i = 0; i < 128; ++i) table[i] = (std::int16_t) ((i % 60) * 21 + 1);

    for (int c1 = 0; c1 < 128; ++c1)
        for (int c2 = 0; c2 < 128; c2 += 17)
        {
            const Words ss = Words::from (SuperSaw, c1, c2, table);
            check (ss.a == double ((table[c1] >> 1) + 2), "super saw detune word", ss.a,
                   double ((table[c1] >> 1) + 2));
            check (ss.b == 64.0 * c2 + 400.0, "super saw mix word", ss.b, 64.0 * c2 + 400.0);

            const Words tm = Words::from (TriangleMod, c1, c2, nullptr);
            check (tm.a == double ((127 - c1) << 8), "triangle mod word A", tm.a,
                   double ((127 - c1) << 8));

            const Words nz = Words::from (Noise, c1, c2, nullptr);
            check (nz.a == 100.0 * c1 - 1292.0, "noise colour cents", nz.a, 100.0 * c1 - 1292.0);
            check (nz.a == noiseColourCents (c1), "noise colour agrees with helper",
                   nz.a, noiseColourCents (c1));
        }

    // The published anchors, printed so a reader can check them against the
    // playbook rather than trusting that the loop above ran.
    std::printf ("  noise colour at ctrl1 0, 64, 127: %.0f, %.0f, %.0f cents\n",
                 noiseColourCents (0), noiseColourCents (64), noiseColourCents (127));
    std::printf ("  super saw mix word at ctrl2 0 and 127: %.0f, %.0f\n",
                 Words::from (SuperSaw, 0, 0, table).b, Words::from (SuperSaw, 0, 127, table).b);
    check (Words::from (SuperSaw, 0, 0, table).b == 400.0, "mix word floor", 0, 400);
    check (Words::from (SuperSaw, 0, 127, table).b == 8528.0, "mix word ceiling", 0, 8528);

    // Out-of-range controls clamp and never index past the table.
    check (Words::from (SuperSaw, -5, 0, table).a == Words::from (SuperSaw, 0, 0, table).a,
           "ctrl1 clamps low", 0, 0);
    check (Words::from (SuperSaw, 999, 0, table).a == Words::from (SuperSaw, 127, 0, table).a,
           "ctrl1 clamps high", 0, 0);
    // SUPER SAW with no table must not read one.
    check (Words::from (SuperSaw, 64, 0, nullptr).a == 0.0, "no table, no read", 0, 0);

    // The balance ends isolate, which is the thing that cost an afternoon.
    check (mix (1.0, -1.0, 0) == 1.0, "balance 0 is OSC1 alone", mix (1.0, -1.0, 0), 1.0);
    check (mix (1.0, -1.0, 127) == -1.0, "balance 127 is OSC2 alone", mix (1.0, -1.0, 127), -1.0);

    // Cross mod is unavailable on exactly two waveforms [MAN].
    int avail = 0;
    for (int w = 0; w <= 6; ++w) if (crossModAvailable (w)) ++avail;
    check (avail == 5, "cross mod on five of seven waveforms", double (avail), 5);
    check (! crossModAvailable (SuperSaw), "no cross mod on super saw", 0, 0);
    check (! crossModAvailable (Noise), "no cross mod on noise", 0, 0);

    std::printf (fails == 0 ? "  ok - every word law transcribes correctly\n"
                            : "  %d FAILURES\n", fails);
    return fails == 0 ? 0 : 1;
}
