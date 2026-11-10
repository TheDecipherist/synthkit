// Does the machine actually follow the clock? Not "does the maths return 500" -
// does an impulse pushed through the REAL processor come back out with its
// echoes 500 ms apart when the transport says 120 BPM and the division says 1/4.
//
// This exists because the maths was right and the sound was wrong: nine effects
// units shipped with the tempo never reaching the engine at all, and a test of
// TempoSync::milliseconds would have passed the whole time. So it measures the
// AUDIO, out of the processor, with the shell host in the path.
//
// MIDI carries no tempo message: 24 clock bytes per quarter note, and the tempo
// IS their rate. Every expected number below is 60000/BPM x beats and nothing
// else, so a failure here is a WIRING failure, never a rounding one.
//
//   ExampleSynthSyncProbe             measure
//   ExampleSynthSyncProbe --selftest  break the wiring on purpose and require
//                                     this probe to notice
#include <juce_audio_processors/juce_audio_processors.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include "PluginProcessor.h"
#include "ui/ShellAdapter.h"

namespace {

constexpr double kSr = 48000.0;
constexpr int    kBlock = 512;

int failures = 0;

void check (const char* what, double got, double want, double tol, const char* unit = "ms")
{
    const bool ok = std::fabs (got - want) <= tol;
    if (! ok) ++failures;
    std::printf ("  %-40s measured %9.2f %-2s  expected %9.2f %-2s  %s\n",
                 what, got, unit, want, unit, ok ? "ok" : "MISMATCH");
}

/** A host playhead, with an ADVANCING ppq.
    A frozen position keeps tripping the locate threshold and PINS the clock. It
    is invisible to a delay, which wants a length, and obvious to anything that
    wants a position - which is why it presents as "the modulation is broken and
    the delay is fine". */
struct Head : juce::AudioPlayHead
{
    double bpm = 120.0, ppq = 0.0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setIsPlaying (playing);
        p.setPpqPosition (ppq);
        p.setTimeInSeconds (ppq * 60.0 / bpm);
        return p;
    }
};

/** Render `seconds` of the processor with one note held, the transport running,
    and the playhead advancing properly. Returns the left channel. */
std::vector<float> render (examplesynth::ExampleSynthProcessor& proc, Head& head,
                           double seconds, bool holdNote)
{
    proc.prepareToPlay (kSr, kBlock);
    proc.setPlayHead (&head);
    head.ppq = 0.0;

    const int total = (int) (kSr * seconds);
    std::vector<float> out;
    out.reserve ((size_t) total);
    juce::AudioBuffer<float> buf (2, kBlock);
    juce::MidiBuffer midi;
    bool sent = false;
    for (int done = 0; done < total; done += kBlock)
    {
        midi.clear();
        if (holdNote && ! sent)
        { midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 110), 0); sent = true; }
        buf.clear();
        proc.processBlock (buf, midi);
        for (int i = 0; i < kBlock; ++i) out.push_back (buf.getSample (0, i));
        head.ppq += (double) kBlock / kSr * head.bpm / 60.0;
    }
    proc.setPlayHead (nullptr);
    return out;
}

/** TODO(new machine): THIS ASSUMES THE MACHINE HAS A DELAY. Many do not - an
    analogue polysynth's only time parameter may be an LFO DELAY or a glide
    time, and then there is no echo train to space out and this whole function
    has to be replaced by an ONSET measurement. The Synthex is the worked
    example: it measures when a synced LFO delay lets the modulation in, against
    an absolute expected value.

    Whatever the observable, pick it off playbook 9a's ladder and route the
    modulator to a pure GAIN if you can. Three meter faults were found in one
    afternoon doing exactly this, and every one looked like an engine bug: an
    RMS window shorter than the note's period reads the WAVEFORM; two detuned
    oscillators BEAT and a level envelope cannot tell a beat from an LFO; and
    autocorrelation argmax takes the octave-down, which is the exact signature
    of a wrong clock rate and is nothing of the kind.

    The delay time, measured as the spacing of the echo train from an impulse.
    A delay line's output is a train of decaying copies, so the gap between the
    first two IS the delay time. */
double measuredDelayMs (examplesynth::ExampleSynthProcessor& proc, Head& head)
{
    // A short pluck rather than a held note: the tail has to be quiet between
    // repeats or there is nothing to find a peak in.
    proc.prepareToPlay (kSr, kBlock);
    proc.setPlayHead (&head);
    head.ppq = 0.0;
    juce::AudioBuffer<float> buf (2, kBlock);
    juce::MidiBuffer midi;
    std::vector<float> x;
    const int total = (int) (kSr * 4.0);
    for (int done = 0; done < total; done += kBlock)
    {
        midi.clear();
        if (done == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 120), 0);
        if (done == kBlock) midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        buf.clear();
        proc.processBlock (buf, midi);
        for (int i = 0; i < kBlock; ++i) x.push_back (std::fabs (buf.getSample (0, i)));
        head.ppq += (double) kBlock / kSr * head.bpm / 60.0;
    }
    // RMS in short windows, then peak-pick the windows. Peak-picking the RAW
    // waveform reads the oscillator; a one-pole follower fast enough to track
    // the echo train still ripples at the note's own rate and picks bumps
    // 20 ms apart inside the first blip, which is what this used to do. Whole
    // windows are also the answer to the other meter trap: peak-picking an
    // envelope reads about 2 % fast at every tempo, and a meter wrong by a
    // consistent percentage survives every cross-check that compares one
    // measurement against another.
    constexpr int kWin = (int) (kSr * 0.002);            // 2 ms
    std::vector<double> rms;
    for (size_t i = 0; i + (size_t) kWin <= x.size(); i += (size_t) kWin)
    {
        double s2 = 0.0;
        for (int k = 0; k < kWin; ++k) s2 += (double) x[i + (size_t) k] * x[i + (size_t) k];
        rms.push_back (std::sqrt (s2 / (double) kWin));
    }
    double peak = 0.0;
    for (double v : rms) peak = std::max (peak, v);
    if (peak <= 1.0e-6) return -1.0;

    std::vector<double> times;
    const double thresh = peak * 0.15;
    for (size_t i = 1; i + 1 < rms.size(); ++i)
    {
        if (rms[i] < thresh) continue;
        if (! (rms[i] >= rms[i - 1] && rms[i] > rms[i + 1])) continue;
        const double ms = (double) i * (double) kWin * 1000.0 / kSr;
        // One peak, not its skirt: the shortest division this probe asks for is
        // a 1/16 at 140 BPM, 107 ms, so 50 ms cannot merge two real echoes.
        if (! times.empty() && ms - times.back() < 50.0) continue;
        times.push_back (ms);
    }
    return times.size() >= 2 ? times[1] - times[0] : -1.0;
}

/** The modulation envelope: BRIGHTNESS in 5 ms windows, as the ratio of
    high-frequency to total energy.
    
    Not the output LEVEL, and the difference cost a diagnosis here. A resonant
    low-pass swept over a harmonic-rich note is LOUDEST when its resonance peak
    crosses a strong harmonic, which happens on the way up and again on the way
    down - so the level peaks twice a cycle and neither peak is where the LFO is.
    Brightness rises and falls with the cutoff itself, so it peaks once, where the
    modulator actually is.
    
    Measured over whole windows rather than by peak-picking, because peak-picking
    an envelope reads about 2 % fast at EVERY tempo. That looks exactly like an
    engine bug and it is the meter - and a meter wrong by a consistent percentage
    survives every cross-check that compares one measurement against another.
    Only an absolute expected value exposes it. */
// 2 ms rather than 5: the grid error is what the "last period you can see"
// refinement below divides down, and a finer grid makes short periods - a 1/8
// at 140 BPM is 214 ms - measurable at all.
constexpr double kEnvHopMs = 2.0;

std::vector<double> modulationEnvelope (const std::vector<float>& x)
{
    const int win = (int) (kSr * kEnvHopMs * 0.001);
    std::vector<double> e;
    for (size_t i = 1; i + (size_t) win <= x.size(); i += (size_t) win)
    {
        double hi = 0.0, all = 0.0;
        for (int k = 0; k < win; ++k)
        {
            const double v = x[i + (size_t) k];
            const double d = v - x[i + (size_t) k - 1];
            hi += d * d; all += v * v;
        }
        e.push_back (all > 1.0e-14 ? std::sqrt (hi / all) : 0.0);
    }
    // Remove the note's own decay, which is a slow trend the period search would
    // otherwise lock onto.
    const size_t n = e.size();
    double mean = 0.0;
    for (double v : e) mean += v;
    mean /= (double) std::max<size_t> (1, n);
    for (auto& v : e) v -= mean;
    return e;
}

/** The modulation period in ms, by AUTOCORRELATION rather than peak counting: a
    peak counter picks the LFO's harmonics out of a resonant sweep and reports a
    period that is a fraction of the real one. */
double modulationPeriodMs (const std::vector<double>& e, double loMs, double hiMs)
{
    const int lo = (int) (loMs / kEnvHopMs), hi = (int) (hiMs / kEnvHopMs);
    if ((int) e.size() < hi * 2) return -1.0;
    std::vector<double> ac ((size_t) (hi + 1), 0.0);
    double best = 0.0;
    for (int lag = lo; lag <= hi; ++lag)
    {
        double a = 0.0;
        for (size_t i = 0; i + (size_t) lag < e.size(); ++i) a += e[i] * e[i + (size_t) lag];
        a /= (double) (e.size() - (size_t) lag);
        ac[(size_t) lag] = a;
        if (a > best) best = a;
    }
    if (best <= 0.0) return -1.0;

    // TAKE THE SMALLEST STRONG LAG, NOT THE LARGEST CORRELATION. A periodic
    // signal correlates just as well at two and five times its period, and the
    // raw maximum read a 214 ms modulation as 1070 ms and a 400 ms one as 795 -
    // integer multiples every time, which is the signature of the METER rather
    // than of a bad clock. Anything within 90% of the best correlation IS the
    // period, so the first such lag that is a local maximum is the answer.
    // (Found on the Emulator II, 2026-09-09.)
    for (int lag = lo + 1; lag < hi; ++lag)
        if (ac[(size_t) lag] >= 0.9 * best
            && ac[(size_t) lag] >= ac[(size_t) lag - 1]
            && ac[(size_t) lag] >= ac[(size_t) lag + 1])
        {
            const auto refine = [&ac] (int at) {
                const double a = ac[(size_t) at - 1], b = ac[(size_t) at], c = ac[(size_t) at + 1];
                const double d = (a - 2.0 * b + c) != 0.0 ? 0.5 * (a - c) / (a - 2.0 * b + c) : 0.0;
                return (double) at + std::max (-1.0, std::min (1.0, d)); };

            // MEASURE THE LAST PERIOD YOU CAN SEE, NOT THE FIRST. The window hop
            // puts +/-1 hop of grid error on the answer, which on a 250 ms period
            // is 0.8% - enough to fail a 6% tolerance on a clock that is exact.
            // The Nth correlation peak carries the same ABSOLUTE grid error over
            // N periods, so dividing it by N divides the error by N. It is the
            // same trick as timing a hundred swings of a pendulum, and it took
            // the Emulator II's readings from 6% out to 0.5%.
            int k = 1;
            while ((k + 1) * lag < hi) ++k;
            int at = k * lag;
            for (int j = at - lag / 4; j <= at + lag / 4 && j < hi; ++j)
                if (j > lo && ac[(size_t) j] > ac[(size_t) at]) at = j;
            if (at > lo && at + 1 < hi && ac[(size_t) at] >= 0.5 * best)
                return refine (at) * kEnvHopMs / (double) k;
            return refine (lag) * kEnvHopMs;
        }
    return -1.0;
}

/** Where the FIRST modulation maximum falls, in ms from sample zero. */
double firstModulationPeakMs (const std::vector<double>& e, double periodMs)
{
    const int n = std::min ((int) e.size(), (int) (periodMs / kEnvHopMs));
    int at = 0;
    for (int i = 1; i < n; ++i) if (e[(size_t) i] > e[(size_t) at]) at = i;
    return (double) at * kEnvHopMs;
}

// ---------------------------------------------------------------------------
/** The whole measurement, as a function, so the selftest can run it against a
    deliberately broken processor and require it to report failures.
    `breakIt` is the injection point; nothing is written to disk, ever. */
int measureEverything (bool breakIt)
{
    using namespace examplesynth;
    failures = 0;

    ExampleSynthProcessor proc;
    auto& host = proc.host();
    Head head;

    std::printf ("MIDI sends %d clocks per quarter note and no tempo at all; the tempo is\n"
                 "their rate. Every expected number below is 60000/BPM x beats.\n\n",
                 shell::TempoSync::kPPQN);

    if (host.syncTargets().size() != (size_t) ExampleSynthProcessor::kNumSyncTargets)
    {
        std::printf ("FAIL: the unit declares %d sync destinations, expected %d\n",
                     (int) host.syncTargets().size(), ExampleSynthProcessor::kNumSyncTargets);
        return 1;
    }

    // A program with an audible delay and no LFO, so the echo train is clean.
    proc.setCurrentProgram (3);                        // "Saw Lead": delay mix 0.18
    proc.setParamValue (pDelayMix, 0.75f);
    proc.setParamValue (pLfoDepth, 0.0f);
    proc.setParamValue (pRelease, 0.05f);
    proc.setParamValue (pDecay, 0.08f);
    proc.setParamValue (pSustain, 0.0f);

    // ---- 1. A TIME DESTINATION, ACROSS THREE TEMPOS ------------------------
    // THE CHECK THAT CATCHES A DEAD CLOCK IS A TEMPO CHANGE, NEVER A DIVISION
    // CHANGE. The 2290 had 1/4, 1/8, 1/16, 1/8. and 1/8T all measuring exactly
    // right, and read 500 ms at 120 BPM and still 500 ms at 90.
    std::printf ("1. SYNC EXTERNAL - the clock overrides the stored delay time\n");
    host.setSyncMode (shell::ShellHost::syncExternal);
    for (size_t t = 0; t < host.syncTargets().size(); ++t)
        host.setSyncDivision ((int) t, shell::TempoSync::off);

    // THE INJECTION: a destination that is not there. Deleting a sync
    // destination is the commonest way a unit silently stops following the
    // clock - the division is set, nothing consumes it - and the probe has to
    // notice. Here it is done by leaving the delay's division at OFF while
    // claiming it is synced, which is what "the clock never reaches the engine"
    // looks like from the outside.
    struct Case { double bpm; int div; const char* name; };
    const Case cases[] = {
        { 120.0, shell::TempoSync::quarter,       "120 BPM, 1/4" },
        { 120.0, shell::TempoSync::eighth,        "120 BPM, 1/8" },
        {  90.0, shell::TempoSync::quarter,       " 90 BPM, 1/4" },
        { 140.0, shell::TempoSync::eighth,        "140 BPM, 1/8" },
        { 120.0, shell::TempoSync::dottedEighth,  "120 BPM, 1/8." },
    };
    for (const auto& c : cases)
    {
        host.setSyncDivision (ExampleSynthProcessor::kDelayTime,
                              breakIt ? shell::TempoSync::off : c.div);
        head.bpm = c.bpm;
        const double got = measuredDelayMs (proc, head);
        const double want = 60000.0 / c.bpm * shell::TempoSync::beats (c.div);
        if (got < 0.0)
        { std::printf ("  %-40s NO ECHO TRAIN\n", c.name); ++failures; continue; }
        check (c.name, got, want, std::max (12.0, want * 0.06));
    }

    // ---- 2. INTERNAL IS THE MACHINE'S OWN CLOCK ----------------------------
    // The subtlest failure in the whole area, found in three units at once, and
    // the SHARED GATE CANNOT SEE IT because it never calls processBlock. A
    // processor doing bpm.store(tempo) each block writes the resolved tempo -
    // the DAW's, while a DAW plays - into the field that IS the internal clock.
    //
    // TWO TEMPO PAIRS, NEVER ONE. With only 120-against-90, a unit whose
    // internal tempo happens to BE 90 passes while reading the host.
    std::printf ("\n2. SYNC INTERNAL against a host playing something else\n"
                 "   (the write-back check; two pairs, because one proves nothing)\n");
    host.setSyncMode (shell::ShellHost::syncInternal);
    host.setSyncDivision (ExampleSynthProcessor::kDelayTime, shell::TempoSync::quarter);
    for (const double own : { 120.0, 150.0 })
    {
        if (auto* p = proc.apvts.getParameter ("bpm"))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) own));
        head.bpm = 90.0;                       // the host, playing something ELSE
        const double got = measuredDelayMs (proc, head);
        char label[80];
        std::snprintf (label, sizeof label, "field at %.0f, host playing 90", own);
        if (got < 0.0) { std::printf ("  %-40s NO ECHO TRAIN\n", label); ++failures; continue; }
        check (label, got, 60000.0 / own, std::max (12.0, 60000.0 / own * 0.06));
    }
    if (auto* p = proc.apvts.getParameter ("bpm"))
        p->setValueNotifyingHost (p->convertTo0to1 (120.0f));

    // ---- 3. SYNC OFF LEAVES THE PROGRAM'S OWN TIME ALONE -------------------
    std::printf ("\n3. SYNC OFF - the program's own time, whatever the transport says\n");
    host.setSyncMode (shell::ShellHost::syncOff);
    proc.setParamValue (pDelayTime, 400.0f);
    head.bpm = 120.0;
    const double a = measuredDelayMs (proc, head);
    head.bpm = 75.0;
    const double b = measuredDelayMs (proc, head);
    if (a > 0.0 && b > 0.0)
    {
        const bool ok = std::fabs (a - b) < 12.0 && std::fabs (a - 400.0) < 30.0;
        if (! ok) ++failures;
        std::printf ("  %-40s %8.2f ms at 120, %8.2f ms at 75   %s\n",
                     "the stored 400 ms, tempo-independent", a, b, ok ? "ok" : "MOVED");
    }
    else { std::printf ("  no echo train with sync off\n"); ++failures; }

    // ---- 4. A RATE DESTINATION, AND ITS ABSOLUTE BEAT ALIGNMENT ------------
    // A SPACING MEASUREMENT CANNOT SEE A PHASE ERROR, and this bit twice. Test
    // the rate of a synced modulator by the gap between its peaks and a CONSTANT
    // offset cancels out of the arithmetic entirely, so every spacing test
    // passes while the modulator sits a full block early on every beat. The
    // D-50's LFOs were exactly that: green across the board and a block ahead of
    // the music. Spacing tests rate; only an ABSOLUTE beat-alignment assertion
    // tests phase, so every rate destination needs both.
    std::printf ("\n4. A RATE DESTINATION - period AND absolute beat alignment\n");
    proc.setCurrentProgram (0);
    proc.setParamValue (pDelayMix, 0.0f);
    proc.setParamValue (pLfoDepth, 0.9f);
    proc.setParamValue (pCutoff, 900.0f);
    // RESONANCE 0, and it is a METER decision, not a taste one. A resonant
    // low-pass swept over a harmonic-rich source is BRIGHTEST TWICE A CYCLE,
    // because the resonant peak crosses harmonics on the way up and again on the
    // way down. With Q up, the absolute beat-alignment test below read the
    // second bump and reported the LFO 115 ms off a 500 ms beat on an engine
    // that was exactly on it. (Emulator II, 2026-09-09.)
    proc.setParamValue (pResonance, 0.0f);
    proc.setParamValue (pSustain, 1.0f);
    proc.setParamValue (pDecay, 0.05f);
    proc.setParamValue (pAttack, 0.002f);
    proc.setParamValue (pKeyTrack, 0.0f);
    proc.setParamValue (pEnvAmount, 0.0f);
    host.setSyncMode (shell::ShellHost::syncExternal);
    host.setSyncDivision (ExampleSynthProcessor::kFilterLfo,
                          breakIt ? shell::TempoSync::off : shell::TempoSync::quarter);
    head.bpm = 120.0;
    {
        const auto env = modulationEnvelope (render (proc, head, 6.0, true));
        const double period = modulationPeriodMs (env, 150.0, 1200.0);
        if (period < 0.0)
        { std::printf ("  the modulation produced nothing measurable\n"); ++failures; }
        else
        {
            check ("1/4 at 120 BPM, modulation period", period, 500.0, 30.0);

            // THE ABSOLUTE ASSERTION, and it is not the same test. A SPACING
            // measurement cannot see a PHASE error: a constant offset cancels out
            // of the arithmetic entirely, so every spacing test passes while the
            // modulator sits a full block early on every beat. The D-50's LFOs
            // were exactly that - green across the board, and a block ahead of the
            // music. Spacing tests rate; only this tests phase.
            //
            // The convention: a sine LFO anchored to the clock is at zero and
            // RISING on the beat, so the filter is widest a quarter of a period
            // after it. Anything else means the anchor is wrong.
            const double first = firstModulationPeakMs (env, period);
            const double want = period * 0.25;
            const double off = std::fmod (first - want + period * 1.5, period) - period * 0.5;
            const bool aligned = std::fabs (off) < 60.0;
            if (! aligned) ++failures;
            std::printf ("  %-40s peak at %7.2f ms, %+6.1f ms off the beat   %s\n",
                         "absolute beat alignment", first, off,
                         aligned ? "ok" : "OFF THE BEAT (a period test cannot see this)");
        }
    }

    // ---- 5. THE RAW PULSE PATH ---------------------------------------------
    // No playhead at all, just 0xF8 at 24 per quarter. This is how the box is
    // slaved to a drum machine, and it is the only way tempo travels over MIDI.
    std::printf ("\n5. RAW MIDI CLOCK - no host, just pulses on the wire\n");
    {
        proc.prepareToPlay (kSr, kBlock);
        proc.setPlayHead (nullptr);
        host.setSyncMode (shell::ShellHost::syncExternal);
        host.setSyncDivision (ExampleSynthProcessor::kDelayTime, shell::TempoSync::quarter);
        for (const double wantBpm : { 120.0, 90.0, 145.0 })
        {
            const double perPulse = kSr * 60.0 / (wantBpm * shell::TempoSync::kPPQN);
            juce::AudioBuffer<float> buf (2, kBlock);
            double next = 0.0;
            bool started = false;
            for (int blk = 0; blk < 200; ++blk)
            {
                juce::MidiBuffer midi;
                if (! started) { midi.addEvent (juce::MidiMessage::midiStart(), 0); started = true; }
                while (next < (double) kBlock)
                { midi.addEvent (juce::MidiMessage::midiClock(), (int) next); next += perPulse; }
                next -= (double) kBlock;
                buf.clear();
                proc.processBlock (buf, midi);
            }
            const double got = host.hostTempo();
            const double ms  = host.syncedMs (ExampleSynthProcessor::kDelayTime);
            const bool ok = std::fabs (got - wantBpm) < 0.6 && std::fabs (ms - 60000.0 / wantBpm) < 4.0;
            if (! ok) ++failures;
            std::printf ("  pulses at %6.1f BPM -> read %6.2f BPM, 1/4 = %7.2f ms   %s\n",
                         wantBpm, got, ms, ok ? "ok" : "MISMATCH");
        }
        // Stop (0xFC) must RELEASE the lock, or the box stays slaved to a master
        // that has gone away.
        juce::AudioBuffer<float> buf (2, kBlock);
        for (int blk = 0; blk < 80; ++blk)
        {
            juce::MidiBuffer midi;
            if (blk == 0) midi.addEvent (juce::MidiMessage::midiStop(), 0);
            buf.clear();
            proc.processBlock (buf, midi);
        }
        const bool released = ! host.followingExternalClock();
        if (! released) ++failures;
        std::printf ("  %-40s %s\n", "Stop releases the external clock",
                     released ? "ok" : "STILL LOCKED");
    }

    return failures;
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    bool self = false;
    for (int i = 1; i < argc; ++i) if (std::strcmp (argv[i], "--selftest") == 0) self = true;

    if (! self)
    {
        std::printf ("ExampleSynth - does the machine follow the clock?\n\n");
        const int bad = measureEverything (false);
        std::printf ("\n%s\n", bad == 0 ? "PASS - the unit is on the clock"
                                        : "FAIL - see the mismatches above");
        return bad == 0 ? 0 : 1;
    }

    // The negative control: unhook the sync destinations - which is what "the
    // clock never reaches the engine" looks like from outside - and require this
    // probe to say so. Verify a check can FAIL, and do it in process so a run
    // that dies in the middle cannot leave an injected bug behind that looks
    // identical to a real failure.
    std::printf ("SELFTEST - unhook the clock and require the probe to notice\n\n");
    const int bad = measureEverything (true);
    std::printf ("\nthe broken build produced %d findings\n", bad);
    std::printf ("%s\n", bad > 0
        ? "PASS - the probe fails when the clock does not reach the engine"
        : "FAIL - the probe passed a unit with no clock; it is proving nothing");
    return bad > 0 ? 0 : 1;
}
