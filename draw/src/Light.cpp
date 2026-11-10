#include "draw/Light.h"
#include "draw/Tokens.h"

namespace synthkit::draw {

namespace {

void disc (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour col)
{
    g.setColour (col);
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
}

} // namespace

void glowDot (const Pen& p, juce::Point<float> c, float r, juce::Colour body, float haloFrac, float coreFrac)
{
    auto& g = p.g;
    const float hr = r * haloFrac;
    juce::ColourGradient halo (body.withAlpha (0.55f), c.x, c.y, body.withAlpha (0.0f), c.x + hr, c.y, true);
    halo.addColour (r / hr, body.withAlpha (0.45f));
    g.setGradientFill (halo);
    g.fillEllipse (c.x - hr, c.y - hr, 2.0f * hr, 2.0f * hr);
    disc (g, c, r, body);
    juce::ColourGradient core (p.t.ledCore, c.x, c.y, body, c.x + r, c.y, true);
    core.addColour (coreFrac, p.t.ledCore);
    g.setGradientFill (core);
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
}

void light (const Pen& p, juce::Point<float> c, float r, bool lit, const juce::String& id)
{
    auto& g = p.g;
    if (lit)
    {
        // body to r, core to coreFrac r, halo to haloFrac r: the baseline's lit LED
        glowDot (p, c, r, p.t.ledBody, p.t.ledHaloFrac, p.t.ledCoreFrac);
    }
    else
    {
        // bezel, ring, olive disc and its highlight, and NOTHING outside r
        disc (g, c, r, p.t.ledBezel);
        disc (g, c, r * 0.84f, p.t.ledRing);
        disc (g, c, r * 0.68f, p.t.ledOff);
        disc (g, c.translated (-r * 0.2f, -r * 0.22f), r * 0.24f, p.t.ledOffHi.withAlpha (0.8f));
    }
    p.mark ("light", id, lit ? "lit" : "unlit",
            juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c));
}

void glowStroke (const Pen& p, const juce::Path& path, juce::Colour c, float width, float halo, float alpha)
{
    for (int i = 3; i >= 1; --i)
    {
        p.g.setColour (c.withAlpha (alpha * 0.12f));
        p.g.strokePath (path, juce::PathStrokeType (width * (1.0f + halo * (float) i / 3.0f),
                                                   juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    p.g.setColour (c.withMultipliedAlpha (alpha));
    p.g.strokePath (path, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void chevron (const Pen& p, juce::Point<float> c, float s, juce::Colour col, int dir)
{
    juce::Path t;
    const float h = s * 0.5f;
    switch (dir)
    {
        case 0:  t.addTriangle (c.x + h, c.y - h, c.x + h, c.y + h, c.x - h, c.y); break;
        case 1:  t.addTriangle (c.x - h, c.y - h, c.x - h, c.y + h, c.x + h, c.y); break;
        case 2:  t.addTriangle (c.x - h, c.y + h, c.x + h, c.y + h, c.x, c.y - h); break;
        default: t.addTriangle (c.x - h, c.y - h, c.x + h, c.y - h, c.x, c.y + h); break;
    }
    p.g.setColour (col);
    p.g.fillPath (t);
}

} // namespace synthkit::draw
