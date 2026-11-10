#include "PluginEditor.h"
#include "BinaryData.h"

namespace examplesynth {

ExampleSynthEditor::ExampleSynthEditor (ExampleSynthProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), panel (SYNTHKIT_PANEL_ASSETS())
{
    loadBindings();
    panel.onChange = [this] (const synthkit::NativePanel::Event& e) { onPanelEvent (e); };
    addAndMakeVisible (panel);

    strip = std::make_unique<shell::SessionStrip> (proc.host());
    addAndMakeVisible (*strip);
    keys = std::make_unique<shell::KeyboardRow> (proc.host());
    addAndMakeVisible (*keys);

    // Arrow keys browse the Patch Explorer. Routed through the QWERTY listener,
    // NEVER by giving the explorer keyboard focus: a focused control is exactly
    // what silenced a whole synth once, when a BPM field took focus from the
    // on-screen keyboard.
    qwerty.onOtherKey = [this] (const juce::KeyPress& k)
    {
        if (strip == nullptr || ! strip->explorerVisible()) return false;
        if (k == juce::KeyPress::leftKey)  return strip->stepExplorer (-1, 0);
        if (k == juce::KeyPress::rightKey) return strip->stepExplorer (+1, 0);
        if (k == juce::KeyPress::upKey)    return strip->stepExplorer (0, -1);
        if (k == juce::KeyPress::downKey)  return strip->stepExplorer (0, +1);
        return false;
    };
    qwerty.suppressed = [this] { return strip != nullptr && strip->isEditingText(); };
    addKeyListener (&qwerty);
    setWantsKeyboardFocus (true);

    setResizable (true, true);
    const double aspect = (double) panel.designWidth() / (double) panel.designHeight();
    const int minW = 920;
    const int maxW = juce::jmax (panel.designWidth(), 2 * panel.designWidth());
    int openW = 1380;
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = d->userArea;
        // A MACHINE DEEPER THAN IT IS WIDE CANNOT BE FITTED BY NARROWING THE WINDOW.
        // The width is already floored at minW, and on such a panel that floor alone
        // puts the window off the bottom of the screen - the HR-16's 1200 x 1644 top
        // view wants 1585 px on a 982 px display. Only then does the panel get a band
        // and scroll inside it; every rack-front panel leaves maxBand at zero and is
        // laid out to the pixel it always was.
        const int room = (int) ((double) area.getHeight() * 0.94) - kShellDesign;
        if (panel.heightForWidth (minW) > room) maxBand = juce::jmax (240, room);

        const int byWidth  = (int) ((double) area.getWidth() * 0.94);
        const int byHeight = (int) (((double) area.getHeight() * 0.92 - kShellDesign) * aspect);
        // With a band there is no height to trade width against, so the window is as
        // wide as the screen allows and the panel is drawn as large as it can be.
        openW = juce::jlimit (minW, maxW, maxBand > 0 ? byWidth : juce::jmin (byWidth, byHeight));
    }
    getConstrainer()->setSizeLimits (minW, heightForWidth (minW), maxW, heightForWidth (maxW));
    setSize (openW, heightForWidth (openW));
    pushAllToPanel();
    startTimerHz (12);
}

ExampleSynthEditor::~ExampleSynthEditor()
{
    if (qwertyHost != nullptr && qwertyHost != this) qwertyHost->removeKeyListener (&qwerty);
    removeKeyListener (&qwerty);
}

void ExampleSynthEditor::parentHierarchyChanged()
{
    // The listener has to live on the TOP-LEVEL window, not only the editor: a
    // key event targets the focused component and walks UP its parents, so with
    // nothing focused the target is the window, which sits above the editor and
    // never reaches a listener on it. The tell is that the system beep stops
    // once you click the on-screen keyboard.
    auto* top = getTopLevelComponent();
    if (top == qwertyHost.getComponent()) return;
    if (qwertyHost != nullptr && qwertyHost != this) qwertyHost->removeKeyListener (&qwerty);
    qwertyHost = top;
    if (top != nullptr && top != this) top->addKeyListener (&qwerty);
    if (isShowing()) grabKeyboardFocus();
}

int ExampleSynthEditor::panelBand (int w) const
{
    const int want = panel.heightForWidth (w);
    return maxBand > 0 ? juce::jmin (want, maxBand) : want;
}

int ExampleSynthEditor::heightForWidth (int w) const
{
    return panelBand (w) + kShellDesign;
}

void ExampleSynthEditor::loadBindings()
{
    const auto json = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::bindings_json,
                                                                BinaryData::bindings_jsonSize));
    if (auto* obj = json.getProperty ("bindings", {}).getDynamicObject())
        for (const auto& kv : obj->getProperties())
        {
            Binding b;
            b.kind   = kv.value.getProperty ("kind", "").toString();
            b.param  = kv.value.getProperty ("param", "").toString();
            b.action = kv.value.getProperty ("action", "").toString();
            b.name   = kv.value.getProperty ("name", "").toString();
            b.stepOnPress = kv.value.getProperty ("press", "").toString() == "step";
            b.altParam  = kv.value.getProperty ("altParam", "").toString();
            b.altSelect = kv.value.getProperty ("altSelect", "").toString();
            b.altName   = kv.value.getProperty ("altName", "").toString();
            if (b.kind == "select") selectors[kv.name.toString()] = false;
            bindings[kv.name.toString()] = b;
        }
}

juce::StringArray ExampleSynthEditor::devControlIds() const
{
    juce::StringArray ids;
    for (const auto& [id, b] : bindings)
        if (b.kind != "screen") ids.add (id);
    return ids;
}

bool ExampleSynthEditor::devPanel (const juce::String& id, bool isValue, float value)
{
    const auto it = bindings.find (id);
    if (it == bindings.end()) return false;
    if (it->second.kind == "screen") return false;
    onPanelEvent ({ id, isValue, value });
    return true;
}

juce::String ExampleSynthEditor::liveParam (const Binding& b) const
{
    if (b.altParam.isNotEmpty() && b.altSelect.isNotEmpty())
    {
        const auto s = selectors.find (b.altSelect);
        if (s != selectors.end() && s->second) return b.altParam;
    }
    return b.param;
}

void ExampleSynthEditor::onPanelEvent (const synthkit::NativePanel::Event& ev)
{
    const auto it = bindings.find (ev.id);
    if (it == bindings.end()) return;
    const auto& b = it->second;
    if (b.kind == "select")
    {
        // An edit-mode selector: it writes no parameter and only changes which
        // one the control beside it reaches. Its own state is SESSION state, not
        // program data. Write down which, in docs/laws.md.
        auto& on = selectors[ev.id];
        on = ! on;
        panel.setLit (ev.id, on);
        return;
    }

    if (b.kind == "value")
    {
        const auto pid = liveParam (b);
        auto* q = proc.apvts.getParameter (pid);
        if (q == nullptr) return;
        float v = juce::jlimit (0.0f, 1.0f, ev.value);
        if (! ev.hasValue)
        {
            // A CAP, clicked. A click carries no value, so writing ev.value here
            // sets every toggle to 0 and a toggle only ever turns OFF. It STEPS
            // instead, and wraps.
            if (! b.stepOnPress) return;
            const auto* rp = dynamic_cast<const juce::RangedAudioParameter*> (q);
            const int steps = rp != nullptr ? juce::jmax (1, rp->getNumSteps() - 1) : 1;
            v = q->getValue() + 1.0f / (float) steps;
            if (v > 1.0f + 1.0e-4f) v = 0.0f;
            v = juce::jlimit (0.0f, 1.0f, v);
        }
        q->setValueNotifyingHost (v);
        panel.setValue (ev.id, v);
        panel.setLit (ev.id, v > 0.5f);
        panel.setScreen (b.altParam.isNotEmpty() && pid == b.altParam ? b.altName : b.name,
                         q->getCurrentValueAsText());
        return;
    }
    if (b.kind == "button")
        doAction (b.action);
}

void ExampleSynthEditor::doAction (const juce::String& action)
{
    const int n = juce::jmax (1, (int) proc.programs().size());
    if (action == "program.up")   { proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % n); return; }
    if (action == "program.down") { proc.setCurrentProgram ((proc.getCurrentProgram() - 1 + n) % n); return; }
    if (action == "program.write"){ if (strip != nullptr) strip->showExplorer ("USER"); return; }
    if (action == "panic")        { proc.keyboardState.allNotesOff (1); return; }
}

void ExampleSynthEditor::pushAllToPanel()
{
    for (const auto& [id, b] : bindings)
        if (b.kind == "value")
            if (auto* q = proc.apvts.getParameter (liveParam (b)))
            {
                panel.setValue (id, q->getValue());
                // The lamp follows the PARAMETER. A cap lit from anything else
                // can be right while the button is dead - the TC 2290's five EXT
                // EFFECTS buttons were lit by the panel refresh and had no click
                // handler at all.
                if (b.stepOnPress) panel.setLit (id, q->getValue() > 0.5f);
            }
    const auto& progs = proc.programs();
    const int i = proc.getCurrentProgram();
    if (i >= 0 && i < (int) progs.size())
        panel.setScreen (juce::String (progs[(size_t) i].name),
                         juce::String (progs[(size_t) i].id) + "   "
                           + juce::String (progs[(size_t) i].category));
}

void ExampleSynthEditor::timerCallback()
{
    if (strip != nullptr && strip->isEditingText()) qwerty.releaseAllKeys();
    else                                            qwerty.tick();
    if (proc.programVersion() != lastProgramVer || proc.getCurrentProgram() != lastProgram)
    {
        lastProgramVer = proc.programVersion();
        lastProgram = proc.getCurrentProgram();
        pushAllToPanel();
    }
}

void ExampleSynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14161a));
    const int panelH = (int) ((double) getWidth() * panel.designHeight() / panel.designWidth());
    g.setColour (juce::Colour (0xffe0a34a));
    g.fillRect (0, panelH, getWidth(), juce::jmax (1, getHeight() / 400));
}

void ExampleSynthEditor::resized()
{
    const int panelH = panelBand (getWidth());
    panel.setBounds (0, 0, getWidth(), panelH);
    const int divider = juce::jmax (1, getHeight() / 400);
    const int top = panelH + divider;
    const int rest = juce::jmax (0, getHeight() - top);
    const int stripH = rest * 7 / 10;          // section 19: 70 percent of what is left
    strip->setBounds (0, top, getWidth(), stripH);
    keys->setBounds (0, top + stripH, getWidth(), rest - stripH);
}

} // namespace examplesynth
