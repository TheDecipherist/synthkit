#include "draw/Glyphs.h"
#include <initializer_list>

namespace synthkit::draw {

namespace {

using P = juce::Point<float>;

juce::Path poly (juce::Rectangle<float> r, std::initializer_list<P> unit)
{
    juce::Path p;
    bool first = true;
    for (const auto& u : unit)
    {
        const P at { r.getX() + u.x * r.getWidth(), r.getY() + u.y * r.getHeight() };
        if (first) p.startNewSubPath (at); else p.lineTo (at);
        first = false;
    }
    return p;
}

juce::Path shapeFor (const juce::String& s, juce::Rectangle<float> r)
{
    if (s == "tri")    return poly (r, { { 0, 1 }, { 0.5f, 0 }, { 1, 1 } });
    if (s == "square") return poly (r, { { 0, 1 }, { 0, 0 }, { 0.5f, 0 }, { 0.5f, 1 }, { 1, 1 }, { 1, 0 } });
    if (s == "saw")    return poly (r, { { 0, 1 }, { 1, 0 }, { 1, 1 } });
    if (s == "sawdn")  return poly (r, { { 0, 1 }, { 0, 0 }, { 1, 1 } });
    if (s == "pulse")  return poly (r, { { 0, 1 }, { 0, 0 }, { 0.2f, 0 }, { 0.2f, 1 }, { 1, 1 }, { 1, 0 } });
    if (s == "sh")     return poly (r, { { 0, 0.6f }, { 0.25f, 0.6f }, { 0.25f, 0.1f }, { 0.5f, 0.1f }, { 0.5f, 0.9f },
                                         { 0.75f, 0.9f }, { 0.75f, 0.35f }, { 1, 0.35f } });
    if (s == "envA")   return poly (r, { { 0, 1 }, { 0.6f, 0 }, { 1, 0 } });
    if (s == "envD")   return poly (r, { { 0, 0 }, { 0.4f, 0 }, { 1, 0.75f } });
    if (s == "envS")   return poly (r, { { 0, 0.5f }, { 1, 0.5f } });
    if (s == "envR")   return poly (r, { { 0, 0.5f }, { 0.35f, 0.5f }, { 1, 1 } });
    if (s == "lp")
    {
        juce::Path p;
        p.startNewSubPath (r.getX(), r.getY() + r.getHeight() * 0.45f);
        p.lineTo (r.getX() + r.getWidth() * 0.5f, r.getY() + r.getHeight() * 0.45f);
        p.quadraticTo (r.getX() + r.getWidth() * 0.66f, r.getY(), r.getX() + r.getWidth() * 0.72f,
                       r.getY() + r.getHeight() * 0.45f);
        p.quadraticTo (r.getX() + r.getWidth() * 0.8f, r.getBottom(), r.getRight(), r.getBottom());
        return p;
    }
    if (s == "phones")
    {
        juce::Path p;
        p.addCentredArc (r.getCentreX(), r.getBottom() - r.getHeight() * 0.25f, r.getWidth() * 0.42f,
                         r.getHeight() * 0.7f, 0.0f, -1.45f, 1.45f, true);
        p.addRectangle (r.getX(), r.getBottom() - r.getHeight() * 0.38f, r.getWidth() * 0.2f, r.getHeight() * 0.38f);
        p.addRectangle (r.getRight() - r.getWidth() * 0.2f, r.getBottom() - r.getHeight() * 0.38f,
                        r.getWidth() * 0.2f, r.getHeight() * 0.38f);
        return p;
    }
    return {};
}

} // namespace

bool knownGlyph (const juce::String& s)
{
    return ! shapeFor (s, { 0, 0, 10, 10 }).isEmpty();
}

void glyph (const Pen& p, const juce::String& shape, juce::Rectangle<float> r, juce::Colour ink, float stroke)
{
    const auto path = shapeFor (shape, r);
    p.g.setColour (ink);
    p.g.strokePath (path, juce::PathStrokeType (stroke, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
    p.mark ("glyph", {}, shape, r);
}

} // namespace synthkit::draw
