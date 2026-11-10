// The Pen every piece draws with, and the Ledger a gate reads back.
// Lifted from Stellix's lib/painter/src/draw.h (Pen, Ledger, col); here the
// units are the caller's DESIGN units, not fractions of a window: the caller
// scales the Graphics once and every piece is vector at any size.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace synthkit::draw {

struct Tokens;

/** What a paint drew, part by part. A gate reads this instead of the spec, so
    the question "is GLIDE in the header" is answered by the drawing itself. */
struct Ledger
{
    struct Item
    {
        juce::String section, kind, id, text;
        juce::Rectangle<float> bounds;   // design units
        float size = 0.0f;               // the type size used, for kind "text"
    };
    std::vector<Item> items;
    int imagesDrawn = 0;                 // a piece that ever draws an image says so here
    void clear() { items.clear(); imagesDrawn = 0; }
};

struct Pen
{
    juce::Graphics& g;
    const Tokens& t;
    Ledger* ledger = nullptr;
    juce::String section;

    [[nodiscard]] Pen in (const juce::String& s) const { Pen p = *this; p.section = s; return p; }
    void mark (const juce::String& kind, const juce::String& id, const juce::String& text,
               juce::Rectangle<float> bounds, float size = 0.0f) const
    {
        if (ledger != nullptr) ledger->items.push_back ({ section, kind, id, text, bounds, size });
    }
};

inline juce::Colour col (juce::uint32 argb, float alpha = 1.0f)
{
    return juce::Colour (argb).withMultipliedAlpha (alpha);
}

} // namespace synthkit::draw
