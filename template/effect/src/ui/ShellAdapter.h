// ExampleFx on the shared bottom shell (~/projects/synthshell), effects variant:
// no keyboard row, no CHORDS / ARP / CHOPPER, and an AUDITION tab in their place.
#pragma once
#include <shell/ShellHost.h>
#include "../PluginProcessor.h"
#include "BuildVersion.h"

namespace examplefx {

class ShellAdapter : public shell::ShellHost
{
public:
    /** THIS MACHINE'S CATEGORIES, and they are its own.

        The eleven reverb categories (Hall, Plate, Room, Chamber, Ambience, ...)
        are a REVERB vocabulary and say nothing on a delay-and-modulation box:
        every program would land in "Other". A category list that puts most of
        the bank in "Other" is the wrong list. Pick it from the machine, keep
        "Other" last, and FREEZE it alongside the parameter ids, because a
        program stores its category by name.

        TODO(new machine): replace with this machine's own. A filter box wants
        Filter / Resonant / Sweep / Envelope / Distortion / Ring Mod / Notch /
        Bass / Percussive / Special / Other; a looper wants Beat / Bass / Melody
        / Texture / Vocal / Percussion / FX / Other. */
    static constexpr int kNumCategories = 10;
    static constexpr const char* kCategories[kNumCategories] =
    { "Delay", "Echo", "Chorus", "Flanger", "Doubler", "Pan",
      "Tremolo", "Modulation", "Special", "Other" };

    explicit ShellAdapter (ExampleFxProcessor& p) : proc (p) {}

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
        out.id = juce::String (p.id);              // the HARDWARE's own number
        out.name = juce::String (p.name);
        out.bank = juce::String (p.bank);
        out.category = juce::String (p.category);
        out.kind = "Preset";
        out.isUser = p.bank == "USER";
        return out;
    }
    juce::StringArray categories() const override
    {
        juce::StringArray c;
        for (int i = 0; i < kNumCategories; ++i) c.add (kCategories[i]);
        return c;
    }

    bool canEditUserBank() const override { return true; }
    int  addUserProgram (const juce::String& n) override { return proc.addUserProgram (n, kCategories[0]); }
    bool updateUserProgram (int i) override { return proc.updateUserProgram (i); }
    bool deleteUserProgram (int i) override { return proc.deleteUserProgram (i); }
    bool renameProgram (int i, const juce::String& n) override { return proc.renameProgram (i, n); }
    bool setProgramCategory (int i, const juce::String& c) override { return proc.setProgramCategory (i, c); }
    bool saveUserBank() override { proc.saveUserBank(); return true; }
    bool canExportSysex() const override { return true; }
    juce::File exportProgramSysex (int i) override { return proc.exportProgramSysex (i); }

    bool   isSessionPlaying() const override { return proc.isSessionPlaying(); }
    void   setSessionPlaying (bool on) override { proc.setSessionPlaying (on); }
    bool   isSessionLoop() const override { return proc.isSessionLoop(); }
    void   setSessionLoop (bool l) override { proc.setSessionLoop (l); }
    int    sessionBars() const override { return proc.sessionBars(); }
    void   setSessionBars (int n) override { proc.setSessionBars (n); }
    double sessionPosBeats() const override { return proc.sessionPosBeats(); }

    /** AN EFFECT HAS TWO CLOCK STATES, NOT THREE. Its timing is always
        something - a delay has a delay time whether or not it is following a
        clock - so its panel offers only the SOURCE. A synth's LFOs and envelopes
        have their own rates and running them free against each other is a sound
        in its own right, so OFF has to be reachable there.

        The shell's default derives this from whether the unit has a keyboard,
        which is right for both templates; it is stated explicitly here because
        it is the one line that differs between a synth and an effect, and
        because some machines disagree with the default. */
    bool offersSyncOff() const override { return false; }

    std::vector<SyncTarget> syncTargets() const override
    {
        std::vector<SyncTarget> t;
        SyncTarget d;
        d.name = "DELAY"; d.kind = SyncTarget::time;
        d.defaultDivision = shell::TempoSync::quarter;
        d.lo = 1.0; d.hi = 2000.0;
        t.push_back (d);
        SyncTarget m;
        m.name = "MOD"; m.kind = SyncTarget::rate;
        m.defaultDivision = shell::TempoSync::whole;
        m.lo = 0.02; m.hi = 8.0;
        t.push_back (m);
        return t;
    }

    juce::String lastTouchedText() const override { return proc.lastTouched(); }
    juce::String buildStamp() const override { return SYNTHKIT_BUILD_VERSION; }
    juce::String brandLabel() const override { return "The Decipherist  ExampleFx"; }
    juce::AudioProcessorValueTreeState& parameters() override { return proc.apvts; }

private:
    ExampleFxProcessor& proc;
};

} // namespace examplefx
