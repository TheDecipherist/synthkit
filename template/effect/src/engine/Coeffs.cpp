#include "Coeffs.h"
#include <cmath>
#include <cstdio>

namespace examplefx {

Coeffs makeCoeffs (const Program& p)
{
    Coeffs c;
    c.drive      = p.value[pDrive];
    c.delayMs    = p.value[pDelayTime];
    c.feedback   = p.value[pFeedback];
    c.dampingHz  = p.value[pDamping];
    c.delayMix   = p.value[pDelayMix];
    c.modRateHz  = p.value[pModRate];
    c.modDepthMs = p.value[pModDepth];
    c.modMix     = p.value[pModMix];
    c.tremDepth  = p.value[pTremDepth];
    c.width      = p.value[pWidth];
    c.output     = p.value[pOutput];
    return c;
}

int checkNeutralDefaults() { return checkNeutralDefaults (Program{}); }

int checkNeutralDefaults (const Program& defaults)
{
    struct Neutral { int index; const char* what; float wanted; float got; };
    const auto c = makeCoeffs (defaults);
    const Neutral checks[] = {
        { pDrive,    "no saturation",           0.0f, c.drive },
        { pModDepth, "no modulation sweep",     0.0f, c.modDepthMs },
        { pModMix,   "modulation inaudible",    0.0f, c.modMix },
        { pTremDepth,"no tremolo",              0.0f, c.tremDepth },
    };
    int bad = 0;
    for (const auto& n : checks)
        if (std::fabs (n.got - n.wanted) > 1.0e-6f)
        {
            std::printf ("  NEUTRAL DEFAULT WRONG: %s (%s) defaults to %.4f and computes "
                         "%.4f, wanted %.4f\n", kParams[n.index].id, n.what,
                         defaults.value[n.index], n.got, n.wanted);
            ++bad;
        }
    return bad;
}

} // namespace examplefx
