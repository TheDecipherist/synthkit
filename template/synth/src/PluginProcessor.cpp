#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/ShellAdapter.h"
#include <algorithm>
#include <cmath>

namespace examplesynth {

// The two parameters the shell itself binds by name. `bpm` IS the master clock -
// the one value a host automates and the one hostTempo() reads - and `transpose`
// is the performance row's semitone slider. Neither belongs to the machine's
// patch record, so neither is reset by a program change.
static constexpr const char* kBpmId = "bpm";
static constexpr const char* kTransposeId = "transpose";

juce::StringArray ExampleSynthProcessor::parameterIds()
{
    juce::StringArray ids;
    for (int i = 0; i < kNumParams; ++i) ids.add (kParams[i].id);
    ids.add (kBpmId);
    ids.add (kTransposeId);
    return ids;
}

juce::AudioProcessorValueTreeState::ParameterLayout ExampleSynthProcessor::makeLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    // Straight from the generated table, so a range exists in exactly one place.
    // A range that lives in both a spec and a registration WILL disagree, and you
    // cannot tell by reading which of the two is decorative (playbook 17a, third
    // occurrence in one repo).
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
        // One decimal, always: a plain continuous float prints "120.0000000 BPM"
        // in the strip's field, because a JUCE SliderAttachment installs the
        // PARAMETER's text function over the slider's.
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); })
            .withLabel ("BPM")));
    l.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { kTransposeId, 1 }, "Transpose", -24, 24, 0));
    return l;
}

ExampleSynthProcessor::ExampleSynthProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
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

ExampleSynthProcessor::~ExampleSynthProcessor()
{
    for (const auto& p : kParams)
        apvts.removeParameterListener (p.id, this);
}

// ---------------------------------------------------------------------------
float ExampleSynthProcessor::paramValue (int index) const
{
    if (index < 0 || index >= kNumParams) return 0.0f;
    if (auto* v = apvts.getRawParameterValue (kParams[index].id)) return v->load();
    return kParams[index].defaultValue;
}

void ExampleSynthProcessor::setParamValue (int index, float v, const juce::String& touched)
{
    if (index < 0 || index >= kNumParams) return;
    if (auto* p = apvts.getParameter (kParams[index].id))
        p->setValueNotifyingHost (p->convertTo0to1 (clampStored (index, v)));
    if (touched.isNotEmpty()) lastTouchedName = touched;
    coeffsDirty.store (true);
}

void ExampleSynthProcessor::setCurrentProgram (int index)
{
    if (bank.empty()) return;
    current = juce::jlimit (0, (int) bank.size() - 1, index);
    const auto& p = bank[(size_t) current];
    for (int i = 0; i < kNumParams; ++i)
    {
        bool moved = false;
        const float v = clampStored (i, p.value[i], &moved);
        if (moved) ++clamped;
        if (auto* q = apvts.getParameter (kParams[i].id))
            q->setValueNotifyingHost (q->convertTo0to1 (v));
    }
    // NOT bpm and NOT transpose: the master clock is excluded from the
    // per-program reset. A program change on a machine that stores its own tempo
    // MOVES the clock - and does not do so while an external master is running -
    // but this machine stores no tempo, so it leaves the clock alone.
    ++programVer;
    coeffsDirty.store (true);
}

// ---------------------------------------------------------------------------
void ExampleSynthProcessor::prepareToPlay (double sampleRate, int blockSize)
{
    sr = sampleRate;
    engine.prepare (sampleRate, blockSize);
    host().prepareClock (sampleRate);
    delayDesigner.prepare (sampleRate);
    keyboardState.reset();
    coeffsDirty.store (true);
}

void ExampleSynthProcessor::pushCoeffs()
{
    Program live = bank.empty() ? Program{} : bank[(size_t) current];
    for (int i = 0; i < kNumParams; ++i) live.value[i] = paramValue (i);
    engine.setCoeffs (makeCoeffs (live));
}

void ExampleSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    // ONE LINE, and it is the first: the whole of this unit's transport code.
    // After it, hostTempo() and syncedMs()/syncedHz() are correct, whether the
    // master is a DAW playhead or 0xF8 bytes on a cable.
    //
    // NOTE WHAT IS NOT HERE: nothing writes the resolved tempo back into the
    // `bpm` parameter. A processor doing bpm.store(tempo) each block stores the
    // DAW's tempo into the field that IS the machine's internal clock, so SYNC
    // INTERNAL quietly becomes the host's tempo. It looks perfect under every
    // external measurement, because the write-back is what makes external work.
    // Found in three units at once.
    host().readTransport (getPlayHead(), midi, n);
    const double bpm = host().hostTempo();

    if (coeffsDirty.exchange (false))
        pushCoeffs();

    // Sync OVERRIDES the knob, and the time is built from the LIVE tempo before
    // the block's parameter pull - otherwise the delay follows the clock one
    // block late, and not at all on the block the tempo moved.
    {
        auto c = engine.currentCoeffs();
        c.lfoPhaseAnchor = -1.0f;
        if (const double hz = host().syncedHz (kFilterLfo); hz > 0.0)
        {
            c.lfoRateHz = (float) juce::jlimit (0.05, 20.0, hz);
            // Take the PHASE from the clock, not a rate converted to Hz. A
            // derived phase cannot drift from the master, and it puts the
            // modulator ON the beat rather than merely at the right speed.
            //
            // syncPhase, NEVER clock().phase(). The master clock advances at
            // hostTempo(), which is the DAW's while a DAW plays, and syncedHz
            // above follows syncTempo(), which under SYNC INTERNAL is the
            // machine's own. Anchoring straight to the master re-pins the
            // modulator to the host every block and silently overrides the rate
            // we just set - the Polysix's MG measured the host's 666.7 ms while
            // its own field said 120 BPM. The two tempos are the same number in
            // every ordinary case and differ in exactly the one that matters.
            c.lfoPhaseAnchor = (float) host().syncPhase (kFilterLfo);
        }
        if (const double ms = host().syncedMs (kDelayTime); ms > 0.0)
            c.delayMs = (float) juce::jlimit (10.0, 1500.0, ms);
        engine.setCoeffs (c);
    }

    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    runSession (midi, n, bpm);

    const int transpose = (int) apvts.getRawParameterValue (kTransposeId)->load();
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())
            engine.noteOn (juce::jlimit (0, 127, m.getNoteNumber() + transpose), m.getFloatVelocity());
        else if (m.isNoteOff())
            engine.noteOff (juce::jlimit (0, 127, m.getNoteNumber() + transpose));
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            engine.allNotesOff();
    }

    buffer.clear();
    if (buffer.getNumChannels() >= 2)
        engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
    else if (buffer.getNumChannels() == 1)
    {
        std::vector<float> scratch ((size_t) n, 0.0f);
        engine.process (buffer.getWritePointer (0), scratch.data(), n);
    }

    // The chopper: one bar of 16ths shaping the output level, beat-locked to the
    // same clock, applied post-everything.
    const auto& ss = sessionRt[(size_t) sessIdx.load (std::memory_order_acquire)];

    // The DELAY DESIGNER. Two lanes of hand-placed taps, each tap with its own
    // time, feedback, volume, decay and pan, built from the LIVE tempo - which
    // is what makes it follow a tempo change rather than only a division
    // change. Before the chopper, because the chopper is post-everything.
    //
    // This is the unit's ENTIRE share of the feature: a member, a prepare and
    // this call. Anything more belongs in synthshell.
    delayDesigner.process (ss, bpm, buffer.getArrayOfWritePointers(),
                           buffer.getNumChannels(), n);

    if (ss.chopOn)
    {
        const double beatsPerSample = bpm / (60.0 * sr);
        double pos = sessPos.load();
        for (int i = 0; i < n; ++i)
        {
            const int step = ((int) std::floor (std::fmod (pos, 4.0) * 4.0)) & 15;
            const float g = (float) ss.chop[(size_t) step] / 127.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.getWritePointer (ch)[i] *= g;
            pos += beatsPerSample;
        }
    }

    midi.clear();
}

// ---------------------------------------------------------------------------
void ExampleSynthProcessor::publishSession()
{
    const int next = 1 - sessIdx.load (std::memory_order_relaxed);
    sessionRt[(size_t) next] = sessionUi;
    sessIdx.store (next, std::memory_order_release);
}

void ExampleSynthProcessor::runSession (juce::MidiBuffer& midi, int numSamples, double bpm)
{
    const bool playing = sessPlaying.load (std::memory_order_relaxed);
    const auto& ss = sessionRt[(size_t) sessIdx.load (std::memory_order_acquire)];

    auto chordOff = [&] (int at)
    {
        for (int& v : chordSounding)
            if (v >= 0) { midi.addEvent (juce::MidiMessage::noteOff (1, v), at); v = -1; }
    };
    auto arpOff = [&] (int at)
    {
        if (arpNoteSounding >= 0)
        { midi.addEvent (juce::MidiMessage::noteOff (1, arpNoteSounding), at); arpNoteSounding = -1; }
    };

    const double beatsPerSample = bpm / (60.0 * sr);
    if (! playing)
    {
        if (wasPlaying)
        {
            chordOff (0); arpOff (0);
            chordSlotCur = -1; arpStepCur = -1;
            sessPos.store (0.0);
            arpStepDisplay.store (-1);
            wasPlaying = false;
        }
        return;
    }
    wasPlaying = true;

    const double totalBeats = 4.0 * (double) juce::jlimit (1, shell::SessionState::kMaxBars, sessBars.load());
    double pos = sessPos.load (std::memory_order_relaxed);
    int at = 0;
    while (at < numSamples)
    {
        const int slot = juce::jlimit (0, shell::SessionState::kChordSlots - 1, (int) (pos / 2.0));
        if (slot != chordSlotCur)
        {
            const auto& cs = ss.chords[(size_t) slot];
            chordSlotCur = slot;
            chordOff (at);
            int tones[shell::SessionState::kMaxChordNotes];
            // Voiced from C4, the keyboard's own register. A chord player an
            // octave low was a real bug: every generated note source must voice
            // from the same register as the keys.
            const int nt = shell::chordTones (cs, 60, tones);
            for (int k = 0; k < nt; ++k)
            {
                if (! ss.arpUse)
                    midi.addEvent (juce::MidiMessage::noteOn (1, tones[k], (juce::uint8) 100), at);
                chordSounding[(size_t) k] = tones[k];
            }
        }

        const double stepLen = ss.arpStepBeats();
        if (ss.arpUse)
        {
            const int len = juce::jlimit (shell::SessionState::kArpMinLen,
                                          shell::SessionState::kArpSteps, ss.arpLen);
            const int step = ((int) std::floor (pos / stepLen + 1e-9)) % len;
            if (step != arpStepCur)
            {
                arpStepCur = step;
                arpStepDisplay.store (step);
                const int row = ss.arpNote[(size_t) step];
                int note = -1;
                if (row > 0)
                {
                    int pool[shell::SessionState::kMaxChordNotes]; int pc = 0;
                    for (int v : chordSounding) if (v >= 0) pool[pc++] = v;
                    if (pc == 0) { pool[pc++] = 60; }        // a held root, so a pattern auditions alone
                    std::sort (pool, pool + pc);
                    note = ss.arpRowMode == 1 ? pool[0] + (row - 1)
                                              : pool[(row - 1) % pc] + 12 * ((row - 1) / pc);
                    note = juce::jlimit (0, 127, note);
                }
                if (note >= 0)
                {
                    const int vel = juce::jlimit (1, 127, (int) ss.arpVel[(size_t) step] * 8 + 7);
                    // GLIDE/TIE into a different note: new note first, then release
                    // the old one, so an engine with portamento glides between them.
                    midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) vel), at);
                    if (arpNoteSounding >= 0 && arpNoteSounding != note)
                        midi.addEvent (juce::MidiMessage::noteOff (1, arpNoteSounding), at);
                    arpNoteSounding = note;
                    const auto gate = ss.arpGate[(size_t) step];
                    const double stepStart = std::floor (pos / stepLen + 1e-9) * stepLen;
                    arpGateEnd = (gate == shell::SessionState::kGateTie
                                  || gate == shell::SessionState::kGateGlide)
                               ? 1e9
                               : stepStart + stepLen * (double) juce::jlimit (1, 8, (int) gate) / 8.0;
                }
                else
                    arpOff (at);
            }
            if (arpNoteSounding >= 0 && pos >= arpGateEnd) arpOff (at);
        }

        const double nextHalfBar = ((double) ((int) (pos / 2.0)) + 1.0) * 2.0;
        const double nextStep    = (std::floor (pos / stepLen + 1e-9) + 1.0) * stepLen;
        const double nextEvent   = std::min ({ nextHalfBar, nextStep, totalBeats,
                                               arpNoteSounding >= 0 ? arpGateEnd : 1e9 });
        const int adv = std::min (numSamples - at,
                                  std::max (1, (int) std::ceil (std::max (1e-9, nextEvent - pos) / beatsPerSample)));
        pos += (double) adv * beatsPerSample;
        at  += adv;

        if (pos >= totalBeats)
        {
            if (sessLoop.load()) { pos -= totalBeats; chordSlotCur = -1; }
            else
            {
                chordOff (std::min (at, numSamples - 1)); arpOff (std::min (at, numSamples - 1));
                chordSlotCur = -1; arpStepCur = -1;
                sessPlaying.store (false); wasPlaying = false; pos = 0.0;
                break;
            }
        }
    }
    sessPos.store (pos);
}

// ---------------------------------------------------------------------------
juce::File ExampleSynthProcessor::supportDir()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
             .getChildFile ("ExampleSynth");
}

void ExampleSynthProcessor::loadUserBank()
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

void ExampleSynthProcessor::saveUserBank()
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

int ExampleSynthProcessor::addUserProgram (const juce::String& name, const juce::String& category)
{
    Program p;
    for (int i = 0; i < kNumParams; ++i) p.value[i] = paramValue (i);
    p.id = "U" + juce::String (bank.size()).paddedLeft ('0', 2).toStdString();
    p.name = name.isEmpty() ? "User" : name.toStdString();
    p.category = category.toStdString();
    p.bank = "USER";
    p.raw = bank.empty() ? std::vector<uint8_t>{} : bank[(size_t) current].raw;   // travels verbatim
    bank.push_back (p);
    ++programVer;
    current = (int) bank.size() - 1;
    return current;
}

bool ExampleSynthProcessor::updateUserProgram (int index)
{
    if (index < 0 || index >= (int) bank.size() || bank[(size_t) index].bank != "USER") return false;
    for (int i = 0; i < kNumParams; ++i) bank[(size_t) index].value[i] = paramValue (i);
    ++programVer;
    return true;
}

bool ExampleSynthProcessor::deleteUserProgram (int index)
{
    if (index < 0 || index >= (int) bank.size() || bank[(size_t) index].bank != "USER") return false;
    bank.erase (bank.begin() + index);
    current = juce::jlimit (0, (int) bank.size() - 1, current);
    ++programVer;
    return true;
}

bool ExampleSynthProcessor::renameProgram (int index, const juce::String& name)
{
    if (index < 0 || index >= (int) bank.size() || bank[(size_t) index].bank != "USER") return false;
    bank[(size_t) index].name = name.toStdString();
    ++programVer;
    return true;
}

bool ExampleSynthProcessor::setProgramCategory (int index, const juce::String& c)
{
    if (index < 0 || index >= (int) bank.size()) return false;
    bank[(size_t) index].category = c.toStdString();
    ++programVer;
    return true;
}

juce::File ExampleSynthProcessor::exportProgramSysex (int index)
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

// ---------------------------------------------------------------------------
void ExampleSynthProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("program", current, nullptr);
    // The session rides the plugin state so a reload keeps the chords and the
    // pattern. It is stored as PROPERTIES, not a struct memcpy: a raw memcpy
    // with a size check silently discards a saved session the day SessionState
    // changes, which is exactly what happened once.
    auto sess = juce::ValueTree ("session");
    sess.setProperty ("arpLen", sessionUi.arpLen, nullptr);
    sess.setProperty ("arpUse", sessionUi.arpUse, nullptr);
    sess.setProperty ("arpRowMode", (int) sessionUi.arpRowMode, nullptr);
    sess.setProperty ("arpStepValue", (int) sessionUi.arpStepValue, nullptr);
    sess.setProperty ("arpFeel", (int) sessionUi.arpFeel, nullptr);
    sess.setProperty ("chopOn", sessionUi.chopOn, nullptr);
    juce::MemoryBlock blob (sessionUi.arpNote.data(), sessionUi.arpNote.size());
    sess.setProperty ("arpNote", blob.toBase64Encoding(), nullptr);
    blob = juce::MemoryBlock (sessionUi.arpVel.data(), sessionUi.arpVel.size());
    sess.setProperty ("arpVel", blob.toBase64Encoding(), nullptr);
    blob = juce::MemoryBlock (sessionUi.arpGate.data(), sessionUi.arpGate.size());
    sess.setProperty ("arpGate", blob.toBase64Encoding(), nullptr);
    blob = juce::MemoryBlock (sessionUi.chop.data(), sessionUi.chop.size());
    sess.setProperty ("chop", blob.toBase64Encoding(), nullptr);
    sess.setProperty ("delayOn", sessionUi.delayOn, nullptr);
    sess.setProperty ("delayMix", (int) sessionUi.delayMix, nullptr);
    blob = juce::MemoryBlock (sessionUi.delayLane.data(),
                              sessionUi.delayLane.size() * sizeof (shell::SessionState::DelayLane));
    sess.setProperty ("delayLanes", blob.toBase64Encoding(), nullptr);
    juce::MemoryOutputStream chords;
    for (const auto& c : sessionUi.chords)
    { chords.writeByte ((char) c.root); chords.writeByte ((char) c.quality); chords.writeByte ((char) c.ext); }
    sess.setProperty ("chords", chords.getMemoryBlock().toBase64Encoding(), nullptr);
    state.appendChild (sess, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ExampleSynthProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    apvts.replaceState (state);
    current = juce::jlimit (0, juce::jmax (0, (int) bank.size() - 1), (int) state.getProperty ("program", 0));
    auto sess = state.getChildWithName ("session");
    if (sess.isValid())
    {
        sessionUi.arpLen = (int) sess.getProperty ("arpLen", 16);
        sessionUi.arpUse = (bool) sess.getProperty ("arpUse", false);
        sessionUi.arpRowMode = (uint8_t) (int) sess.getProperty ("arpRowMode", 0);
        sessionUi.arpStepValue = (uint8_t) (int) sess.getProperty ("arpStepValue", 2);
        sessionUi.arpFeel = (uint8_t) (int) sess.getProperty ("arpFeel", 0);
        sessionUi.chopOn = (bool) sess.getProperty ("chopOn", false);
        const auto get = [&] (const char* key, void* dst, size_t n)
        {
            juce::MemoryBlock mb;
            if (mb.fromBase64Encoding (sess.getProperty (key, "").toString()) && mb.getSize() >= n)
                std::memcpy (dst, mb.getData(), n);
        };
        get ("arpNote", sessionUi.arpNote.data(), sessionUi.arpNote.size());
        get ("arpVel", sessionUi.arpVel.data(), sessionUi.arpVel.size());
        get ("arpGate", sessionUi.arpGate.data(), sessionUi.arpGate.size());
        get ("chop", sessionUi.chop.data(), sessionUi.chop.size());
        sessionUi.delayOn = (bool) sess.getProperty ("delayOn", false);
        sessionUi.delayMix = (uint8_t) (int) sess.getProperty ("delayMix", 64);
        get ("delayLanes", sessionUi.delayLane.data(),
             sessionUi.delayLane.size() * sizeof (shell::SessionState::DelayLane));
        juce::MemoryBlock mb;
        if (mb.fromBase64Encoding (sess.getProperty ("chords", "").toString())
            && mb.getSize() >= sessionUi.chords.size() * 3)
        {
            const auto* b = (const int8_t*) mb.getData();
            for (size_t i = 0; i < sessionUi.chords.size(); ++i)
            {
                sessionUi.chords[i].root = b[i * 3];
                sessionUi.chords[i].quality = (uint8_t) b[i * 3 + 1];
                sessionUi.chords[i].ext = (uint8_t) b[i * 3 + 2];
            }
        }
        publishSession();
    }
    coeffsDirty.store (true);
}

juce::AudioProcessorEditor* ExampleSynthProcessor::createEditor()
{
    return new ExampleSynthEditor (*this);
}

} // namespace examplesynth

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new examplesynth::ExampleSynthProcessor();
}
