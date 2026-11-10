// The text law. One family per synth (Tokens::face), a floor under every size
// (Tokens::minText), and a word that has no room sideways WRAPS to two lines
// before it is ever made smaller. Every word drawn is recorded in the ledger
// with the size it was drawn at, so a gate can name the smallest text on the panel.
#pragma once
#include "Pen.h"

namespace synthkit::draw {

/** The font at a NOMINAL size in design units (the CSS px a spec gives). */
[[nodiscard]] juce::Font font (const Pen&, float size, bool bold = false, float tracking = 0.0f);
[[nodiscard]] float widthOf (const Pen&, const juce::String&, float size, bool bold = false, float tracking = 0.0f);

/** Draws `s` in `r`. Too wide at `size`: split at the space nearest the middle
    into two lines; still too wide: shrink, never below Tokens::minText. Returns
    the size used. Recorded in the ledger as kind "text" (with `id` when given). */
float text (const Pen&, const juce::String& s, juce::Rectangle<float> r, float size, juce::Colour c,
            juce::Justification j = juce::Justification::centred, bool bold = false,
            float tracking = 0.0f, const juce::String& id = {});

/** A REVERSED legend: the word printed dark in a filled ink box. On a hardware
    panel it marks the shifted function of the control beside it. */
void boxed (const Pen&, const juce::String& s, juce::Point<float> centre, float size);

} // namespace synthkit::draw
