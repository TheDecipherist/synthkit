// The effect engine. Blocks only: it never sees a Program and never allocates
// once prepared.
//
// The blocks themselves come from ~/projects/dspblocks - DEPEND, NEVER COPY.
// That library exists because six effects units each reproduced their machine's
// PARAMETER layer faithfully while running one generic engine underneath, so a
// Digiplex did not sound like a delay and a Swept Comb did not sound like a
// comb. A unit's own coefficient builder stays here, next to the firmware
// evidence that produced it; the processing does not.
//
// TODO(new machine): the STRUCTURE is right; the effect is a stand-in. A
// delay, a modulated delay and a drive stage, and none of it is anybody's
// machine. AND CHECK IT IS THE RIGHT KIND OF EFFECT: a phaser is a chain of
// allpasses with unevenly spaced notches and no delay anywhere; a flanger is a
// SHORT swept delay whose notches are harmonically spaced; a chorus is a longer
// gentler one with little feedback. Getting the delay range wrong turns one into
// another, and the parameter layer will look perfect throughout.
#pragma once
#include <dsp/Delay.h>
#include <dsp/Filters.h>
#include <dsp/Modulation.h>
#include <dsp/Drive.h>
#include <cmath>

namespace examplefx {

/** What the message thread hands the audio thread. Plain data, double-buffered. */
struct Coeffs
{
    float drive = 0.0f;
    float delayMs = 300.0f;
    float feedback = 0.3f;
    float dampingHz = 6000.0f;
    float delayMix = 0.35f;
    float modRateHz = 0.6f;
    float modDepthMs = 0.0f;
    float modMix = 0.0f;
    float tremDepth = 0.0f;
    float width = 0.5f;
    float output = 0.8f;
    /** The master clock's phase at THIS BLOCK'S START when the modulation is
        slaved, 0..1 within the chosen division; -1 when free-running. Same rule
        as the synth template: take the phase from the clock rather than
        converting a rate, so the modulator is ON the beat and survives a locate,
        and take the BLOCK'S START, because readTransport advances the clock
        before processBlock runs. */
    float modPhaseAnchor = -1.0f;

    /** Every field, in order, as one comparable sequence.

        NOT a memcmp on this struct, and that is not a style preference. The
        moment a Coeffs holds anything but one type - a bool beside an int, an
        int beside a double - the compiler puts PADDING between them, and padding
        bytes are INDETERMINATE. Two Coeffs built from the same program by the
        same function then compare unequal roughly at random, and the probes
        report "every program's stored fields never reach the engine" on a build
        where every one of them does. A false alarm from the check that exists to
        catch the H3500's 272 real ones is the worst kind of check.

        The static_assert beside the definition is the other half: add a field to
        this struct and the size changes and the build stops, so nothing can be
        added and silently left out of every comparison the probes make. */
    [[nodiscard]] std::vector<double> fields() const;

};

class Engine
{
public:
    void prepare (double sampleRate, int blockSize);
    void reset();
    void setCoeffs (const Coeffs& c) noexcept { coeffs = c; }
    [[nodiscard]] const Coeffs& currentCoeffs() const noexcept { return coeffs; }

    /** In place, stereo. */
    void process (float* left, float* right, int numSamples) noexcept;

private:
    double sr = 48000.0;
    Coeffs coeffs;
    dsp::FeedbackDelay delayL, delayR;
    dsp::DelayLine modL, modR;
    dsp::LFO lfo;
    dsp::SoftClip clip;
};

} // namespace examplefx
