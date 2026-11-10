// The voice engine. Blocks only: it never sees a Program, never parses
// anything, and never allocates once prepared.
//
// This mirrors the hardware split every one of these machines actually has - a
// host CPU computed every coefficient and the DSPs held only blocks - and it is
// what keeps patch-state branching off the audio thread. ParameterEngine (the
// processor) -> coefficients -> VoiceEngine -> audio.
//
// TODO(new machine): the STRUCTURE here is right; the SOUND is a stand-in. One
// oscillator, one filter, one envelope, one LFO, one delay, and nothing in it
// belongs to any machine. Replace the laws, not the shape.
#pragma once
#include <dsp/Filters.h>
#include <dsp/Delay.h>
#include <dsp/Modulation.h>
#include <array>
#include <vector>
#include <cmath>

namespace examplesynth {

/** THE CUTOFF IS A SUM, and the term you miss has no parameter behind it.
    (Playbook 17b; the single most valuable finding of the M1 work.)
    
    A hardware filter's cutoff is not a parameter. It is a running sum of terms,
    computed per note, and only SOME of them are things a user can set. On the
    M1 the firmware summed five words and our engine modelled three; the missing
    one was an unconditional keyboard follow of 71 counts per key that no program
    byte can switch off, and it was worth 2.3 OCTAVES at middle C.
    
    The terms you will miss are exactly the ones with no patch byte behind them,
    because nothing in the record ever points at them. So this struct exists to
    make the sum EXPLICIT and to make an unaccounted term a visible gap rather
    than an absence. Every field is in the same unit - semitones, so the whole
    sum is one exponential away from a frequency - and every field has to be
    named, sourced and tagged.
    
    HOW TO FILL IT ON A REAL MACHINE: find the routine that writes the filter
    coefficient and read every instruction that reaches its accumulator. Not the
    parameter handlers - the coefficient builder. Then account for every term.
    If your engine sums fewer terms than the firmware, the difference is your bug
    and it is measurable. */
struct CutoffTerms
{
    float baseSemitones   = 0.0f;   ///< the patch's own cutoff, as semitones above 20 Hz
    float patchKeyTrack   = 0.0f;   ///< the key-track control the USER can set
    float hardwiredKeyFollow = 0.0f;///< [TODO/FW] the one with no parameter. The M1's was
                                    ///  71 counts per key, three instructions away from
                                    ///  and entirely separate from the control above.
                                    ///  Leave at 0 only until the firmware says otherwise;
                                    ///  a 0 here is a CLAIM, not a default.
    float envelope        = 0.0f;   ///< filter envelope x its depth
    float velocity        = 0.0f;   ///< [TODO/FW] velocity into the EG's INTENSITY, not just
                                    ///  its level. 103 of the M1's 126 active halves carried
                                    ///  one and nothing read it.
    float lfo             = 0.0f;   ///< modulation, sums in at zero so it is invisible at rest
    float zoneTrim        = 0.0f;   ///< [TODO/FW] a per-sample or per-zone signed trim. A
                                    ///  sample-based machine normalises somewhere and that
                                    ///  somewhere is usually a byte nobody documents.
    float controllers     = 0.0f;   ///< [TODO/FW] joystick / aftertouch / breath sweeps. They
                                    ///  sum in at zero, so they are only wrong when moved.

    [[nodiscard]] float total() const noexcept
    {
        return baseSemitones + patchKeyTrack + hardwiredKeyFollow + envelope
             + velocity + lfo + zoneTrim + controllers;
    }

    /** Semitones above 20 Hz -> Hz. The ANCHOR and the SLOPE are two different
        claims and they fail differently: how many counts make an octave, and
        what frequency count zero means. Do not let a settled slope imply a
        settled anchor - the M1's slope was settled twice over and its anchor is
        still [EAR] and honestly labelled. */
    static constexpr float kAnchorHz = 20.0f;              // [TODO/FW] count zero
    static constexpr float kSemitonesPerOctave = 12.0f;    // [TODO/FW] the slope
    [[nodiscard]] float hz() const noexcept
    { return kAnchorHz * std::pow (2.0f, total() / kSemitonesPerOctave); }
};

/** What the message thread hands the audio thread. Plain data, double-buffered
    by the processor, swapped at block start. No strings, no vectors, no patch. */
struct Coeffs
{
    int   wave = 0;               // 0 saw, 1 square, 2 triangle
    float tuneSemis = 0.0f;       // osc tune + fine, in semitones
    float subLevel = 0.0f;

    float cutoffSemis = 0.0f;     // CutoffTerms::baseSemitones, resolved
    float resonance = 0.0f;
    float keyTrack = 0.0f;
    float envAmount = 0.0f;

    float attack = 0.005f, decay = 0.4f, sustain = 0.8f, release = 0.3f;

    float lfoRateHz = 4.0f;
    float lfoToCutoff = 0.0f;
    /** When the LFO is slaved, the master clock's phase at THIS BLOCK'S START,
        0..1 within the chosen division. -1 means free-running.

        RE-ANCHOR A SYNCED MODULATOR TO THE CLOCK EACH BLOCK, and take the
        BLOCK'S START. Measured over 40 s on raw 0xF8, an integrated rate drifts
        -0.79 ms and an anchored one +0.02, so drift is not the argument: what
        the anchor buys is being ON THE BEAT rather than merely at the right
        speed, and surviving a locate. And it has to be the block's start,
        because readTransport advances the clock before processBlock runs -
        anchoring to the raw position puts every synced LFO one block, about
        10 ms, past the beat forever. */
    float lfoPhaseAnchor = -1.0f;

    float delayMs = 300.0f;
    float delayMix = 0.0f;

    float master = 0.8f;

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

/** One voice: oscillator + sub, a 2-pole state-variable low-pass from dspblocks,
    and one envelope doing amplitude and cutoff. */
class Voice
{
public:
    void prepare (double sampleRate);
    void noteOn (int midiNote, float velocity);
    void noteOff();
    [[nodiscard]] bool active() const noexcept { return stage != Stage::idle; }
    [[nodiscard]] int  note() const noexcept { return key; }
    [[nodiscard]] float age() const noexcept { return started; }

    /** The cutoff this voice is running at, right now, as the full sum. Public
        because the bank-wide "where does each program's cutoff settle" check in
        tools/spread_probe.cpp reads it, and that check needs no disassembly:
        factory banks do not ship silent programs, so a meaningful share of
        voices resting implausibly dark means a term is missing. */
    [[nodiscard]] CutoffTerms cutoffTerms (const Coeffs&, float lfoValue) const noexcept;

    float render (const Coeffs&, float lfoValue) noexcept;

private:
    enum class Stage { idle, attack, decay, sustain, release };
    double sr = 48000.0;
    dsp::StateVariableFilter filter;
    double phase = 0.0, subPhase = 0.0;
    int   key = 60;
    float vel = 1.0f, env = 0.0f, started = 0.0f;
    Stage stage = Stage::idle;
};

class Engine
{
public:
    static constexpr int kMaxVoices = 8;

    void prepare (double sampleRate, int blockSize);
    void setCoeffs (const Coeffs& c) noexcept { coeffs = c; }
    [[nodiscard]] const Coeffs& currentCoeffs() const noexcept { return coeffs; }

    void noteOn (int midiNote, float velocity);
    void noteOff (int midiNote);
    void allNotesOff();

    /** Mono in, stereo out; the delay is the only stereo thing here. */
    void process (float* left, float* right, int numSamples) noexcept;

    /** For the spread probe: the cutoff the first voice would rest at for a
        given note with the envelope at sustain and the LFO at zero. */
    [[nodiscard]] float restingCutoffHz (int midiNote) const noexcept;

private:
    double sr = 48000.0;
    Coeffs coeffs;
    std::array<Voice, kMaxVoices> voices;
    dsp::LFO lfo;
    dsp::FeedbackDelay delayL, delayR;
    float voiceClock = 0.0f;
};

} // namespace examplesynth
