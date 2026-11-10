// Light. A lamp has EXACTLY TWO LOOKS: lit (body and glow) and unlit (a dark body
// and zero glow outside it). Nothing between, and nothing drawn lit that its
// owner has not lit (Sub37 B-1: thirty lamps painted into the background, the
// mixer's ON buttons the same picture on and off).
#pragma once
#include "Pen.h"

namespace synthkit::draw {

/** A panel lamp of radius r. Recorded as kind "light", text "lit" or "unlit". */
void light (const Pen&, juce::Point<float> centre, float r, bool lit, const juce::String& id = {});

/** A glowing dot: a core, a body and a halo falling to nothing at haloFrac * r. */
void glowDot (const Pen&, juce::Point<float> centre, float r, juce::Colour body,
              float haloFrac = 1.46f, float coreFrac = 0.6f);

/** A stroke with a soft halo of `halo` widths either side. */
void glowStroke (const Pen&, const juce::Path&, juce::Colour c, float width, float halo = 3.0f,
                 float alpha = 1.0f);

/** A filled triangle pointing left or right (or up/down), centred on `c`. */
void chevron (const Pen&, juce::Point<float> c, float size, juce::Colour, int direction);   // 0 L, 1 R, 2 U, 3 D

} // namespace synthkit::draw
