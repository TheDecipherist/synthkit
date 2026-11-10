#include "draw/Type.h"
#include "draw/Tokens.h"

namespace synthkit::draw {

juce::Font font (const Pen& p, float size, bool bold, float tracking)
{
    auto o = juce::FontOptions().withPointHeight (size);
    if (auto tf = bold ? (p.t.faceBold != nullptr ? p.t.faceBold : p.t.face) : p.t.face)
        o = o.withTypeface (tf);
    else
        o = o.withStyle (bold ? "Bold" : "Regular");
    juce::Font f (o);
    if (tracking != 0.0f) f.setExtraKerningFactor (tracking / juce::jmax (1.0f, size));
    return f;
}

float widthOf (const Pen& p, const juce::String& s, float size, bool bold, float tracking)
{
    return juce::GlyphArrangement::getStringWidth (font (p, size, bold, tracking), s);
}

namespace {

/** The split nearest the middle of the string, or -1 when it has no space. */
int splitPoint (const juce::String& s)
{
    int best = -1;
    for (int i = 0; i < s.length(); ++i)
        if (s[i] == ' ' && (best < 0 || std::abs (i - s.length() / 2) < std::abs (best - s.length() / 2)))
            best = i;
    return best;
}

/** One line in `r`. BOLD WITHOUT A BOLD FILE: a synth that bundles one variable
    font (Sub37: Roboto Condensed) gets its bold by filling the glyph outlines and
    stroking them at 4.5% of the size, which is what a 700 weight adds to a 400. */
void drawLine (const Pen& p, const juce::String& l, juce::Rectangle<float> r, const juce::Font& f,
               juce::Justification j, bool fakeBold, float size)
{
    if (! fakeBold) { p.g.drawText (l, r, j, false); return; }
    juce::GlyphArrangement ga;
    ga.addFittedText (f, l, r.getX(), r.getY(), r.getWidth(), r.getHeight(), j, 1, 1.0f);
    juce::Path path;
    ga.createPath (path);
    p.g.fillPath (path);
    p.g.strokePath (path, juce::PathStrokeType (size * 0.045f));
}

bool fakeBoldFor (const Pen& p, bool bold) { return bold && p.t.faceBold == nullptr && p.t.face != nullptr; }

} // namespace

float text (const Pen& p, const juce::String& s, juce::Rectangle<float> r, float size, juce::Colour c,
            juce::Justification j, bool bold, float tracking, const juce::String& id)
{
    if (s.isEmpty()) return 0.0f;
    size = juce::jmax (size, p.t.minText);
    juce::StringArray lines { s };
    if (widthOf (p, s, size, bold, tracking) > r.getWidth())
        if (const int at = splitPoint (s); at > 0)
            lines = juce::StringArray { s.substring (0, at), s.substring (at + 1) };

    // Shrink only when the widest line still has no room, and never past the floor.
    auto widest = [&] (float sz)
    {
        float w = 0.0f;
        for (const auto& l : lines) w = juce::jmax (w, widthOf (p, l, sz, bold, tracking));
        return w;
    };
    while (size > p.t.minText && widest (size) > r.getWidth())
        size = juce::jmax (p.t.minText, size - 0.5f);

    const auto f = font (p, size, bold, tracking);
    p.g.setFont (f);
    p.g.setColour (c);
    const float lineH = size * 1.12f;
    const float blockH = lineH * (float) lines.size();
    float y = r.getY();
    if (j.testFlags (juce::Justification::verticallyCentred)) y = r.getCentreY() - blockH * 0.5f;
    else if (j.testFlags (juce::Justification::bottom))      y = r.getBottom() - blockH;
    const auto hj = juce::Justification (j.getOnlyHorizontalFlags() | juce::Justification::verticallyCentred);
    for (const auto& l : lines)
    {
        const juce::Rectangle<float> lr (r.getX() - 40.0f, y, r.getWidth() + 80.0f, lineH);
        // Centred lines may overhang their box sideways rather than clip: the box
        // is where a legend is placed, the floor is what a legend may not cross.
        drawLine (p, l, j.testFlags (juce::Justification::horizontallyCentred) ? lr
                                                                                : lr.withX (r.getX()).withWidth (r.getWidth() + 40.0f),
                  f, hj, fakeBoldFor (p, bold), size);
        y += lineH;
    }
    // THE TRUE EXTENT, not the box it was placed in: what a gate asks "does this
    // word touch anything" of. Widest line by the block's height, where the
    // justification put it; the height is the cap height plus a line gap.
    const float ink = widest (size);
    float x0 = r.getX();
    if (j.testFlags (juce::Justification::horizontallyCentred)) x0 = r.getCentreX() - ink * 0.5f;
    else if (j.testFlags (juce::Justification::right))         x0 = r.getRight() - ink;
    const float firstY = j.testFlags (juce::Justification::verticallyCentred) ? r.getCentreY() - blockH * 0.5f
                       : j.testFlags (juce::Justification::bottom) ? r.getBottom() - blockH : r.getY();
    const float capH = size * p.t.capHeightRatio;
    const float top = firstY + (lineH - capH) * 0.5f;
    p.mark ("text", id, s, { x0, top, ink, blockH - (lineH - capH) }, size);
    return size;
}

void boxed (const Pen& p, const juce::String& s, juce::Point<float> centre, float size)
{
    size = juce::jmax (size, p.t.minText);
    const float w = widthOf (p, s, size, true) + size * 0.7f;
    const float h = size * 1.15f;
    const juce::Rectangle<float> r (centre.x - w * 0.5f, centre.y - h * 0.5f, w, h);
    p.g.setColour (p.t.ink);
    p.g.fillRoundedRectangle (r, size * 0.12f);
    p.g.setColour (p.t.ground);
    const auto f = font (p, size, true);
    p.g.setFont (f);
    drawLine (p, s, r, f, juce::Justification::centred, fakeBoldFor (p, true), size);
    p.mark ("text", {}, s, r, size);
}

} // namespace synthkit::draw
