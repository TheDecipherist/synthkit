#include "Coeffs.h"
#include <cmath>
#include <cstdio>

namespace examplesynth {

namespace {
/** Hz -> semitones above CutoffTerms::kAnchorHz. The filter and the oscillator
    share ONE pitch law here, deliberately: on the Supernova the filter cutoff
    reads the SAME exponent table as the oscillator pitch, and giving cutoff its
    own curve is what makes the two stop tracking as they do on the original.
    [REF] - check whether YOUR machine does the same before splitting them. */
inline float hzToSemis (float hz) noexcept
{
    return CutoffTerms::kSemitonesPerOctave
         * std::log2 (std::max (1.0f, hz) / CutoffTerms::kAnchorHz);
}
} // namespace

Coeffs makeCoeffs (const Program& p)
{
    Coeffs c;
    c.wave        = (int) (p.value[pOscWave] + 0.5f);
    c.tuneSemis   = p.value[pOscTune] + p.value[pOscFine] * 0.01f;
    c.subLevel    = p.value[pSubLevel];

    c.cutoffSemis = hzToSemis (p.value[pCutoff]);
    c.resonance   = p.value[pResonance];
    c.keyTrack    = p.value[pKeyTrack];
    c.envAmount   = p.value[pEnvAmount];

    c.attack      = p.value[pAttack];
    c.decay       = p.value[pDecay];
    c.sustain     = p.value[pSustain];
    c.release     = p.value[pRelease];

    c.lfoRateHz   = p.value[pLfoRate];
    c.lfoToCutoff = p.value[pLfoDepth];

    c.delayMs     = p.value[pDelayTime];
    c.delayMix    = p.value[pDelayMix];

    c.master      = p.value[pMaster];
    return c;
}

int checkNeutralDefaults() { return checkNeutralDefaults (Program{}); }

int checkNeutralDefaults (const Program& defaults)
{
    // What "neutral" means for each parameter that claims to have a neutral
    // value, and what its default must therefore compute. A control the machine
    // does not have cannot have a right default - two of the PSR-730's three
    // did not exist on the machine at all - so this list is also the place to
    // notice a parameter nobody can point at on a front panel.
    struct Neutral { int index; const char* what; float wanted; float got; };
    const auto c = makeCoeffs (defaults);
    const Neutral checks[] = {
        { pSubLevel,   "sub oscillator off",          0.0f, c.subLevel },
        { pResonance,  "filter not resonating",       0.0f, c.resonance },
        { pKeyTrack,   "no key tracking",             0.0f, c.keyTrack },
        { pEnvAmount,  "envelope not moving cutoff",  0.0f, c.envAmount },
        { pLfoDepth,   "LFO not reaching the filter", 0.0f, c.lfoToCutoff },
        { pDelayMix,   "delay inaudible",             0.0f, c.delayMix },
    };
    int bad = 0;
    for (const auto& n : checks)
    {
        if (std::fabs (n.got - n.wanted) > 1.0e-6f)
        {
            std::printf ("  NEUTRAL DEFAULT WRONG: %s (%s) defaults to %.4f and computes "
                         "%.4f, wanted %.4f\n", kParams[n.index].id, n.what,
                         defaults.value[n.index], n.got, n.wanted);
            ++bad;
        }
    }
    return bad;
}

} // namespace examplesynth
