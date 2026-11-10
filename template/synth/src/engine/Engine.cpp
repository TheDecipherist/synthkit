#include "Engine.h"
#include <algorithm>

namespace examplesynth {

// ---------------------------------------------------------------------------
// TODO(new machine): every field of Coeffs, in order. The static_assert stops
// the build when a field is added, so this list cannot go stale in silence.
static_assert (sizeof (Coeffs) == 68,
               "Coeffs changed size: a field was added or removed. Add it to "
               "Coeffs::fields() below - every probe's idea of 'did this program "
               "reach the engine' is that list - and then update this number.");

std::vector<double> Coeffs::fields() const
{
    return { (double) wave, (double) tuneSemis, (double) subLevel, (double) cutoffSemis, (double) resonance, (double) keyTrack, (double) envAmount, (double) attack, (double) decay, (double) sustain, (double) release, (double) lfoRateHz, (double) lfoToCutoff, (double) lfoPhaseAnchor, (double) delayMs, (double) delayMix, (double) master };
}


namespace {
inline float polyBlep (double t, double dt) noexcept
{
    if (t < dt)            { const double x = t / dt; return (float) (x + x - x * x - 1.0); }
    if (t > 1.0 - dt)      { const double x = (t - 1.0) / dt; return (float) (x * x + x + x + 1.0); }
    return 0.0f;
}
} // namespace

// ---------------------------------------------------------------------------
void Voice::prepare (double sampleRate)
{
    sr = sampleRate;
    filter.prepare (sampleRate);
    stage = Stage::idle;
    env = 0.0f;
    phase = subPhase = 0.0;
}

void Voice::noteOn (int midiNote, float velocity)
{
    key = midiNote;
    vel = velocity;
    stage = Stage::attack;
    filter.reset();
    phase = subPhase = 0.0;
}

void Voice::noteOff()
{
    if (stage != Stage::idle) stage = Stage::release;
}

CutoffTerms Voice::cutoffTerms (const Coeffs& c, float lfoValue) const noexcept
{
    CutoffTerms t;
    t.baseSemitones = c.cutoffSemis;
    // The key track a user can set. 1.0 = full tracking, one semitone of cutoff
    // per semitone of key, measured from middle C.
    t.patchKeyTrack = c.keyTrack * (float) (key - 60);
    // TODO(new machine): t.hardwiredKeyFollow. Read the coefficient builder, not
    // the parameter handlers. A 0 here is a claim that the firmware sums nothing
    // the record does not name, and on the M1 that claim was wrong by 2.3
    // octaves. Tag it [FW] with an address when you have read it.
    t.envelope = c.envAmount * env * 60.0f;      // +/- 5 octaves at full depth
    // TODO(new machine): t.velocity - velocity into the EG's INTENSITY. Decoded,
    // named and read by nothing on the M1 while 103 of 126 halves carried one.
    t.lfo = c.lfoToCutoff * lfoValue * 24.0f;    // sums in at ZERO: invisible at rest
    return t;
}

float Voice::render (const Coeffs& c, float lfoValue) noexcept
{
    if (stage == Stage::idle) return 0.0f;

    const float hz = 440.0f * std::pow (2.0f, ((float) key + c.tuneSemis - 69.0f) / 12.0f);
    const double dt = (double) hz / sr;
    phase += dt; if (phase >= 1.0) phase -= 1.0;
    subPhase += dt * 0.5; if (subPhase >= 1.0) subPhase -= 1.0;

    float osc = 0.0f;
    switch (c.wave)
    {
        case 1:  osc = (phase < 0.5 ? 1.0f : -1.0f)
                     - polyBlep (phase, dt) + polyBlep (std::fmod (phase + 0.5, 1.0), dt); break;
        case 2:  osc = (float) (4.0 * std::fabs (phase - 0.5) - 1.0); break;
        default: osc = (float) (2.0 * phase - 1.0) - polyBlep (phase, dt); break;
    }
    if (c.subLevel > 0.0f)
        osc += c.subLevel * (float) (2.0 * subPhase - 1.0);

    // envelope
    const float aInc = 1.0f / (float) std::max (1.0, c.attack * sr);
    const float dInc = 1.0f / (float) std::max (1.0, c.decay * sr);
    const float rInc = 1.0f / (float) std::max (1.0, c.release * sr);
    switch (stage)
    {
        case Stage::attack:  env += aInc; if (env >= 1.0f) { env = 1.0f; stage = Stage::decay; } break;
        case Stage::decay:   env -= dInc * (1.0f - c.sustain);
                             if (env <= c.sustain) { env = c.sustain; stage = Stage::sustain; } break;
        case Stage::sustain: env = c.sustain; break;
        case Stage::release: env -= rInc; if (env <= 0.0f) { env = 0.0f; stage = Stage::idle; } break;
        case Stage::idle:    break;
    }
    started += 1.0f / (float) sr;

    const auto terms = cutoffTerms (c, lfoValue);
    filter.set ((double) std::min (terms.hz(), (float) (sr * 0.45)),
                0.7071 + 8.0 * (double) c.resonance);
    return filter.lowpass (osc * 0.35f) * env * vel;
}

// ---------------------------------------------------------------------------
void Engine::prepare (double sampleRate, int blockSize)
{
    (void) blockSize;
    sr = sampleRate;
    for (auto& v : voices) v.prepare (sampleRate);
    lfo.prepare (sampleRate);
    lfo.setShape (dsp::LFO::Shape::Sine);
    delayL.prepare (sampleRate, 2.0);
    delayR.prepare (sampleRate, 2.0);
    delayL.setFeedback (0.35f);
    delayR.setFeedback (0.35f);
}

void Engine::noteOn (int midiNote, float velocity)
{
    Voice* pick = nullptr;
    for (auto& v : voices) if (! v.active()) { pick = &v; break; }
    if (pick == nullptr)
    {
        pick = &voices[0];
        for (auto& v : voices) if (v.age() > pick->age()) pick = &v;
    }
    pick->noteOn (midiNote, velocity);
}

void Engine::noteOff (int midiNote)
{
    for (auto& v : voices) if (v.active() && v.note() == midiNote) v.noteOff();
}

void Engine::allNotesOff()
{
    for (auto& v : voices) v.noteOff();
}

void Engine::process (float* left, float* right, int n) noexcept
{
    lfo.setRate ((double) coeffs.lfoRateHz);
    if (coeffs.lfoPhaseAnchor >= 0.0f)
        lfo.setPhase ((double) coeffs.lfoPhaseAnchor);
    const double dl = std::max (1.0, (double) coeffs.delayMs * 0.001 * sr);
    delayL.setDelaySamples (dl);
    delayR.setDelaySamples (dl * 1.13);       // a hair wider on the right, for width

    for (int i = 0; i < n; ++i)
    {
        const float lfoValue = lfo.tick();
        float dry = 0.0f;
        for (auto& v : voices) dry += v.render (coeffs, lfoValue);
        const float wetL = delayL.process (dry);
        const float wetR = delayR.process (dry);
        const float m = coeffs.delayMix;
        left[i]  += coeffs.master * (dry * (1.0f - m * 0.5f) + wetL * m);
        right[i] += coeffs.master * (dry * (1.0f - m * 0.5f) + wetR * m);
    }
    voiceClock += (float) n / (float) sr;
}

float Engine::restingCutoffHz (int midiNote) const noexcept
{
    CutoffTerms t;
    t.baseSemitones = coeffs.cutoffSemis;
    t.patchKeyTrack = coeffs.keyTrack * (float) (midiNote - 60);
    t.envelope = coeffs.envAmount * coeffs.sustain * 60.0f;
    return t.hz();
}

} // namespace examplesynth
