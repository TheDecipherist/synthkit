// The printed furniture: a section's title bar and frame, a well (a printed box
// round a group), a rule, an outlined number box, and the display's frame and glass.
#pragma once
#include "Pen.h"

namespace synthkit::draw {

/** A cream title bar with the title in the ground's ink. Kind "section". */
void titleBar (const Pen&, juce::Rectangle<float> r, const juce::String& title, const juce::String& id = {});

/** A printed box round a group of controls: a thin ink outline, no fill. Kind "well". */
void well (const Pen&, juce::Rectangle<float> r, const juce::String& id = {});

/** A printed line. */
void rule (const Pen&, juce::Point<float> a, juce::Point<float> b, float width = 0.0f);

/** An outlined box with a digit or letter in it (the oscillators' 1 and 2). */
void numberBox (const Pen&, juce::Rectangle<float> r, const juce::String& s, float size);

/** The display: edge, bezel, a glass gradient with a soft inner shadow at the
    top, and its lines of text, the first at `firstSize` and the rest at
    `lineSize`. Kind "display", text the lines joined. */
void display (const Pen&, juce::Rectangle<float> r, const juce::StringArray& lines,
              float firstSize, float lineSize, const juce::String& id = {});

} // namespace synthkit::draw
