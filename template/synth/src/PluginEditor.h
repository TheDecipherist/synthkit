// The window, top to bottom: the panel at its own aspect ratio, a 1 px divider,
// the session strip at 70% of what is left, then the performance row.
//
// Every part below the divider is the SHARED shell. Nothing here draws a
// keyboard, an arp grid, a chord wheel or a patch browser, and nothing here may
// start to.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <shell/SessionStrip.h>
#include <shell/KeyboardRow.h>
#include <shell/QwertyPlayer.h>
#include <map>
#include "PluginProcessor.h"
#include "ui/ShellAdapter.h"
#include <NativePanel.h>

namespace examplesynth {

/** One panel control's meaning, read from panel/bindings.json.

    The file is DATA and the editor reads it. It is not compiled in and ignored,
    which is what the Reverb 4000 did: its bindings.json was in the binary and
    read by no code at all, the real binding was a hard-coded if chain, and 68 of
    189 rows were silent with nothing able to notice. */
struct Binding
{
    juce::String kind;       // value | button | select | screen
    juce::String param;      // the plugin parameter id, for kind == value
    juce::String action;     // for kind == button
    juce::String name;

    /** A CAP bound to a parameter is CLICKED, not dragged, so a click carries no
        value. Writing that 0 straight to the parameter makes every toggle turn
        OFF and nothing else; `press: "step"` in the bindings makes a click
        ADVANCE the parameter and wrap, which is also what a menu button on a
        real panel does. Registering the parameter as an AudioParameterInt is the
        other half - a step needs a step count. */
    bool stepOnPress = false;

    /** ONE KNOB, TWO PARAMETERS. A panel legend reading TUNE / FINE, or a SHIFT
        key, is one control that edits either, chosen by a selector beside it.
        Without this the second parameter is an orphan with no control, which is
        the inverse of a dead knob and just as easy to miss: the Reverb 4000 had
        eleven, including Attack, Hold and Release, which were already tempo-sync
        destinations, so the clock could drive a gate the user had no way to set.
        The controls probe counts `altParam` as reachable. */
    juce::String altParam, altSelect, altName;
};

class ExampleSynthEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit ExampleSynthEditor (ExampleSynthProcessor&);
    ~ExampleSynthEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

    /** Window height for a width, in ONE place, so the snapshot renders at the
        size the window opens to and a legibility check on the PNG means
        something. The shell is a CONSTANT number of window pixels, not a scaled
        fraction: every pixel of shell is a pixel off the panel, one for one. */
    [[nodiscard]] int heightForWidth (int w) const;

    /** Dev entry points. controls_probe drives THESE, which are the same calls a
        mouse reaches - a code-reading audit cannot tell a live control from a
        dead one, so the probe has to move the real thing and measure the audio. */
    bool devPanel (const juce::String& id, bool isValue, float value);
    [[nodiscard]] juce::StringArray devControlIds() const;
    /** The panel itself, so the probe can ask where each control is DRAWN and what a click
        there actually reaches. Driving by id cannot answer that question. */
    [[nodiscard]] const synthkit::NativePanel& devPanelView() const noexcept { return panel; }
    void devMoveHit (const juce::String& id, float dx, float dy) { panel.devMoveHit (id, dx, dy); }
    void showStripTab (int t) { if (strip != nullptr) strip->setTab (t); }

private:
    void timerCallback() override;
    void loadBindings();
    void onPanelEvent (const synthkit::NativePanel::Event&);
    void pushAllToPanel();
    void doAction (const juce::String& action);
    /** Which parameter a control writes right now, following any selector. */
    [[nodiscard]] juce::String liveParam (const Binding&) const;

    static constexpr int kShellDesign = 460;   // window pixels under the panel


    /** THE BAND THE PANEL GETS, which is its own aspect until that would put the
        window off the bottom of the screen. Past that the panel keeps its scale and
        scrolls inside the band - synthkit's viewport - instead of the window growing
        to fit a machine that is deeper than it is wide. `maxBand` is 0, and this is
        the height it always was, unless the window genuinely cannot be made to fit
        by choosing a width. */
    [[nodiscard]] int panelBand (int w) const;
    int maxBand = 0;
    ExampleSynthProcessor& proc;
    synthkit::NativePanel panel;
    std::map<juce::String, Binding> bindings;
    /** Edit-mode selectors: id -> engaged. A selector writes no parameter; it
        changes which one a knob beside it writes. */
    std::map<juce::String, bool> selectors;

    std::unique_ptr<shell::SessionStrip> strip;
    std::unique_ptr<shell::KeyboardRow>  keys;
    shell::QwertyPlayer qwerty { proc.keyboardState };
    juce::Component::SafePointer<juce::Component> qwertyHost;
    int lastProgramVer = -1, lastProgram = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExampleSynthEditor)
};

} // namespace examplesynth
