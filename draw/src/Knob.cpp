#include "draw/Knob.h"
#include "draw/Tokens.h"
#include "draw/Type.h"

namespace synthkit::draw {

juce::Point<float> polar (juce::Point<float> c, float rad, float deg)
{
    const float a = juce::degreesToRadians (deg);
    return { c.x + rad * std::sin (a), c.y - rad * std::cos (a) };
}

namespace {

void ring (juce::Graphics& g, juce::Point<float> c, float r, float w, juce::Colour col)
{
    g.setColour (col);
    g.drawEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r, w);
}

} // namespace

void knob (const Pen& p, juce::Point<float> c, float r, float v, float fromDeg, float toDeg, const juce::String& id)
{
    auto& g = p.g;
    const auto& t = p.t;
    // the skirt's shadow, below only: all depth on this panel is vertical
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - r, c.y - r + r * t.skirtShadow, 2.0f * r, 2.0f * r);
    // the skirt, its chamfer and its edge
    g.setColour (t.knobSkirt);
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
    ring (g, c, r * 0.955f, r * 0.03f, t.knobChamfer);
    ring (g, c, r * 0.99f, r * 0.045f, t.knobSkirtEdge);

    // the spun cap: specular top-left falling to the bottom right, a brighter
    // centre, faint concentric spinning, a dark lip
    const float cr = r * t.knobCapRatio;
    juce::ColourGradient spec (t.knobCapHi, c.x - cr * 0.7f, c.y - cr * 0.7f,
                               t.knobCapLo, c.x + cr * 0.7f, c.y + cr * 0.7f, false);
    spec.addColour (0.5, t.knobCapMid);
    g.setGradientFill (spec);
    g.fillEllipse (c.x - cr, c.y - cr, 2.0f * cr, 2.0f * cr);
    for (int i = 1; i <= 5; ++i)
        ring (g, c, cr * (float) i / 6.0f, cr * 0.012f, juce::Colours::white.withAlpha (0.07f));
    juce::ColourGradient centre (t.knobCentre.withAlpha (0.9f), c.x, c.y,
                                 t.knobCentre.withAlpha (0.0f), c.x + cr * 0.35f, c.y, true);
    g.setGradientFill (centre);
    g.fillEllipse (c.x - cr * 0.35f, c.y - cr * 0.35f, cr * 0.7f, cr * 0.7f);
    ring (g, c, cr, r * 0.02f, juce::Colour (0xff333230));

    // the pointer, drawn live
    const float a = fromDeg + juce::jlimit (0.0f, 1.0f, v) * (toDeg - fromDeg);
    juce::Path ptr;
    ptr.startNewSubPath (polar (c, r * t.pointerFrom, a));
    ptr.lineTo (polar (c, r * t.pointerTo, a));
    g.setColour (t.knobPointer);
    g.strokePath (ptr, juce::PathStrokeType (r * t.pointerWidth, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    p.mark ("knob", id, juce::String (v, 3), juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c));
}

void dial (const Pen& p, juce::Point<float> c, float r, const Dial& d, float numeralSize)
{
    auto& g = p.g;
    g.setColour (p.t.ink);
    // the printed ring is an obstacle: kind "dial", bounds the outer circle, size
    // the inner radius, so a gate can test a word against the ring itself
    if (d.ticks > 0)
        p.mark ("dial", {}, juce::String (d.start) + "," + juce::String (d.end),
                juce::Rectangle<float> (2.0f * r * d.outer, 2.0f * r * d.outer).withCentre (c), r * d.inner);
    if (d.ticks > 0)
    {
        const juce::PathStrokeType st (juce::jmax (0.6f, r * d.weight), juce::PathStrokeType::mitered,
                                       juce::PathStrokeType::butt);
        for (int i = 0; i < d.ticks; ++i)
        {
            const float a = d.ticks == 1 ? d.start
                                         : d.start + (d.end - d.start) * (float) i / (float) (d.ticks - 1);
            juce::Path tick;
            tick.startNewSubPath (polar (c, r * d.inner, a));
            tick.lineTo (polar (c, r * d.outer, a));
            g.strokePath (tick, st);
        }
    }
    for (const auto& m : d.marks)
    {
        const auto at = polar (c, r * m.rf, m.angle);
        const float w = widthOf (p, m.text, numeralSize) + numeralSize;
        text (p, m.text, juce::Rectangle<float> (w, numeralSize * 1.3f).withCentre (at), numeralSize,
              p.t.ink, juce::Justification::centred);
    }
}

} // namespace synthkit::draw
