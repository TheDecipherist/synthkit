// The shared clock gate, run against a KNOWN-GOOD host and against known-bad
// ones.
//
// A shared check that is not itself checked is a shared bug. Ours silently
// inverted into passing nothing and failing eleven healthy units, and every
// build stayed green throughout - the same failure mode as the thing it exists
// to catch. So this asserts BOTH directions: a healthy host reports zero
// findings, and a host carrying a real defect reports at least one.
//
// It needs no engine and no plugin - shell::conformance::checkClock reads a
// ShellHost and nothing else - so it runs in milliseconds and is the first thing
// to look at when the clock behaves oddly.
//
// WHAT THIS TEST CANNOT COVER, and why the audio probe is not optional:
// checkClock's own numbered checks 2, 3, 5, 6 and 7 read members of ShellHost
// that a subclass cannot override, so no fake host can make them fire. They are
// checks on the SHELL, not on the unit. Everything a unit can actually get
// wrong - the tempo never reaching the engine, INTERNAL following the host,
// sync not overriding the knob - is measured in the AUDIO by
// tools/sync_probe.cpp. The gate never calls processBlock, so it structurally
// cannot see those.
#include <shell/ClockConformance.h>
#include <cstdio>

namespace {

/** A minimal HEALTHY unit: two destinations, one time and one rate, an OFF
    position, and an internal clock that is genuinely its own. */
struct GoodHost : shell::ShellHost
{
    juce::AudioProcessorValueTreeState* apvts = nullptr;

    int  programCount() const override { return 1; }
    int  currentProgram() const override { return 0; }
    void selectProgram (int) override {}
    shell::ProgramInfo programInfo (int) const override { return {}; }
    bool isUserProgram (int) const override { return false; }
    bool   isSessionPlaying() const override { return false; }
    void   setSessionPlaying (bool) override {}
    bool   isSessionLoop() const override { return true; }
    void   setSessionLoop (bool) override {}
    int    sessionBars() const override { return 2; }
    void   setSessionBars (int) override {}
    double sessionPosBeats() const override { return 0.0; }
    juce::String brandLabel() const override { return "Good"; }
    juce::AudioProcessorValueTreeState& parameters() override { return *apvts; }

    double sessionBpm() const override { return 120.0; }
    bool offersSyncOff() const override { return true; }
    std::vector<SyncTarget> syncTargets() const override
    {
        SyncTarget a; a.name = "DELAY"; a.kind = SyncTarget::time;
        a.defaultDivision = shell::TempoSync::quarter;
        SyncTarget b; b.name = "LFO"; b.kind = SyncTarget::rate;
        b.defaultDivision = shell::TempoSync::eighth;
        return { a, b };
    }
};

/** DEFECT 1: the machine declares that nothing in it follows the clock.
    That was the state every unit was in, and it is not a legitimate answer for a
    box with a delay, an LFO, a chorus, a tremolo or a chopper in it. A machine
    with genuinely nothing that moves in time skips this check DELIBERATELY, with
    the evidence written down - never by accident. */
struct NoDestinations : GoodHost
{
    std::vector<SyncTarget> syncTargets() const override { return {}; }
};

/** DEFECT 2: the tempo write-back, and it is the subtlest failure in the whole
    area - found in three units at once.

    A processor doing `bpm.store (tempo)` each block writes the RESOLVED tempo,
    which while a DAW plays is the DAW's, into the field that IS the machine's
    internal clock. SYNC INTERNAL then quietly becomes the host's tempo, which is
    the one thing internal is not. It looks perfect under every external
    measurement, because the write-back is what makes external work.

    Modelled here exactly: internalTempo() returns whatever the transport says. */
struct WritesBackTheHostTempo : GoodHost
{
    double internalTempo() const override { return hostTempo(); }
};

/** DEFECT 3: an internal clock that does not run. A synced patch then goes
    silent with no host clock, which is not what any of these machines do - the
    internal clock is always running and nothing stalls waiting for a transport
    that is not coming. */
struct DeadInternalClock : GoodHost
{
    double internalTempo() const override { return 0.0; }
};

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // The gate reads the "bpm" parameter, so the fakes get a real APVTS. A stub
    // here is how a gate ends up testing itself.
    struct Dummy : juce::AudioProcessor
    {
        Dummy() : juce::AudioProcessor (BusesProperties()) {}
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        const juce::String getName() const override { return "dummy"; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    } dummy;
    juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "s",
    {
        std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "bpm", 1 }, "Tempo",
            juce::NormalisableRange<float> { 20.0f, 300.0f }, 120.0f)
    });

    int bad = 0;
    std::printf ("shell::conformance::checkClock, in BOTH directions\n\n");

    std::printf ("A. a healthy unit must PASS\n");
    {
        GoodHost g; g.apvts = &apvts;
        const int n = shell::conformance::checkClock (g, "healthy");
        if (n != 0)
        {
            std::printf ("   *** the gate failed a healthy unit (%d findings). It has inverted,\n"
                         "       which is how it once passed nothing and failed eleven good units.\n", n);
            ++bad;
        }
        else std::printf ("   ok\n");
    }

    std::printf ("\nB. each known defect must FAIL\n");
    const auto mustFail = [&bad, &apvts] (auto&& host, const char* name, const char* why)
    {
        host.apvts = &apvts;
        std::printf ("   %-26s %s\n", name, why);
        if (shell::conformance::checkClock (host, name) == 0)
        {
            std::printf ("   *** NOT CAUGHT - the gate is proving nothing here\n");
            ++bad;
        }
    };
    mustFail (NoDestinations{}, "no destinations",
              "nothing in the machine declared to follow the clock");
    mustFail (WritesBackTheHostTempo{}, "tempo write-back",
              "INTERNAL follows the DAW, so internal is not internal");
    mustFail (DeadInternalClock{}, "dead internal clock",
              "nothing moves without a host, and synced patches go silent");

    std::printf ("\n%s\n", bad == 0
        ? "PASS - the gate passes the good host and fails every bad one"
        : "FAIL - the gate is not measuring what it claims");
    return bad == 0 ? 0 : 1;
}
