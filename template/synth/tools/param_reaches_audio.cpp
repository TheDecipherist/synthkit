// Does a parameter written the way the EDITOR and the HOST write it actually
// reach the audio, in the right DIRECTION and by a musically real amount?
//
// The first version of this test asked only "did the rms change at all", with a
// 1e-5 threshold. It passed while MAIN measured LOUDER at 0.05 than at 0.9 -
// which is not a parameter working, it is render-to-render variation being
// mistaken for one. A check that cannot fail is worse than no check, and a
// threshold that small cannot fail.
//
// So: a fresh prepareToPlay before each render, so neither carries the other's
// state, and a DIRECTION with a magnitude, not a difference.
#include "PluginProcessor.h"
#include <cstdio>
#include <cmath>

namespace {

/** Render WITHOUT re-preparing, which is what the running plugin does: it is
    prepared once when the host opens it, and after that a knob is turned and
    nothing else happens. Re-preparing between renders hides the whole bug,
    because prepareToPlay marks the coefficients stale as a side effect - so a
    test that prepares each time proves only that STARTUP works. */
double renderRms (examplesynth::ExampleSynthProcessor& p, const char* id, float value)
{
    if (auto* q = p.apvts.getParameter (id)) q->setValueNotifyingHost (value);

    juce::AudioBuffer<float> b (2, 512);
    juce::MidiBuffer m;
    m.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
    double acc = 0.0; int n = 0;
    for (int i = 0; i < 40; ++i)
    {
        b.clear();
        p.processBlock (b, m);
        m.clear();
        for (int s = 0; s < b.getNumSamples(); ++s)
        { const double v = b.getSample (0, s); acc += v * v; ++n; }
    }
    return n ? std::sqrt (acc / (double) n) : 0.0;
}

} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    examplesynth::ExampleSynthProcessor proc;
    proc.prepareToPlay (48000.0, 512);        // ONCE, as a host does

    struct Case { const char* id; const char* what; double minRatio; };
    // ADAPT THIS LIST TO YOUR MACHINE, and pick parameters whose effect this
    // METER can actually see. RMS measures LEVEL, so a level parameter belongs
    // here and a FILTER does not: the template's cutoff measured a ratio of
    // 1.07 open against closed, because a saw's fundamental passes a closed
    // low-pass and carries most of the energy. That is the meter being wrong,
    // not the filter - the same family of mistake as an RMS window shorter than
    // the oscillator's period reading the waveform, or a resonant sweep being
    // brightest twice a cycle. Test a filter with a SPECTRAL measure (energy
    // above a corner), which is what spread_probe and sync_probe already do.
    //
    // ADAPT THIS LIST TO YOUR MACHINE. Each row says what the parameter MEANS,
    // so a wrong DIRECTION fails instead of counting as "it changed something".
    // The ids below are this template's; a real unit names its own - and a name
    // that does not exist is reported rather than silently skipped, because a
    // test that writes to nothing passes every time.
    const Case cases[] = {
        { "sustain",   "louder held at 0.9 than at 0.05",       1.5 },
        { "sub_level", "louder with the sub up",                1.2 },
    };

    int bad = 0;
    for (const auto& c : cases)
    {
        if (proc.apvts.getParameter (c.id) == nullptr)
        {
            std::printf ("  %-12s NO SUCH PARAMETER - this probe's list is stale\n", c.id);
            ++bad;
            continue;
        }
        const double hi = renderRms (proc, c.id, 0.9f);
        const double lo = renderRms (proc, c.id, 0.05f);
        const double ratio = lo > 1.0e-9 ? hi / lo : (hi > 1.0e-9 ? 1.0e9 : 1.0);
        const bool ok = ratio >= c.minRatio;
        if (! ok) ++bad;
        std::printf ("  %-12s rms %.6f at 0.9, %.6f at 0.05   ratio %6.2f (want >= %.1f)  %s\n",
                     c.id, hi, lo, ratio, c.minRatio, ok ? "ok" : "NOT REACHING THE AUDIO");
    }
    std::printf ("\n%s\n", bad == 0
        ? "PASS - a direct parameter write moves the sound, in the right direction"
        : "FAIL - the parameter changes and the engine does not follow it");
    return bad == 0 ? 0 : 1;
}
