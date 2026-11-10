// Do the front panel's controls reach anything?
//
// A whole class of bug that no test catches and every listener notices. Found
// across the Reverb 4000, TC 2290, PCM 91, H3500 and M1 in one week; each looked
// like a different problem and several shared a cause. The Reverb 4000's
// bindings.json was compiled into the binary and read by NO code at all - the
// real binding was a hard-coded if chain, so the file said one thing, the code
// did another, and nothing could notice. 68 of 189 rows were silent: 36 % of the
// machine.
//
// So this drives the REAL editor - the same entry point a mouse reaches - moves
// every control end to end, renders audio at both ends and compares. A control
// that changes nothing audible is the entire complaint, and a code-reading audit
// cannot answer it either way.
//
// Four checks, and they fail for different reasons:
//   1. every id in panel/bindings.json is recognised by the editor;
//   2. every control the panel offers MOVES THE AUDIO;
//   3. every plugin parameter is reachable from some control, or is on the
//      declared list of things that are not (the inverse is just as common and
//      easier to miss: the Reverb 4000 had 11, including Attack, Hold and
//      Release, which were already tempo-sync destinations - so the clock could
//      drive a gate the user had no way to set);
//   4. every panel control id exists in the panel SPEC, so a binding cannot
//      point at a control nobody can see;
//   6. every control is CLICKABLE WHERE IT IS DRAWN. Checks 1 to 3 drive controls by
//      identifier, which proves the editor moves the audio when handed an id and says
//      nothing about whether a click on the painted control produces that id. A stale
//      native export, or a control moved in panel.json without re-exporting, makes every
//      click land nowhere and leaves every other check green.
//
//   ExampleFxControlsProbe             measure
//   ExampleFxControlsProbe --selftest  kill a control on purpose and require
//                                         this probe to notice
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

using namespace examplefx;

namespace {

constexpr double kSr = 48000.0;
constexpr int    kBlock = 512;
// Four seconds, not one. A slow modulator that steps once per LFO cycle never
// reaches its first step in a short render, and then measures as dead wiring
// when it is only slower than the measurement. The Reverb 4000's Mod Width was
// exactly that.
constexpr int    kRenderSamples = (int) (kSr * 4.0);
constexpr double kLiveThreshold = 2.0e-3;

/** Parameters with deliberately NO control on this panel. A parameter that is
    not reachable from the front panel and not on this list is a finding.
    TODO(new machine): keep this list short and honest, and put the reason next
    to each entry. "We have not got to it yet" is a legitimate reason; "it does
    not matter" is not. */
struct Unreachable { const char* id; const char* why; };
constexpr Unreachable kUnreachable[] = {
    { "bpm", "the master clock: the shell's transport header owns it" },
};
constexpr int kNumUnreachable = (int) (sizeof (kUnreachable) / sizeof (kUnreachable[0]));

int failures = 0;

/** A neutral starting point, chosen so nothing masks anything else.

    This matters more than it looks. Run at the machine's own defaults and half
    the panel measures dead: the Reverb 4000's four band multipliers all at 1.0
    made every crossover genuinely inaudible, and its Lo Cut was correctly silent
    at Lo Damp 0 because Lo Cut is only the CORNER. A probe that reports a
    working control as dead teaches people to ignore it. */
void neutral (ExampleFxProcessor& p)
{
    p.setCurrentProgram (3);                      // a program with something happening
    p.setParamValue (pDrive, 0.25f);              // NOT 0, or drive itself measures dead
    p.setParamValue (pDelayTime, 280.0f);
    p.setParamValue (pFeedback, 0.40f);           // NOT 0, or damping has nothing to damp
    p.setParamValue (pDamping, 6000.0f);
    p.setParamValue (pDelayMix, 0.50f);           // NOT 0, or delay_time is inaudible
    p.setParamValue (pModRate, 1.20f);
    p.setParamValue (pModDepth, 5.0f);            // NOT 0, or mod_rate is inaudible
    p.setParamValue (pModMix, 0.50f);
    p.setParamValue (pTremDepth, 0.35f);          // NOT 0, or mod_rate loses half its evidence
    p.setParamValue (pWidth, 0.50f);
    p.setParamValue (pOutput, 0.80f);
    p.setBypassed (false);
}

/** 100 ms of DECORRELATED noise, then silence. Decorrelated because a width
    control that narrows the wet pair towards mono cannot be measured with the
    same signal in both channels, and neither can true stereo. */
std::vector<float> render (ExampleFxProcessor& proc)
{
    proc.prepareToPlay (kSr, kBlock);             // clears the tail: no leak from the last render
    juce::AudioBuffer<float> buf (2, kBlock);
    juce::Random rl { 0x5eed1 }, rr { 0x5eed2 };
    std::vector<float> out;
    out.reserve ((size_t) kRenderSamples * 2);
    const int burst = (int) (kSr * 0.1);
    for (int done = 0; done < kRenderSamples; done += kBlock)
    {
        juce::MidiBuffer midi;
        for (int i = 0; i < kBlock; ++i)
        {
            const bool on = (done + i) < burst;
            buf.setSample (0, i, on ? rl.nextFloat() * 2.0f - 1.0f : 0.0f);
            buf.setSample (1, i, on ? rr.nextFloat() * 2.0f - 1.0f : 0.0f);
        }
        proc.processBlock (buf, midi);
        for (int i = 0; i < kBlock; ++i)
        { out.push_back (buf.getSample (0, i)); out.push_back (buf.getSample (1, i)); }
    }
    return out;
}

double difference (const std::vector<float>& a, const std::vector<float>& b)
{
    const size_t n = std::min (a.size(), b.size());
    double d = 0, ea = 0, eb = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double x = a[i], y = b[i];
        d += (x - y) * (x - y); ea += x * x; eb += y * y;
    }
    const double ref = std::sqrt (std::max (ea, eb));
    return ref > 1.0e-12 ? std::sqrt (d) / ref : 0.0;
}

std::map<std::string, std::string> bindingKinds()
{
    std::map<std::string, std::string> out;
    const auto json = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::bindings_json,
                                                                BinaryData::bindings_jsonSize));
    if (auto* obj = json.getProperty ("bindings", {}).getDynamicObject())
        for (const auto& kv : obj->getProperties())
            out[kv.name.toString().toStdString()] =
                kv.value.getProperty ("kind", "").toString().toStdString();
    return out;
}

std::map<std::string, std::string> bindingParams()
{
    std::map<std::string, std::string> out;
    const auto json = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::bindings_json,
                                                                BinaryData::bindings_jsonSize));
    if (auto* obj = json.getProperty ("bindings", {}).getDynamicObject())
        for (const auto& kv : obj->getProperties())
        {
            const auto p = kv.value.getProperty ("param", "").toString();
            if (p.isNotEmpty()) out[kv.name.toString().toStdString()] = p.toStdString();
        }
    return out;
}

/** Every parameter any binding can reach, INCLUDING the second one a single
    control reaches through its selector. Counting only `param` reports the
    second as an orphan with no control, which is a real finding on some machines
    and a false one wherever a panel has a SHIFT key. */
std::set<std::string> reachableParams()
{
    std::set<std::string> out;
    const auto json = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::bindings_json,
                                                                BinaryData::bindings_jsonSize));
    if (auto* obj = json.getProperty ("bindings", {}).getDynamicObject())
        for (const auto& kv : obj->getProperties())
            for (const char* key : { "param", "altParam" })
            {
                const auto p = kv.value.getProperty (key, "").toString();
                if (p.isNotEmpty()) out.insert (p.toStdString());
            }
    return out;
}

/** Every `select` binding's stated reason. A selector writes no parameter and so
    cannot be measured in the audio, which makes it the one shape of control this
    probe would otherwise wave through. It has to say WHY, in the file, or the
    exemption is a loophole. */
std::map<std::string, std::string> selectReasons()
{
    std::map<std::string, std::string> out;
    const auto json = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::bindings_json,
                                                                BinaryData::bindings_jsonSize));
    if (auto* obj = json.getProperty ("bindings", {}).getDynamicObject())
        for (const auto& kv : obj->getProperties())
            if (kv.value.getProperty ("kind", "").toString() == "select")
                out[kv.name.toString().toStdString()] =
                    kv.value.getProperty ("why", "").toString().toStdString();
    return out;
}

/** Every control id the SPEC draws, wherever the spec puts it.
    This used to read only `sections[].controls`, so a panel that places anything at the top
    level - a header strip, a row of transport buttons, anything not inside a bordered
    section - reported EVERY one of those controls as unreachable. The SuperClassic's whole
    header (program display, the up/down buttons, VOICES, GLIDE, PB RANGE, the meter) is
    top-level, because the machine draws no box around it, and this check called eleven live
    controls dead. Read the same three places renderPanel does: `spec.controls`,
    `sections[].controls` and `sections[].wells[].controls`. */
std::set<std::string> panelSpecIds()
{
    std::set<std::string> ids;
    const auto spec = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::panel_json,
                                                                BinaryData::panel_jsonSize));
    const auto take = [&ids] (const juce::var& list)
    {
        if (auto* cs = list.getArray())
            for (const auto& c : *cs)
            {
                const auto id = c.getProperty ("id", "").toString();
                if (id.isNotEmpty()) ids.insert (id.toStdString());
            }
    };
    take (spec.getProperty ("controls", {}));
    if (auto* secs = spec.getProperty ("sections", {}).getArray())
        for (const auto& s : *secs)
        {
            take (s.getProperty ("controls", {}));
            if (auto* wells = s.getProperty ("wells", {}).getArray())
                for (const auto& w : *wells) take (w.getProperty ("controls", {}));
        }
    return ids;
}

int run (bool breakOneControl, bool verbose)
{
    failures = 0;
    ExampleFxProcessor proc;
    proc.prepareToPlay (kSr, kBlock);
    ExampleFxEditor ed (proc);

    const auto kinds = bindingKinds();
    const auto params = bindingParams();
    const auto specIds = panelSpecIds();

    // THE INJECTION for --selftest: one control whose write path is cut. This is
    // the shape of the TC 2290's five EXT EFFECTS buttons, which were LIT from
    // the engine state by the panel refresh and had no click handler at all -
    // lamps correct, buttons dead - and because the panel looked alive, nobody
    // suspected the binding.
    const std::string dead = breakOneControl ? "feedback" : "";
    // The SECOND injection: a control whose hit area has slid off what it draws, which is
    // what a stale layout export looks like from the outside. Only check 6 can see it.
    if (breakOneControl) ed.devMoveHit (juce::String (dead), 220.0f, 160.0f);

    // ---------------------------------------------------------------- 1
    std::printf ("1. every id in panel/bindings.json is recognised by the editor\n");
    {
        int checked = 0, orphan = 0;
        for (const auto& [id, kind] : kinds)
        {
            if (kind == "screen") continue;
            ++checked;
            if (! ed.devPanel (juce::String (id), kind == "value", 0.5f))
            {
                ++orphan; ++failures;
                std::printf ("   %-16s declared %-7s and NO HANDLER - the control is dead\n",
                             id.c_str(), kind.c_str());
            }
        }
        std::printf ("   %d interactive controls declared, %d with no handler\n\n", checked, orphan);
    }

    // ---------------------------------------------------------------- 2
    std::printf ("2. every control the panel offers, measured in the AUDIO\n");
    {
        int live = 0, dead2 = 0;
        for (const auto& [id, kind] : kinds)
        {
            if (kind != "value") continue;
            neutral (proc);
            const auto pid = params.count (id) ? params.at (id) : std::string();
            const bool cut = (id == dead);
            if (! cut) ed.devPanel (juce::String (id), true, 0.0f);
            const auto A = render (proc);
            neutral (proc);
            if (! cut) ed.devPanel (juce::String (id), true, 1.0f);
            const auto B = render (proc);
            const double d = difference (A, B);
            const bool ok = d > kLiveThreshold;
            if (ok) ++live; else { ++dead2; ++failures; }
            if (verbose || ! ok)
                std::printf ("   %-16s -> %-14s difference %9.5f  %s\n",
                             id.c_str(), pid.c_str(), d, ok ? "" : "<-- REACHES NO AUDIO");
        }
        std::printf ("   %d controls offered, %d silent\n\n", live + dead2, dead2);
    }

    // ---------------------------------------------------------------- 3
    // THE INVERSE, and it is just as common and easier to miss.
    std::printf ("3. every plugin parameter reachable from some control\n");
    {
        const auto reachable = reachableParams();
        int orphans = 0;
        for (auto* prm : proc.getParameters())
        {
            const auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (prm);
            if (wp == nullptr) continue;
            const std::string id = wp->paramID.toStdString();
            if (reachable.count (id)) continue;
            bool declared = false;
            for (int i = 0; i < kNumUnreachable; ++i)
                if (id == kUnreachable[i].id) declared = true;
            if (! declared)
            {
                ++orphans; ++failures;
                std::printf ("   %-16s has NO CONTROL and is not on the declared list\n", id.c_str());
            }
        }
        std::printf ("   %d parameters on the panel, %d declared unreachable, %d orphaned\n\n",
                     (int) reachable.size(), kNumUnreachable, orphans);
    }

    // ---------------------------------------------------------------- 4
    std::printf ("4. every binding points at a control the panel spec actually draws\n");
    {
        int missing = 0;
        for (const auto& [id, kind] : kinds)
            if (specIds.count (id) == 0)
            {
                ++missing; ++failures;
                std::printf ("   %-16s is bound but is not in panel/panel.json - nobody can "
                             "reach it\n", id.c_str());
            }
        // And the other direction: a control drawn on the panel with no binding
        // is a knob that does nothing, which is what a listener actually reports.
        int unbound = 0;
        for (const auto& id : specIds)
            if (kinds.count (id) == 0)
            {
                ++unbound; ++failures;
                std::printf ("   %-16s is drawn on the panel and bound to NOTHING\n", id.c_str());
            }
        std::printf ("   %d spec controls, %d bindings; %d dangling, %d unbound\n\n",
                     (int) specIds.size(), (int) kinds.size(), missing, unbound);
    }

    // ---------------------------------------------------------------- 5
    std::printf ("5. every `select` binding says WHY it moves no audio\n");
    {
        int silentExemptions = 0;
        for (const auto& [id, why] : selectReasons())
        {
            if (why.size() < 40)
            {
                ++silentExemptions; ++failures;
                std::printf ("   %-16s is exempt from check 2 and gives no reason\n", id.c_str());
            }
            else
                std::printf ("   %-16s %s\n", id.c_str(), why.c_str());
        }
        std::printf ("   %d selectors, %d without a reason\n\n",
                     (int) selectReasons().size(), silentExemptions);
    }


    // ---------------------------------------------------------------- 6
    std::printf ("6. every control is clickable where it is drawn\n");
    {
        // How far a hit area may reasonably exceed the control it belongs to. A knob wants a
        // few units of slack round its edge; a hit box far larger than the thing drawn is how
        // one control swallows the ones beside it and they all report as dead.
        constexpr float kSlack = 26.0f;
        int checked = 0, misplaced = 0, offset = 0;
        for (const auto& p : ed.devPanelView().placements())
        {
            if (p.hit.isEmpty() || p.id.isEmpty()) continue;      // a screen is not clickable
            ++checked;
            const auto centre = p.drawn.getCentre();
            const auto got = ed.devPanelView().idAt (centre);
            if (got != p.id)
            {
                ++misplaced; ++failures;
                std::printf ("   %-16s is drawn at %.0f,%.0f and a click there reaches %s\n",
                             p.id.toRawUTF8(), centre.x, centre.y,
                             got.isEmpty() ? "NOTHING" : got.toRawUTF8());
            }
            if (! p.drawn.expanded (kSlack).contains (p.hit))
            {
                ++offset; ++failures;
                std::printf ("   %-16s draws %.0fx%.0f at %.0f,%.0f and claims %.0fx%.0f at %.0f,%.0f - "
                             "a hit area that is not over its own control both misses it and "
                             "swallows the ones beside it\n", p.id.toRawUTF8(),
                             p.drawn.getWidth(), p.drawn.getHeight(), p.drawn.getX(), p.drawn.getY(),
                             p.hit.getWidth(), p.hit.getHeight(), p.hit.getX(), p.hit.getY());
            }
        }
        std::printf ("   %d clickable controls, %d not where they are drawn, %d whose hit area is not "
                     "over what they draw\n\n", checked, misplaced, offset);
    }

    return failures;
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    bool self = false, verbose = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp (argv[i], "--selftest") == 0) self = true;
        if (std::strcmp (argv[i], "--verbose") == 0) verbose = true;
    }

    if (! self)
    {
        std::printf ("ExampleFx - are the panel's controls wired to anything?\n"
                     "Every control is measured in the AUDIO, through the real editor path.\n\n");
        const int bad = run (false, verbose);
        std::printf ("%s\n", bad == 0 ? "PASS - every control the panel offers moves the audio"
                                      : "FAIL - see the rows above");
        return bad == 0 ? 0 : 1;
    }

    std::printf ("SELFTEST - cut one control's write path and require the probe to notice.\n"
                 "(The TC 2290's EXT EFFECTS buttons were LIT by the panel refresh and had no\n"
                 " click handler at all: lamps correct, buttons dead, panel looking alive.)\n\n");
    const int bad = run (true, false);
    std::printf ("\nthe broken build produced %d findings\n", bad);
    std::printf ("%s\n", bad > 0 ? "PASS - the probe catches a dead control"
                                 : "FAIL - the probe passed a panel with a dead control");
    return bad > 0 ? 0 : 1;
}
