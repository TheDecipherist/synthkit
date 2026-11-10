// Offscreen render of the editor, for visual proof.
//
//   ExampleFxSnapshot [width]   -> build/snapshot.png at the width the window
//                                     actually opens to
//   ExampleFxSnapshot --tab N   -> pick the session-strip tab first
//                                     (0 CHORDS, 1 ARP, 2 CHOPPER, 3 EXPLORER)
//   ExampleFxSnapshot --rms N.. -> RMS through the full plugin path per program,
//                                  auditioned with STRINGS, not a click
//   ExampleFxSnapshot --syx N.. -> the real SAVE path, read back
//
// JUDGE THE PANEL AT THE WIDTH THE WINDOW OPENS TO, not at the width of the
// render: the JP-8000's legends all looked fine in a 3680 px PNG and half of
// them collided at the ~2444 px the editor actually opens on. heightForWidth()
// lives in the editor so the render and the window cannot disagree.
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <cstdio>
#include "PluginProcessor.h"
#include "ui/ShellAdapter.h"
#include "PluginEditor.h"
#include "patch/Program.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    examplefx::ExampleFxProcessor proc;
    const juce::String mode = argc > 1 ? argv[1] : "";

    if (mode == "--rms")
    {
        constexpr double sr = 48000.0;
        constexpr int block = 512;
        for (int a = 2; a < argc; ++a)
        {
            const int prog = std::atoi (argv[a]);
            proc.setCurrentProgram (prog);
            proc.prepareToPlay (sr, block);
            // AUDITION AN EFFECT WITH SOMETHING THAT SHOWS WHAT IT DOES. "I am
            // auditioning the click and I don't hear a huge difference in any of
            // the patches" was the click, not the presets: measured across the
            // 2290's twenty factory presets, the median preset-to-preset
            // difference was 0.0001 through a click and 0.0104 through strings.
            for (int i = 0; i < proc.host().auditionModes().size(); ++i)
                if (proc.host().auditionModes()[i].containsIgnoreCase ("string"))
                { proc.host().setAuditionMode (i); break; }
            proc.setSessionPlaying (true);
            juce::AudioBuffer<float> buf (2, block);
            double s2 = 0.0; int n = 0;
            for (int b = 0; b < 200; ++b)
            {
                juce::MidiBuffer m;
                buf.clear();
                proc.processBlock (buf, m);
                if (b > 20)
                    for (int i = 0; i < block; ++i)
                    { const float v = buf.getSample (0, i); s2 += (double) v * v; ++n; }
            }
            std::printf ("program %3d %-20s rms %.5f\n", prog,
                         proc.getProgramName (prog).toRawUTF8(),
                         std::sqrt (s2 / juce::jmax (1, n)));
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
            examplefx::Program back;
            const bool ok = examplefx::fromRecord ((const uint8_t*) mb.getData(), mb.getSize(), back);
            std::printf ("%-60s %4d bytes  decode %s  name %s\n",
                         f.getFullPathName().toRawUTF8(), (int) mb.getSize(),
                         ok ? "ok" : "FAILED", back.name.c_str());
            if (! ok) return 1;
        }
        return 0;
    }

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto* e = dynamic_cast<examplefx::ExampleFxEditor*> (ed.get());
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
