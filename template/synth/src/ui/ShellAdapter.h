// ExampleSynth on the shared bottom shell (~/projects/synthshell), synth variant:
// the keyboard row, CHORDS / ARP / CHOPPER on one master clock, and the Patch
// Explorer.
//
// THIS FILE IS THE WHOLE INTEGRATION. The shell talks only to shell::ShellHost,
// so a unit writes this adapter and nothing else - no SessionStrip, no
// KeyboardRow, no explorer, no arp designer. Thirty repos each held their own
// 1,250-line copy of that code and the copies drifted in silence: three had
// fallen behind, two had fixes nobody else had, and six named the wrong machine
// on screen. DEPEND, NEVER COPY.
#pragma once
#include <shell/ShellHost.h>
#include "../PluginProcessor.h"
#include "BuildVersion.h"

namespace examplesynth {

class ShellAdapter : public shell::ShellHost
{
public:
    explicit ShellAdapter (ExampleSynthProcessor& p) : proc (p) {}

    // ---- programs ---------------------------------------------------------
    int  programCount() const override { return (int) proc.programs().size(); }
    int  currentProgram() const override { return proc.getCurrentProgram(); }
    void selectProgram (int i) override { proc.setCurrentProgram (i); }
    int  programsVersion() const override { return proc.programVersion(); }
    bool isUserProgram (int i) const override
    {
        const auto& b = proc.programs();
        return i >= 0 && i < (int) b.size() && b[(size_t) i].bank == "USER";
    }
    shell::ProgramInfo programInfo (int i) const override
    {
        shell::ProgramInfo out;
        const auto& b = proc.programs();
        if (i < 0 || i >= (int) b.size()) return out;
        const auto& p = b[(size_t) i];
        // The HARDWARE's own number, not the explorer's row index. The 2290's
        // factory sounds live in 80..99 on the machine and its display read "00"
        // for preset 80 until it was pointed at the program's own location.
        out.id       = juce::String (p.id);
        out.name     = juce::String (p.name);
        out.bank     = juce::String (p.bank);
        out.category = juce::String (p.category);
        out.kind     = "Program";
        out.isUser   = p.bank == "USER";
        return out;
    }
    // The shared sound-type list. Categories are sound TYPES, never a bank name,
    // so "By category" means the same thing on every machine.
    // TODO(new machine): if this machine stores its OWN category byte - the
    // Supernova keeps a Finder category at body[1] - read it, because it is the
    // sound designer's own answer and beats any heuristic. And if a list puts
    // most of the bank in "Other", it is the wrong list: a filter box is not a
    // reverb and neither is a looper.

    // ---- the user bank ----------------------------------------------------
    bool canEditUserBank() const override { return true; }
    int  addUserProgram (const juce::String& n) override { return proc.addUserProgram (n, "Other"); }
    bool updateUserProgram (int i) override { return proc.updateUserProgram (i); }
    bool deleteUserProgram (int i) override { return proc.deleteUserProgram (i); }
    bool renameProgram (int i, const juce::String& n) override { return proc.renameProgram (i, n); }
    bool setProgramCategory (int i, const juce::String& c) override { return proc.setProgramCategory (i, c); }
    bool saveUserBank() override { proc.saveUserBank(); return true; }
    bool canExportSysex() const override { return true; }
    juce::File exportProgramSysex (int i) override { return proc.exportProgramSysex (i); }

    // ---- the transport ----------------------------------------------------
    bool   isSessionPlaying() const override { return proc.isSessionPlaying(); }
    void   setSessionPlaying (bool on) override { proc.setSessionPlaying (on); }
    bool   isSessionLoop() const override { return proc.isSessionLoop(); }
    void   setSessionLoop (bool l) override { proc.setSessionLoop (l); }
    int    sessionBars() const override { return proc.sessionBars(); }
    void   setSessionBars (int n) override { proc.setSessionBars (n); }
    double sessionPosBeats() const override { return proc.sessionPosBeats(); }

    // ---- the session tabs -------------------------------------------------
    shell::SessionState& sessionEdit() override { return proc.sessionEdit(); }
    void publishSession() override { proc.publishSession(); }
    int  arpDisplayStep() const override { return proc.arpDisplayStep(); }

    // ---- tempo sync -------------------------------------------------------
    /** WHAT IN THIS MACHINE FOLLOWS THE CLOCK. Declaring this list is the ONLY
        sync code a unit writes; the shell owns the switch, the divisions, the
        menu, the maths and the persistence.

        SYNC EVERYTHING THAT MOVES, AND DEFAULT IT OFF. The first version of that
        rule said the opposite - only sync what the hardware itself could sync -
        and it was wrong. These are plugins in a host: offering the option
        falsifies no law, because it sets a rate from the clock instead of from a
        knob and it is OFF until the player asks. What the hardware could do
        governs the SOUND LAWS; it does not govern whether the plugin can take a
        tempo.

        TODO(new machine): list this machine's destinations, and fill `lo`/`hi`
        where the machine has a real limit. The Reverb 4000's pre-delay tops out
        at 300 ms, so a quarter at 120 BPM silently clamped and the header read
        SYNC 1/4 while the box sat at its ceiling. And carry the machine's own
        vocabulary in `ownNames` where it differs: the Virus writes a dotted
        eighth as 3/16, the H3000 counts in n/24. */
    std::vector<SyncTarget> syncTargets() const override
    {
        std::vector<SyncTarget> t;
        SyncTarget lfo;
        lfo.name = "LFO"; lfo.kind = SyncTarget::rate;
        lfo.defaultDivision = shell::TempoSync::sixteenth;
        lfo.lo = 0.05; lfo.hi = 20.0;                   // the LFO's declared range, in Hz
        t.push_back (lfo);
        SyncTarget dly;
        dly.name = "DELAY"; dly.kind = SyncTarget::time;
        dly.defaultDivision = shell::TempoSync::eighth;
        dly.lo = 10.0; dly.hi = 1500.0;                 // the delay's declared range, in ms
        t.push_back (dly);
        return t;
    }
    void syncChanged() override { /* the processor rebuilds coefficients each block */ }

    // ---- readouts ---------------------------------------------------------
    juce::MidiKeyboardState* keyboardState() override { return &proc.keyboardState; }
    void setUiPitchBend (float v) override { proc.setUiPitchBend (v); }
    void setUiModWheel (float v) override { proc.setUiModWheel (v); }
    juce::String lastTouchedText() const override { return proc.lastTouched(); }
    juce::String buildStamp() const override { return SYNTHKIT_BUILD_VERSION; }
    juce::String brandLabel() const override { return "The Decipherist  ExampleSynth"; }
    juce::AudioProcessorValueTreeState& parameters() override { return proc.apvts; }

private:
    ExampleSynthProcessor& proc;
};

} // namespace examplesynth
