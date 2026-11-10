#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/ShellAdapter.h"
#include <cmath>

namespace examplefx {

static constexpr const char* kBpmId = "bpm";

juce::StringArray ExampleFxProcessor::parameterIds()
{
    juce::StringArray ids;
    for (int i = 0; i < kNumParams; ++i) ids.add (kParams[i].id);
    ids.add (kBpmId);
    return ids;
}

juce::AudioProcessorValueTreeState::ParameterLayout ExampleFxProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (int i = 0; i < kNumParams; ++i)
    {
        const auto& p = kParams[i];
        // A `choice` row is an INTEGER parameter, and that is not cosmetic: a cap
        // bound to a parameter STEPS it on a click, and a step needs a step
        // count. Registered as a plain float, a six-position menu has 2^31 steps
        // and one click moves it by nothing at all.
        if (juce::String (p.kind) == "choice")
            l.add (std::make_unique<juce::AudioParameterInt> (
                juce::ParameterID { p.id, 1 }, p.name,
                (int) p.minimum, (int) p.maximum, (int) p.defaultValue));
        else
            l.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { p.id, 1 }, p.name,
                juce::NormalisableRange<float> { p.minimum, p.maximum }, p.defaultValue));
    }
    l.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kBpmId, 1 }, "Tempo",
        juce::NormalisableRange<float> { 20.0f, 300.0f }, 120.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); })
            .withLabel ("BPM")));
    return l;
}

ExampleFxProcessor::ExampleFxProcessor()
    : juce::AudioProcessor (BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "state", makeLayout())
{
    // Every parameter, so no writer can be forgotten: the editor and the
    // host both write straight to the parameter and never touch the flag.
    for (const auto& p : kParams)
        apvts.addParameterListener (p.id, this);

    bank = factoryBank();
    loadUserBank();
    shellHost = std::make_unique<ShellAdapter> (*this);
    setCurrentProgram (0);
}

ExampleFxProcessor::~ExampleFxProcessor()
{
    for (const auto& p : kParams)
        apvts.removeParameterListener (p.id, this);
}

float ExampleFxProcessor::paramValue (int index) const
{
    if (index < 0 || index >= kNumParams) return 0.0f;
    if (auto* v = apvts.getRawParameterValue (kParams[index].id)) return v->load();
    return kParams[index].defaultValue;
}

void ExampleFxProcessor::setParamValue (int index, float v, const juce::String& touched)
{
    if (index < 0 || index >= kNumParams) return;
    if (auto* p = apvts.getParameter (kParams[index].id))
        p->setValueNotifyingHost (p->convertTo0to1 (clampStored (index, v)));
    if (touched.isNotEmpty()) lastTouchedName = touched;
    coeffsDirty.store (true);
}

void ExampleFxProcessor::setCurrentProgram (int index)
{
    if (bank.empty()) return;
    current = juce::jlimit (0, (int) bank.size() - 1, index);
    const auto& p = bank[(size_t) current];
    for (int i = 0; i < kNumParams; ++i)
        if (auto* q = apvts.getParameter (kParams[i].id))
            q->setValueNotifyingHost (q->convertTo0to1 (clampStored (i, p.value[i])));
    // A PROGRAM LOAD MUST NOT MOVE THE TEMPO. Section 18 is strict about the one
    // clock: a stored setup's BPM is informational, shown in the explorer and
    // the inspector, and never pushed into the clock.
    ++programVer;
    coeffsDirty.store (true);
}

void ExampleFxProcessor::prepareToPlay (double sampleRate, int blockSize)
{
    sr = sampleRate;
    engine.prepare (sampleRate, blockSize);
    host().prepareClock (sampleRate);
    host().prepareAudition (sampleRate);
    coeffsDirty.store (true);
}

void ExampleFxProcessor::pushCoeffs()
{
    Program live = bank.empty() ? Program{} : bank[(size_t) current];
    for (int i = 0; i < kNumParams; ++i) live.value[i] = paramValue (i);
    engine.setCoeffs (makeCoeffs (live));
}

void ExampleFxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    // The whole of this unit's transport code, and it is the first line. Nothing
    // below writes the resolved tempo back into the `bpm` parameter - that is
    // what turns SYNC INTERNAL into the host's tempo, and it was found in three
    // units at once.
    host().readTransport (getPlayHead(), midi, n);
    const double bpm = host().hostTempo();

    if (coeffsDirty.exchange (false)) pushCoeffs();
    {
        auto c = engine.currentCoeffs();
        c.modPhaseAnchor = -1.0f;
        if (const double ms = host().syncedMs (kDelayTime); ms > 0.0)
            c.delayMs = (float) juce::jlimit (1.0, 2000.0, ms);
        if (const double hz = host().syncedHz (kModRate); hz > 0.0)
        {
            c.modRateHz = (float) juce::jlimit (0.02, 8.0, hz);
            // syncPhase, NEVER clock().phase(): the master runs at hostTempo()
            // and syncedHz above follows syncTempo(), so anchoring to the master
            // re-pins the modulator to the host under SYNC INTERNAL and silently
            // overrides the rate. (shell/ShellHost.h says so at syncPhase.)
            c.modPhaseAnchor = (float) host().syncPhase (kModRate);
        }
        engine.setCoeffs (c);
    }

    // The audition source SUMS into the real input, and it goes through
    // begin/endAudition so the strip's MIX cell works: 0 is the bare source, 1
    // is what the box made of it. Without that you hear the source whatever you
    // do, which is what "I don't hear a huge difference in any of the patches"
    // turned out to be on two machines.
    float* const* ch = buffer.getArrayOfWritePointers();
    if (isSessionPlaying())
        host().beginAudition (ch, buffer.getNumChannels(), n, /* addTo */ true);

    if (! bypassed.load())
    {
        if (buffer.getNumChannels() >= 2)
            engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
        else if (buffer.getNumChannels() == 1)
        {
            std::vector<float> scratch ((size_t) n);
            std::copy_n (buffer.getReadPointer (0), n, scratch.begin());
            engine.process (buffer.getWritePointer (0), scratch.data(), n);
        }
    }

    if (isSessionPlaying())
    {
        host().endAudition (ch, buffer.getNumChannels(), n);
        const double beatsPerSample = bpm / (60.0 * sr);
        double pos = sessPos.load() + (double) n * beatsPerSample;
        const double total = 4.0 * (double) sessBars.load();
        if (pos >= total) pos = sessLoop.load() ? std::fmod (pos, total) : 0.0;
        sessPos.store (pos);
    }
    midi.clear();
}

juce::File ExampleFxProcessor::supportDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("ExampleFx");
}

void ExampleFxProcessor::loadUserBank()
{
    const auto f = supportDir().getChildFile ("user-bank.json");
    if (! f.existsAsFile()) return;
    const auto v = juce::JSON::parse (f);
    if (auto* arr = v.getArray())
        for (const auto& e : *arr)
        {
            Program p;
            p.id = e.getProperty ("id", "U00").toString().toStdString();
            p.name = e.getProperty ("name", "User").toString().toStdString();
            p.category = e.getProperty ("category", "Other").toString().toStdString();
            p.bank = "USER";
            for (int i = 0; i < kNumParams; ++i)
                p.value[i] = clampStored (i, (float) (double) e.getProperty (kParams[i].id,
                                                                            kParams[i].defaultValue));
            bank.push_back (p);
        }
}

void ExampleFxProcessor::saveUserBank()
{
    juce::Array<juce::var> arr;
    for (const auto& p : bank)
    {
        if (p.bank != "USER") continue;
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", juce::String (p.id));
        o->setProperty ("name", juce::String (p.name));
        o->setProperty ("category", juce::String (p.category));
        for (int i = 0; i < kNumParams; ++i) o->setProperty (kParams[i].id, p.value[i]);
        arr.add (juce::var (o));
    }
    supportDir().createDirectory();
    supportDir().getChildFile ("user-bank.json").replaceWithText (juce::JSON::toString (juce::var (arr)));
}

int ExampleFxProcessor::addUserProgram (const juce::String& name, const juce::String& category)
{
    Program p;
    for (int i = 0; i < kNumParams; ++i) p.value[i] = paramValue (i);
    p.id = "U" + juce::String (bank.size()).paddedLeft ('0', 2).toStdString();
    p.name = name.isEmpty() ? "User" : name.toStdString();
    p.category = category.toStdString();
    p.bank = "USER";
    p.raw = bank.empty() ? std::vector<uint8_t>{} : bank[(size_t) current].raw;
    bank.push_back (p);
    ++programVer;
    current = (int) bank.size() - 1;
    return current;
}

bool ExampleFxProcessor::updateUserProgram (int index)
{
    if (index < 0 || index >= (int) bank.size() || bank[(size_t) index].bank != "USER") return false;
    for (int i = 0; i < kNumParams; ++i) bank[(size_t) index].value[i] = paramValue (i);
    ++programVer;
    return true;
}

bool ExampleFxProcessor::deleteUserProgram (int index)
{
    if (index < 0 || index >= (int) bank.size() || bank[(size_t) index].bank != "USER") return false;
    bank.erase (bank.begin() + index);
    current = juce::jlimit (0, (int) bank.size() - 1, current);
    ++programVer;
    return true;
}

bool ExampleFxProcessor::renameProgram (int index, const juce::String& name)
{
    if (index < 0 || index >= (int) bank.size() || bank[(size_t) index].bank != "USER") return false;
    bank[(size_t) index].name = name.toStdString();
    ++programVer;
    return true;
}

bool ExampleFxProcessor::setProgramCategory (int index, const juce::String& c)
{
    if (index < 0 || index >= (int) bank.size()) return false;
    bank[(size_t) index].category = c.toStdString();
    ++programVer;
    return true;
}

juce::File ExampleFxProcessor::exportProgramSysex (int index)
{
    if (index < 0 || index >= (int) bank.size()) return {};
    const auto bytes = toRecord (bank[(size_t) index]);
    auto dir = supportDir().getChildFile ("export");
    dir.createDirectory();
    auto f = dir.getChildFile (juce::String (bank[(size_t) index].name).trim()
                                 .replaceCharacter (' ', '_') + ".syx");
    f.replaceWithData (bytes.data(), bytes.size());
    return f;
}

void ExampleFxProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", current, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ExampleFxProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    apvts.replaceState (state);
    current = juce::jlimit (0, juce::jmax (0, (int) bank.size() - 1), (int) state.getProperty ("program", 0));
    coeffsDirty.store (true);
}

juce::AudioProcessorEditor* ExampleFxProcessor::createEditor()
{
    return new ExampleFxEditor (*this);
}

} // namespace examplefx

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new examplefx::ExampleFxProcessor();
}
