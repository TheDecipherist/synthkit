// The parameter layer for the effects variant: patch state, the coefficient
// builder, the master clock, and the audition source.
//
// The differences from the synth template are exactly three, and they are the
// ones the shell cares about: SessionStrip::Layout::Effects (no CHORDS, ARP or
// CHOPPER; an AUDITION tab instead), no keyboard row, and offersSyncOff() false,
// because an effect's timing is always SOMETHING - a delay has a delay time
// whether or not it follows a clock - so its panel offers only the source.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <atomic>
#include <vector>
#include "engine/Engine.h"
#include "engine/Coeffs.h"
#include "patch/Program.h"

namespace examplefx {

class ShellAdapter;

class ExampleFxProcessor : public juce::AudioProcessor,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    ExampleFxProcessor();
    ~ExampleFxProcessor() override;

    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "ExampleFx"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return juce::jmax (1, (int) bank.size()); }
    int getCurrentProgram() override { return current; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int i) override
    { return i >= 0 && i < (int) bank.size() ? juce::String (bank[(size_t) i].name) : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    [[nodiscard]] const std::vector<Program>& programs() const noexcept { return bank; }
    [[nodiscard]] int programVersion() const noexcept { return programVer.load(); }

    int  addUserProgram (const juce::String& name, const juce::String& category);
    bool updateUserProgram (int index);
    bool deleteUserProgram (int index);
    bool renameProgram (int index, const juce::String& name);
    bool setProgramCategory (int index, const juce::String& category);
    void saveUserBank();
    juce::File exportProgramSysex (int index);
    static juce::File supportDir();

    [[nodiscard]] float paramValue (int index) const;
    void setParamValue (int index, float v, const juce::String& touched = {});
    [[nodiscard]] juce::String lastTouched() const { return lastTouchedName; }

    bool isSessionPlaying() const noexcept { return sessPlaying.load(); }
    void setSessionPlaying (bool on) noexcept { sessPlaying.store (on); }
    bool isSessionLoop() const noexcept { return sessLoop.load(); }
    void setSessionLoop (bool on) noexcept { sessLoop.store (on); }
    int  sessionBars() const noexcept { return sessBars.load(); }
    void setSessionBars (int n) noexcept { sessBars.store (juce::jlimit (1, 8, n)); }
    double sessionPosBeats() const noexcept { return sessPos.load(); }

    ShellAdapter& host() noexcept { return *shellHost; }

    /** FX BYPASS: an A/B tool, NOT patch state, so it deliberately survives a
        program change. Stored, live or selector - every control is one of three
        things and mixing them up cost a real machine a day of chased bugs. Only
        program data resets on load. */
    bool isBypassed() const noexcept { return bypassed.load(); }
    void setBypassed (bool b) noexcept { bypassed.store (b); }

    enum SyncTarget { kDelayTime = 0, kModRate = 1, kNumSyncTargets };

    juce::AudioProcessorValueTreeState apvts;

    static juce::StringArray parameterIds();

private:
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void pushCoeffs();
    void loadUserBank();

    Engine engine;
    std::vector<Program> bank;
    int current = 0;
    std::atomic<int> programVer { 0 };
    juce::String lastTouchedName;
    std::unique_ptr<ShellAdapter> shellHost;

    std::atomic<bool> sessPlaying { false }, sessLoop { true };
    std::atomic<int> sessBars { 2 };
    std::atomic<double> sessPos { 0.0 };
    std::atomic<bool> coeffsDirty { true }, bypassed { false };
    double sr = 48000.0;

    /** The listener the effect template declared and never implemented, so the
        template did not compile at all from 2026-09-09 until the Ebbe und Flut
        was generated from it. The synth template has always carried this; the
        effect one inherited the Listener base and stopped there.

        Why a listener and not a dirty flag set by the editor: a dirty flag is an
        OPTIMISATION, and the skeleton once shipped with none of the
        invalidation, so a host automating a parameter changed the value, moved
        the control on screen, updated the readout, and left the engine on the
        coefficients it computed at startup. */
    void parameterChanged (const juce::String&, float) override
    { coeffsDirty.store (true); }


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExampleFxProcessor)
};

} // namespace examplefx
