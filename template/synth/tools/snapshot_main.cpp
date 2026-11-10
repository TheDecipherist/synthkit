// Offscreen render of the editor, for visual proof.
//
//   ExampleSynthSnapshot [width]   -> build/snapshot.png at the width the window
//                                     actually opens to
//   ExampleSynthSnapshot --tab N   -> pick the session-strip tab first
//                                     (0 CHORDS, 1 ARP, 2 CHOPPER, 3 EXPLORER)
//   ExampleSynthSnapshot --rms N.. -> RMS through the full plugin path per program
//   ExampleSynthSnapshot --onset N -> samples from note-on to -60 dB and -26 dB
//   ExampleSynthSnapshot --syx N.. -> the real SAVE path, read back
//
// JUDGE THE PANEL AT THE WIDTH THE WINDOW OPENS TO, not at the width of the
// render: the JP-8000's legends all looked fine in a 3680 px PNG and half of
// them collided at the ~2444 px the editor actually opens on. heightForWidth()
// lives in the editor so the render and the window cannot disagree.
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <cstdio>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "patch/Program.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    examplesynth::ExampleSynthProcessor proc;
    const juce::String mode = argc > 1 ? argv[1] : "";

    if (mode == "--rms" || mode == "--onset")
    {
        constexpr double sr = 48000.0;
        constexpr int block = 512;
        for (int a = 2; a < argc; ++a)
        {
            const int prog = std::atoi (argv[a]);
            proc.setCurrentProgram (prog);
            proc.prepareToPlay (sr, block);
            juce::AudioBuffer<float> buf (2, block);
            std::vector<float> out;
            bool sent = false;
            for (int b = 0; b < 200; ++b)
            {
                juce::MidiBuffer m;
                if (! sent) { m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 110), 0); sent = true; }
                buf.clear();
                proc.processBlock (buf, m);
                for (int i = 0; i < block; ++i) out.push_back (buf.getSample (0, i));
            }
            if (mode == "--rms")
            {
                double s = 0.0;
                for (float v : out) s += (double) v * v;
                std::printf ("program %3d %-20s rms %.5f\n", prog,
                             proc.getProgramName (prog).toRawUTF8(),
                             std::sqrt (s / (double) juce::jmax<size_t> (1, out.size())));
            }
            else
            {
                // Before blaming the audio device, MEASURE. Several "latency"
                // reports turned out to be the JUCE standalone running on the
                // device's default 512-sample buffer.
                double peak = 0.0;
                for (float v : out) peak = juce::jmax (peak, (double) std::fabs (v));
                const auto firstAbove = [&] (double rel)
                {
                    for (size_t i = 0; i < out.size(); ++i)
                        if (std::fabs (out[i]) >= peak * rel) return (long) i;
                    return -1L;
                };
                std::printf ("program %3d %-20s -60 dB at %6ld samples, -26 dB at %6ld\n",
                             prog, proc.getProgramName (prog).toRawUTF8(),
                             firstAbove (0.001), firstAbove (0.05));
            }
        }
        return 0;
    }

    if (mode == "--syx")
    {
        for (int a = 2; a < argc; ++a)
        {
            const int prog = std::atoi (argv[a]);
            const auto f = proc.exportProgramSysex (prog);
            if (! f.existsAsFile()) { std::printf ("FAILED program %d\n", prog); return 1; }
            juce::MemoryBlock mb;
            f.loadFileAsData (mb);
            examplesynth::Program back;
            const bool ok = examplesynth::fromRecord ((const uint8_t*) mb.getData(), mb.getSize(), back);
            std::printf ("%-60s %4d bytes  decode %s  name %s\n",
                         f.getFullPathName().toRawUTF8(), (int) mb.getSize(),
                         ok ? "ok" : "FAILED", back.name.c_str());
            if (! ok) return 1;
        }
        return 0;
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* e = dynamic_cast<examplesynth::ExampleSynthEditor*> (ed.get());
    if (e == nullptr) { std::printf ("no editor\n"); return 1; }
    if (mode == "--tab" && argc > 2) e->showStripTab (std::atoi (argv[2]));
    const int w = (argc > 1 && mode != "--tab") ? std::atoi (argv[1]) : ed->getWidth();
    ed->setSize (w, e->heightForWidth (w));

    juce::Image img (juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true);
    struct Stop : juce::Timer
    { void timerCallback() override { juce::MessageManager::getInstance()->stopDispatchLoop(); } } stop;
    stop.startTimer (1200);
    juce::MessageManager::getInstance()->runDispatchLoop();
    { juce::Graphics g (img); ed->paintEntireComponent (g, true); }

    auto out = juce::File::getCurrentWorkingDirectory().getChildFile ("build/snapshot.png");
    out.getParentDirectory().createDirectory();
    // A FileOutputStream over an existing file does not truncate it, so a render
    // that never changes is a stale file until proven otherwise. Delete first.
    out.deleteFile();
    { juce::FileOutputStream fs (out); juce::PNGImageFormat().writeImageToStream (img, fs); }
    std::printf ("wrote %s  %dx%d  %lld bytes\n", out.getFullPathName().toRawUTF8(),
                 img.getWidth(), img.getHeight(), (long long) out.getSize());
    return out.getSize() > 0 ? 0 : 1;
}
