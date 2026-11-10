#pragma once

// WHAT THE CONTROL UNDER THE CURSOR IS, IN WORDS.
//
// The author, 2026-09-27: "when hovering any icon button we need to have a tooltip that says what
// it is."
//
// An icon button is the one control on a plate with nothing printed on it to read - that is
// the point of it being an icon - so the panel cannot answer the question and the glyph
// cannot either. The answer travels as data: the row plan writes a `tip` per control,
// `synthpanel-creator` carries it into `layout.json`, the painter keeps it on the control,
// and this draws it.
//
// WHY IT IS NOT juce::TooltipWindow. That draws in the look-and-feel's own colours on a
// separate desktop window, which on a plate like this reads as an operating-system dialog
// that has wandered onto the instrument. Every other pixel here is the panel's; so is this
// one. It also means no z-order fight with a panel that is one component painting a hundred
// and twenty controls, and no second Component to keep in step with the plate's scale.
//
// IT OWNS ITS OWN CLOCK. A panel usually already has a Timer doing something else - the
// edit display, the meters - and a tooltip's delay is its own business. Give it the owner
// to repaint and it asks for exactly the two repaints it needs: one when the delay expires
// and one when the box goes away.
//
// THE SEAM IS SCREEN PIXELS. `hover` takes the text and where the mouse is in the
// component's own coordinates, `paint` takes the component's bounds. Nothing here knows
// about a design space or a scale factor, so a painter that works in spec units converts
// once, at the call, and two different painters can share this without agreeing on
// anything else.

#include <juce_gui_basics/juce_gui_basics.h>

namespace skit {

/** THE INKS, so a plate hands over its own rather than getting a system dialog.
    Outside the class on purpose: a default argument of `{}` inside it would need these
    initializers while the class is still being defined, which is ill-formed. */
struct TooltipLook
{
    juce::Colour back { 0xf2141722 };   // the panel's own well, nearly opaque
    juce::Colour ink  { 0xfff2f2f6 };   // the legend white
    juce::Colour edge { 0x4dffffff };   // a hairline, so it reads as a part
    float fontHeight = 13.0f;
    float radius     = 4.0f;
    float padX       = 8.0f;
    float padY       = 5.0f;
    float gap        = 14.0f;           // how far below the pointer the box sits
    float maxWidth   = 260.0f;
};

class Tooltip : private juce::Timer
{
public:
    using Look = TooltipLook;
    explicit Tooltip (juce::Component& ownerIn) : owner (ownerIn) {}

    /** HOW LONG THE POINTER HAS TO REST before the box appears, in milliseconds.
        Short enough that asking the question is answered rather than waited on, long
        enough that crossing the plate on the way somewhere else does not strobe boxes at
        you. */
    static constexpr int kDelayMs = 450;

    /** The tip under the cursor, and where the cursor is in the owner's coordinates. An
        empty string means "nothing under it", which hides the box. Call it from
        `mouseMove` with whatever the hit test found; calling it repeatedly with the same
        text is free and does not restart the delay. */
    void hover (const juce::String& tip, juce::Point<float> mouse)
    {
        at = mouse;
        if (tip == current)
        {
            // The box does NOT follow the pointer once it is up. A box that chases the
            // mouse inside one control is movement with no information in it, and the eye
            // goes to the movement rather than to the words.
            return;
        }
        current = tip;
        shownNow = false;
        if (current.isEmpty()) { stopTimer(); owner.repaint(); return; }
        startTimer (kDelayMs);
    }

    /** Hide it: a click, the mouse leaving, a page change. */
    void clear()
    {
        stopTimer();
        if (current.isEmpty() && ! shownNow) return;
        current = {}; shownNow = false;
        owner.repaint();
    }

    [[nodiscard]] bool visible() const noexcept { return shownNow && current.isNotEmpty(); }
    [[nodiscard]] const juce::String& text() const noexcept { return current; }

    /** WHAT THE BOX WILL SAY once the delay is up, whether or not it is showing yet.
        A gate walks a hundred and twenty controls and cannot spend 450 ms on each, so it
        asks this after the move and spends the delay ONCE, on one control, to prove the
        reveal itself works. Reading a pending tip is not a way to skip that. */
    [[nodiscard]] const juce::String& pending() const noexcept { return current; }

    /** REVEAL WITHOUT WAITING, for a gate. Does exactly what the timer's callback does.

        WHY A GATE NEEDS THIS. Waiting out the real delay means running the message loop,
        and `MessageManager::runDispatchLoopUntil` is behind JUCE_MODAL_LOOPS_PERMITTED -
        which JUCE defaults to 0. A gate written on it compiles in some configurations of
        a repo and not others, which is worse than one that does not compile at all,
        because it then exists on one machine and silently vanishes on the next.

        What this leaves untested is that juce::Timer fires, which is JUCE's business
        rather than ours. Everything that IS ours stays covered: the tip arriving, the
        delay being armed, what the reveal does, and the press that dismisses it. */
    void revealForTest() { timerCallback(); }

    /** Draws the box inside `bounds`, which is the owner's own rectangle. Does nothing
        until the delay has elapsed, so a caller can put this at the end of `paint`
        unconditionally.

        IT NEVER LEAVES THE PLATE. Below the pointer by default, above it when there is no
        room below, and slid sideways when it would run off an edge - a tooltip clipped at
        the window edge is a tooltip whose last three words are the ones you needed. */
    void paint (juce::Graphics& g, juce::Rectangle<float> bounds, const Look& look = {}) const
    {
        if (! visible()) return;

        const juce::Font font (juce::FontOptions (look.fontHeight));
        const float textW = juce::jmin (look.maxWidth,
                                        (float) juce::GlyphArrangement::getStringWidthInt (font, current) + 1.0f);
        // ONE LINE UNLESS IT CANNOT BE. A tip is written to be readable in the moment
        // before the hand moves on, so the wrap is a safety net and not a layout.
        const int lines = (int) std::ceil ((float) juce::GlyphArrangement::getStringWidthInt (font, current)
                                           / juce::jmax (1.0f, look.maxWidth));
        const float w = textW + 2.0f * look.padX;
        const float h = (float) juce::jmax (1, lines) * look.fontHeight * 1.25f + 2.0f * look.padY;

        float x = at.x - w * 0.5f;
        float y = at.y + look.gap;
        if (y + h > bounds.getBottom() - 2.0f) y = at.y - look.gap - h;
        y = juce::jlimit (bounds.getY() + 2.0f, juce::jmax (bounds.getY() + 2.0f, bounds.getBottom() - h - 2.0f), y);
        x = juce::jlimit (bounds.getX() + 2.0f, juce::jmax (bounds.getX() + 2.0f, bounds.getRight() - w - 2.0f), x);

        const juce::Rectangle<float> box (x, y, w, h);
        g.setColour (juce::Colour (0x66000000));
        g.fillRoundedRectangle (box.translated (0.0f, 1.5f), look.radius);
        g.setColour (look.back);
        g.fillRoundedRectangle (box, look.radius);
        g.setColour (look.edge);
        g.drawRoundedRectangle (box.reduced (0.5f), look.radius, 1.0f);
        g.setColour (look.ink);
        g.setFont (font);
        g.drawFittedText (current, box.reduced (look.padX, look.padY).toNearestInt(),
                          juce::Justification::centred, juce::jmax (1, lines));
    }

private:
    void timerCallback() override
    {
        stopTimer();
        if (current.isEmpty()) return;
        shownNow = true;
        owner.repaint();
    }

    juce::Component& owner;
    juce::String current;
    juce::Point<float> at;
    bool shownNow = false;
};

} // namespace skit
