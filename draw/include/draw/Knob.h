// The knob and the dial printed around it. Measured parts (Sub37 baseline,
// analysis.md section 4): a near-black skirt with a dark edge and a lighter
// chamfer, a spun aluminium cap at Tokens::knobCapRatio of the radius with a
// top-left specular, and a cream pointer from pointerFrom to pointerTo of the
// radius. Angles are degrees, 0 at the top, clockwise positive.
#pragma once
#include "Pen.h"
#include <vector>

namespace synthkit::draw {

/** A printed scale: `ticks` rays evenly from start to end between inner and outer
    radii (in knob radii), `weight` their width in knob radii, and numerals. */
struct Dial
{
    int ticks = 0;
    float start = -135.0f, end = 135.0f, inner = 1.15f, outer = 1.67f, weight = 0.075f;
    struct Mark { float angle = 0.0f; juce::String text; float rf = 2.0f; };
    std::vector<Mark> marks;
};

/** The point at `deg` on a circle of radius `rad` about `c`. */
[[nodiscard]] juce::Point<float> polar (juce::Point<float> c, float rad, float deg);

/** A knob at value 0..1 swept from `fromDeg` to `toDeg`. Kind "knob" in the ledger. */
void knob (const Pen&, juce::Point<float> c, float r, float value01,
           float fromDeg = -140.0f, float toDeg = 140.0f, const juce::String& id = {});

/** The printed scale around a knob of radius r. Numerals at `numeralSize`. */
void dial (const Pen&, juce::Point<float> c, float r, const Dial&, float numeralSize);

} // namespace synthkit::draw
