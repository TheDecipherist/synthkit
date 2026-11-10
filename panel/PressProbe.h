// THE PRESS PROBE, shared.
//
// It lives in panel/ beside NativePanel.h, which is already on every unit's
// include path, rather than in a probes/ of its own - a second include
// directory is one more thing to wire up per unit and this is a panel probe.
//
// Written three times before this file existed: for the PCM 70, then copied to
// the PCM 80, and wanted a third time by the M-ONE on the same day. The author's
// standing rule is that the SECOND time something is done it becomes a tool
// before the work continues, and this one was already past that.
//
// What it is for: a panel whose controls all have bindings and whose editor
// compiles can still be completely dead to a mouse, and no amount of reading
// the code shows it. This synthesises real mouseDown / mouseDrag / mouseUp on
// the real panel with the real editor attached and the processor's own state
// compared before and after.
//
// READ THE EVENT COLUMN BEFORE BELIEVING THE PARAMETER COLUMN. A control that
// emits an event and moves no PARAMETER is not necessarily broken, and assuming
// it was cost an hour on the PCM 70: its keypad types the row then the column of
// a matrix cell, which is SESSION state, not an APVTS parameter. Same for POWER,
// which has no plugin counterpart. "none" in the EVENT column is a dead hit box
// and nothing else is.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <NativePanel.h>
#include <cstdio>
#include <map>
#include <vector>
#include <string>
#include <type_traits>

namespace synthkit::probe {
namespace detail {

juce::MouseEvent ev (juce::Component& c, juce::Point<float> p)
{
    return { juce::Desktop::getInstance().getMainMouseSource(), p,
             juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
             &c, &c, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false };
}

template <typename Proc>
std::map<std::string, float> snapshot (Proc& p)
{
    std::map<std::string, float> m;
    for (auto* q : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*> (q))
            m[r->paramID.toStdString()] = r->getValue();
    return m;
}

} // namespace detail

using detail::ev;
using detail::snapshot;

/** @param extra  a unit's OWN checks, run after the generic sweep with the
                  live processor and panel. A machine's specifics belong here
                  rather than in this file: the PCM 70's keypad types a row then
                  a column, the M-ONE's ten keys open menu PAGES, and a shared
                  probe that grows a branch per machine stops being shared. */
template <typename Proc, typename Ed, typename Extra = std::nullptr_t>
int runPressProbe (Extra extra = nullptr, int width = 1692)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    Proc proc;
    proc.prepareToPlay (48000.0, 256);
    Ed ed (proc);
    ed.setBounds (0, 0, width, 200);

    auto& panel = const_cast<synthkit::NativePanel&> (ed.devPanelView());

    // LAY IT OUT THE WAY THE EDITOR DOES, not the way a probe would like. The
    // editor decides the panel's band, and if that band is shorter than the
    // panel's natural height the panel SCROLLS - and NativePanel draws the
    // scroll bars ON TOP, where mouseDown gives them the click before any
    // control underneath. Setting the bounds by hand here hides exactly that.
    // getComponentAt returns nullptr for an INVISIBLE component before it looks
    // at any child, so an editor that was never put in a window reports every
    // point as empty and the routing check means nothing. Make it visible.
    ed.setVisible (true);
    ed.setSize (width, ed.heightForWidth (width));
    ed.resized();
    std::printf ("editor %d x %d; panel %d x %d; the panel's NATURAL height at this width is %d\n",
                 ed.getWidth(), ed.getHeight(), panel.getWidth(), panel.getHeight(),
                 panel.heightForWidth (panel.getWidth()));
    if (panel.getHeight() < panel.heightForWidth (panel.getWidth()))
        std::printf ("  THE PANEL IS IN A BAND and will draw scroll bars over itself\n");
    {
        int under = 0, total = 0;
        for (const auto& pl : panel.placements())
        {
            if (pl.id.isEmpty() || pl.kind == "screen" || pl.kind == "static") continue;
            const auto bb = panel.controlBounds (pl.id).toFloat();
            if (bb.isEmpty()) continue;
            ++total;
            if (panel.isOnScrollBar (bb.getCentre())) ++under;
        }
        std::printf ("  %d of %d controls have their CENTRE under a scroll bar\n", under, total);
    }
    // WHAT DOES A CLICK AT EACH CONTROL ACTUALLY REACH? hitCtl scans the control
    // list BACKWARDS, so a big decorative control added late captures the click
    // for everything drawn under it, and the one underneath never hears a thing.
    {
        int stolen = 0;
        for (const auto& pl : panel.placements())
        {
            if (pl.id.isEmpty() || pl.kind == "screen" || pl.kind == "static") continue;
            const auto bb = panel.controlBounds (pl.id).toFloat();
            if (bb.isEmpty()) continue;
            const auto design = panel.toDesign (bb.getCentre());
            const auto got = panel.idAt (design);
            if (got != pl.id)
            { ++stolen;
              std::printf ("  a click on \"%s\" reaches \"%s\"\n",
                           pl.id.toRawUTF8(), got.isEmpty() ? "(nothing)" : got.toRawUTF8()); }
        }
        std::printf ("  %d controls have their click taken by something else\n", stolen);
    }
    // WHICH COMPONENT DOES THE OS ACTUALLY HAND THE CLICK TO? Everything above
    // talks to the panel directly. A real mouse goes to the editor first, and
    // JUCE routes it with getComponentAt - so if the session strip, or anything
    // the strip puts up, covers the panel, the panel never hears a thing however
    // correct its own hit rects are. The author's suggestion, and it is the one layer
    // nothing else here tests.
    {
        int wrong = 0, total = 0;
        juce::String firstWrong;
        for (const auto& pl : panel.placements())
        {
            if (pl.id.isEmpty() || pl.kind == "screen" || pl.kind == "static") continue;
            const auto bb = panel.controlBounds (pl.id).toFloat();
            if (bb.isEmpty()) continue;
            ++total;
            const auto inEditor = panel.getLocalPoint (&ed, bb.getCentre());
            juce::ignoreUnused (inEditor);
            const auto ptInEd = ed.getLocalPoint (&panel, bb.getCentre()).toInt();
            auto* got = ed.getComponentAt (ptInEd);
            const bool ok = got == &panel || (got != nullptr && panel.isParentOf (got));
            if (! ok)
            {
                ++wrong;
                if (firstWrong.isEmpty())
                    firstWrong = "\"" + pl.id + "\" at editor point " + ptInEd.toString()
                               + " is covered by " + (got == nullptr ? juce::String ("(nothing)")
                                 : (got->getName().isNotEmpty() ? got->getName()
                                    : juce::String (typeid (*got).name())));
            }
        }
        std::printf ("  %d of %d control centres are NOT routed to the panel by getComponentAt\n",
                     wrong, total);
        if (firstWrong.isNotEmpty()) std::printf ("    %s\n", firstWrong.toRawUTF8());
        std::printf ("\n");
    }

    // THE CURSOR IS THE CHEAPEST DIAGNOSTIC THERE IS. NativePanel already sets a
    // drag cursor over a knob and a pointing hand over a cap, in mouseMove. If a
    // user hovers a knob and the cursor does NOT change, the panel is receiving
    // no mouse events at all - which is a different fault from a control that is
    // wired wrong, and tells the two apart in two seconds without a debugger.
    // DOES THE PANEL FILL ITS COMPONENT? The author: "the actual panel ui doesnt take
    // 100% width. there is a like 200px empty dark space on the right." If the
    // panel paints at one scale and toDesign() converts at another, the drawing
    // and the hit areas are in different places and every click misses.
    {
        const int fills = panel.maxUsefulWidth();
        std::printf ("panel %d x %d for a design of %d x %d; the widest it can FILL is %d\n",
                     panel.getWidth(), panel.getHeight(), panel.designWidth(),
                     panel.designHeight(), fills);
        if (panel.getWidth() > fills)
            std::printf ("  THE WINDOW IS %d px WIDER THAN THE PANEL CAN FILL - that space is bare\n",
                         panel.getWidth() - fills);
    }
    std::printf ("\n");

    // NOTE: there is deliberately no cursor check here. NativePanel::getMouseCursor
    // reads getMouseXYRelative() - the REAL pointer - so in a headless probe it
    // always reports whatever is under a mouse that is not over the window, and
    // an earlier version of this file printed "NORMAL" for all 24 controls and
    // read exactly like a bug. The cursor is checked by hovering the running
    // plugin, and the law it has to meet is in NativePanel::getMouseCursor.

    std::printf ("EVERY CONTROL, PRESSED FOR REAL\n");
    std::printf ("a synthesised gesture on the panel, the editor attached, parameters diffed\n\n");
    // Chain the editor's own handler so the two failures can be told apart: a
    // control the PANEL never emitted for (a dead hit box) against one it
    // emitted for and the HANDLER ignored. Without this they look identical,
    // and they need opposite fixes.
    auto editorHandler = panel.onChange;
    juce::String emittedFor;
    panel.onChange = [&] (const synthkit::NativePanel::Event& e)
    { emittedFor = e.id; if (editorHandler) editorHandler (e); };

    std::printf ("%-14s %-7s %-7s %-26s %s\n", "control", "kind", "event", "parameter moved", "verdict");

    int dead = 0, live = 0, emittedOnly = 0;
    for (const auto& pl : panel.placements())
    {
        if (pl.id.isEmpty() || pl.kind == "screen" || pl.kind == "static") continue;
        const auto b = panel.controlBounds (pl.id).toFloat();
        if (b.isEmpty()) continue;
        const auto pt = b.getCentre();
        if (! panel.getLocalBounds().toFloat().contains (pt)) continue;
        if (panel.isOnScrollBar (pt)) continue;

        const bool turns = pl.kind == "knob" || pl.kind == "fader" || pl.kind == "readout";
        // DO NOT park the knob. The viewport probe parks every knob mid-travel
        // first, and says why: a control at its stop turned toward that stop
        // emits nothing. That is correct for testing hit boxes and it HIDES the
        // thing the author reported - a knob that opens at a stop and will not move.
        // Test it where it actually opens.
        const auto before = snapshot (proc);
        emittedFor = {};

        // DRAG BOTH WAYS. Dragging only upward reports a knob ALREADY AT ITS
        // MAXIMUM as dead, because there is nowhere above it to go and the
        // panel emits nothing. The M-ONE's MIX sits at 100 per cent out of the
        // box - 40 of its 100 presets are Dual S/R, a send configuration, so
        // the bank is written full wet - and this probe called it DEAD on a
        // knob that works perfectly.
        panel.mouseDown (ev (panel, pt));
        if (turns) panel.mouseDrag (ev (panel, pt - juce::Point<float> (0.0f, 50.0f)));
        panel.mouseUp (ev (panel, turns ? pt - juce::Point<float> (0.0f, 50.0f) : pt));
        if (turns && ! emittedFor.isNotEmpty())
        {
            panel.mouseDown (ev (panel, pt));
            panel.mouseDrag (ev (panel, pt + juce::Point<float> (0.0f, 50.0f)));
            panel.mouseUp (ev (panel, pt + juce::Point<float> (0.0f, 50.0f)));
        }

        const auto after = snapshot (proc);
        std::string moved;
        for (const auto& [k, v] : after)
            if (std::abs (v - before.at (k)) > 1.0e-5f)
            { if (! moved.empty()) moved += ","; moved += k; }

        const bool emitted = emittedFor.isNotEmpty();
        const char* verdict;
        if (! moved.empty())        { ++live;        verdict = "ok"; }
        else if (emitted)           { ++emittedOnly; verdict = "no parameter - UI state or needs more presses"; }
        else                        { ++dead;        verdict = "DEAD - the panel emitted nothing"; }
        std::printf ("%-14s %-7s %-7s %-26s %s\n", pl.id.toRawUTF8(), pl.kind.toRawUTF8(),
                     emitted ? "fired" : "none", moved.empty() ? "-" : moved.c_str(), verdict);
    }
    std::printf ("\n%d moved a parameter, %d emitted without one, %d DEAD (no event at all)\n",
                 live, emittedOnly, dead);

    if constexpr (! std::is_same_v<Extra, std::nullptr_t>) extra (proc, panel);

    // A MACHINE'S OWN EXTRAS GO IN ITS OWN PROBE, not here. The PCM 70's
    // keypad types a row then a column and its arrows step a 10-bit cell; the
    // M-ONE's ten keys open MENU PAGES. Those checks live in each unit's
    // press_probe.cpp, after this call, because a shared probe that grows a
    // branch per machine stops being shared.
    return 0;
}

} // namespace synthkit::probe
