#include "Engine.h"
#include <algorithm>

namespace examplefx {

// ---------------------------------------------------------------------------
// TODO(new machine): every field of Coeffs, in order. The static_assert stops
// the build when a field is added, so this list cannot go stale in silence.
static_assert (sizeof (Coeffs) == 48,
               "Coeffs changed size: a field was added or removed. Add it to "
               "Coeffs::fields() below - every probe's idea of 'did this program "
               "reach the engine' is that list - and then update this number.");

std::vector<double> Coeffs::fields() const
{
    return { (double) drive, (double) delayMs, (double) feedback, (double) dampingHz, (double) delayMix, (double) modRateHz, (double) modDepthMs, (double) modMix, (double) tremDepth, (double) width, (double) output, (double) modPhaseAnchor };
}


void Engine::prepare (double sampleRate, int blockSize)
{
    (void) blockSize;
    sr = sampleRate;
    delayL.prepare (sampleRate, 2.5);
    delayR.prepare (sampleRate, 2.5);
    modL.prepare (sampleRate, 0.1);
    modR.prepare (sampleRate, 0.1);
    lfo.prepare (sampleRate);
    lfo.setShape (dsp::LFO::Shape::Sine);
    clip.setThreshold (0.8f);
    reset();
}

void Engine::reset()
{
    delayL.reset(); delayR.reset();
    modL.reset(); modR.reset();
}

void Engine::process (float* left, float* right, int n) noexcept
{
    const double dl = std::max (1.0, (double) coeffs.delayMs * 0.001 * sr);
    // The right channel is offset by the width control rather than being a copy:
    // a stereo delay that is two identical mono delays measures as stereo and
    // does not sound it.
    const double dr = dl * (1.0 + 0.18 * (double) coeffs.width);
    delayL.setDelaySamples (dl);
    delayR.setDelaySamples (dr);
    delayL.setFeedback (coeffs.feedback);
    delayR.setFeedback (coeffs.feedback);
    delayL.setDamping ((double) coeffs.dampingHz);
    delayR.setDamping ((double) coeffs.dampingHz);
    lfo.setRate ((double) coeffs.modRateHz);
    if (coeffs.modPhaseAnchor >= 0.0f)
        lfo.setPhase ((double) coeffs.modPhaseAnchor);
    clip.setDrive (1.0f + coeffs.drive * 20.0f);

    // A short swept delay, 1 to 13 ms: that range is a FLANGER/CHORUS and
    // nothing else. Push it under a millisecond and it is a comb; push it past
    // 40 and it is a slapback.
    const double centreMs = 7.0;
    const float  modDepth = coeffs.modDepthMs;

    for (int i = 0; i < n; ++i)
    {
        float l = left[i], r = right[i];
        if (coeffs.drive > 0.0f) { l = clip.process (l); r = clip.process (r); }

        const float lfoV = lfo.tick();
        const double sweepL = (centreMs + (double) modDepth * lfoV) * 0.001 * sr;
        const double sweepR = (centreMs + (double) modDepth * -lfoV * (double) coeffs.width
                                        + (double) modDepth * lfoV * (1.0 - (double) coeffs.width))
                              * 0.001 * sr;
        modL.push (l); modR.push (r);
        const float mL = modL.readCubic (std::max (1.0, sweepL));
        const float mR = modR.readCubic (std::max (1.0, sweepR));
        l = l * (1.0f - coeffs.modMix) + mL * coeffs.modMix;
        r = r * (1.0f - coeffs.modMix) + mR * coeffs.modMix;

        const float eL = delayL.process (l);
        const float eR = delayR.process (r);
        float outL = l * (1.0f - coeffs.delayMix * 0.5f) + eL * coeffs.delayMix;
        float outR = r * (1.0f - coeffs.delayMix * 0.5f) + eR * coeffs.delayMix;

        // Tremolo: AMPLITUDE ONLY, no delay anywhere - which is what separates it
        // from the three above it. It rides the same LFO, so a synced MOD rate
        // moves it too, and it is the destination a rate probe can measure
        // without arguing with a comb filter's own ripple.
        if (coeffs.tremDepth > 0.0f)
        {
            const float g = 1.0f - coeffs.tremDepth * 0.5f * (1.0f - lfoV);
            outL *= g; outR *= g;
        }
        left[i]  = coeffs.output * outL;
        right[i] = coeffs.output * outR;
    }
}

} // namespace examplefx
