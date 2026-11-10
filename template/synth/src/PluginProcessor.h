// The parameter layer: patch state, the coefficient builder, the master clock,
// and the session player. It is the ONLY thing that knows what a program is.
//
// The split is the hardware's own, and it is why the originals ran eight parts
// with no polyphony cost: the host CPU computed every coefficient and the DSPs
// held only blocks. Engine never sees a Program.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <shell/SessionState.h>
#include <shell/DelayDesignerEngine.h>
#include <atomic>
#include <vector>
#include "engine/Engine.h"
#include "engine/Coeffs.h"
#include "patch/Program.h"

namespace examplesynth {

class ShellAdapter;

class ExampleSynthProcessor : public juce::AudioProcessor,
                              private juce::AudioProcessorValueTreeState::Listener
{
public:
    ExampleSynthProcessor();
    ~ExampleSynthProcessor() override;

    // ---- JUCE ------------------------------------------------------------
    void prepareToPlay (double sampleRate, int blockSize) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "ExampleSynth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return juce::jmax (1, (int) bank.size()); }
    int getCurrentProgram() override { return current; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int i) override
    { return i >= 0 && i < (int) bank.size() ? juce::String (bank[(size_t) i].name) : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- programs --------------------------------------------------------
    [[nodiscard]] const std::vector<Program>& programs() const noexcept { return bank; }
    [[nodiscard]] const Program& currentPatch() const noexcept { return bank[(size_t) current]; }
    [[nodiscard]] int  programVersion() const noexcept { return programVer.load(); }
    /** How many stored values had to be clamped on load, across the whole bank.
        Must be zero. 383 of the PCM 91's 433 presets failed this silently. */
    [[nodiscard]] int  clampedOnLoad() const noexcept { return clamped; }

    int  addUserProgram (const juce::String& name, const juce::String& category);
    bool updateUserProgram (int index);
    bool deleteUserProgram (int index);
    bool renameProgram (int index, const juce::String& name);
    bool setProgramCategory (int index, const juce::String& category);
    void saveUserBank();
    juce::File exportProgramSysex (int index);
    static juce::File supportDir();

    // ---- edit buffer -----------------------------------------------------
    [[nodiscard]] float paramValue (int index) const;
    void setParamValue (int index, float v, const juce::String& touched = {});
    [[nodiscard]] juce::String lastTouched() const { return lastTouchedName; }

    // ---- the session (chords / arp / chopper), on the ONE master clock ----
    shell::SessionState& sessionEdit() noexcept { return sessionUi; }
    void publishSession();
    bool isSessionPlaying() const noexcept { return sessPlaying.load(); }
    void setSessionPlaying (bool on) noexcept { sessPlaying.store (on); }
    bool isSessionLoop() const noexcept { return sessLoop.load(); }
    void setSessionLoop (bool on) noexcept { sessLoop.store (on); }
    int  sessionBars() const noexcept { return sessBars.load(); }
    void setSessionBars (int n) noexcept { sessBars.store (juce::jlimit (1, 8, n)); }
    double sessionPosBeats() const noexcept { return sessPos.load(); }
    int  arpDisplayStep() const noexcept { return arpStepDisplay.load(); }

    void setUiPitchBend (float v) { uiBend.store (v); }
    void setUiModWheel (float v) { uiMod.store (v); }

    /** The shell host, and the only thing that resolves a tempo. Public because
        the probes drive it exactly as the editor does. */
    ShellAdapter& host() noexcept { return *shellHost; }

    /** The destinations this machine slaves to the clock, named once here so the
        adapter, the probe and the engine cannot disagree about the order. */
    enum SyncTarget { kFilterLfo = 0, kDelayTime = 1, kNumSyncTargets };

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;

    /** The frozen parameter-ID list, in registration order. Freeze at v0.1 and
        never renumber: hosts persist automation by ID and renumbering silently
        breaks every existing project. tools/tests_main.cpp asserts this list. */
    static juce::StringArray parameterIds();

private:
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void pushCoeffs();
    void runSession (juce::MidiBuffer&, int numSamples, double bpm);
    void loadUserBank();

    Engine engine;
    std::vector<Program> bank;
    int current = 0;
    int clamped = 0;
    std::atomic<int> programVer { 0 };
    juce::String lastTouchedName;

    std::unique_ptr<ShellAdapter> shellHost;

    // double buffer: edited on the message thread, read on the audio thread
    shell::SessionState sessionUi, sessionRt[2];
    // The DELAY DESIGNER is a shared block and a shared pattern, so the unit's
    // whole share of it is this member, one prepare and one process call. If a
    // machine ever needs more than that, the fix belongs in synthshell.
    shell::DelayDesignerEngine delayDesigner;
    std::atomic<int> sessIdx { 0 };
    std::atomic<bool> sessPlaying { false }, sessLoop { true };
    std::atomic<int> sessBars { 2 }, arpStepDisplay { -1 };
    std::atomic<double> sessPos { 0.0 };
    std::atomic<float> uiBend { 0.0f }, uiMod { 0.0f };
    std::atomic<bool> coeffsDirty { true };

    /** ANY write to ANY parameter marks the coefficients stale.

        This MUST be a listener. It cannot live in the processor's own setter,
        because that setter is not the only writer and never was: the EDITOR
        writes straight to the parameter, the HOST writes when it plays back
        automation, and a preset load writes a hundred at once.

        Found the hard way on SuperClassic, the first machine generated from
        this skeleton. With the flag set only in setParamValue, every knob turn
        changed the parameter, moved the control on screen, updated the readout
        - and left the engine on the coefficients it computed at startup. The UI
        worked perfectly and the sound did not move.

        Note what this skeleton had done: a dirty flag is an OPTIMISATION, and
        it shipped with none of the invalidation. Every hand-built unit in this
        project is safe because it either re-reads its parameters every block
        (Arcturus) or registers a listener (JP-8000, Virus, M1, TC 2290). The
        skeleton invented a third way that has the cost of both and the safety
        of neither. */
    void parameterChanged (const juce::String&, float) override
    { coeffsDirty.store (true); }

    double sr = 48000.0;
    int chordSounding[shell::SessionState::kMaxChordNotes] { -1, -1, -1, -1, -1 };
    int arpNoteSounding = -1, chordSlotCur = -1, arpStepCur = -1;
    double arpGateEnd = 0.0;
    bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExampleSynthProcessor)
};

} // namespace examplesynth
