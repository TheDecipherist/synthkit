// The pressable parts: the rectangular cap, the round button, the lit lens, and
// the click-the-choice selector that replaces "a button plus a row of lamps".
// Every one of them has two looks and is told which; none decides for itself.
#pragma once
#include "Pen.h"
#include <vector>

namespace synthkit::draw {

/** A cap: a moulded body with a black edge, a highlight under its top edge and a
    soft shadow below. Lit is the FULL ORANGE body, unlit the dark moulding:
    two looks, no rim (`accent` is kept for callers and changes nothing). Kind
    "cap", text "lit"/"unlit". */
void cap (const Pen&, juce::Rectangle<float> r, bool lit, bool accent = false, const juce::String& id = {});

/** A round button. Lit lights its WHOLE face, the way the machine's do. */
void roundButton (const Pen&, juce::Point<float> c, float r, bool lit, const juce::String& id = {});

/** An illuminated lens (a mixer ON button): orange and glowing when on, dark glass
    when off. Two pictures, never the same one (Sub37 B-1). */
void lens (const Pen&, juce::Point<float> c, float r, bool on, const juce::String& id = {});

/** The segments a selector of n choices divides `r` into, in order. */
[[nodiscard]] std::vector<juce::Rectangle<float>> selectorSegments (juce::Rectangle<float> r, int n, bool vertical);

/** A click-the-choice selector: one body, n segments, each with its own lamp
    and its label; `lit[i]` says which are lit (more than one may be: "both").
    `lampIds[i]` names each segment's lamp in the ledger. Kind "selector". */
void selector (const Pen&, juce::Rectangle<float> r, const juce::StringArray& labels,
               const std::vector<bool>& lit, bool vertical, float labelSize,
               const juce::StringArray& lampIds = {}, const juce::String& id = {});

/** The value shown while a control moves: a small dark box with lamp-coloured text. */
void readout (const Pen&, juce::Point<float> anchor, const juce::String& s, float size);

} // namespace synthkit::draw
