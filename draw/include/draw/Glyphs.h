// The printed glyphs, as paths: waveforms (tri, square, saw, sawdn, sh, pulse),
// the low-pass curve (lp), the envelope stages (envA, envD, envS, envR) and the
// headphone mark (phones). Drawn in `r` with the given ink. Kind "glyph".
#pragma once
#include "Pen.h"

namespace synthkit::draw {

/** True when `shape` is one this package draws. */
[[nodiscard]] bool knownGlyph (const juce::String& shape);
void glyph (const Pen&, const juce::String& shape, juce::Rectangle<float> r, juce::Colour ink, float stroke = 1.4f);

} // namespace synthkit::draw
