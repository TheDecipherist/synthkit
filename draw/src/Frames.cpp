#include "draw/Frames.h"
#include "draw/Tokens.h"
#include "draw/Type.h"

namespace synthkit::draw {

void titleBar (const Pen& p, juce::Rectangle<float> r, const juce::String& title, const juce::String& id)
{
    p.g.setColour (p.t.titleFill);
    p.g.fillRoundedRectangle (r, p.t.titleCorner);
    p.mark ("section", id, title, r);
    text (p, title, r.reduced (p.t.titleInset, 0.0f), p.t.titleSize, p.t.titleInk,
          juce::Justification::centred, true, p.t.titleSize * 0.06f);
}

void well (const Pen& p, juce::Rectangle<float> r, const juce::String& id)
{
    p.g.setColour (p.t.ink.withAlpha (0.9f));
    p.g.drawRoundedRectangle (r, 2.0f, p.t.wellStroke);
    p.mark ("well", id, {}, r);
}

void rule (const Pen& p, juce::Point<float> a, juce::Point<float> b, float width)
{
    const float w = width > 0.0f ? width : p.t.ruleStroke;
    p.g.setColour (p.t.ink);
    p.g.drawLine ({ a, b }, w);
    // a rule is an obstacle too: a word printed across one is an overlap
    p.mark ("rule", {}, juce::String (a.x) + "," + juce::String (a.y) + "," + juce::String (b.x) + "," + juce::String (b.y),
            juce::Rectangle<float> (a, b).expanded (w * 0.5f), w);
}

void numberBox (const Pen& p, juce::Rectangle<float> r, const juce::String& s, float size)
{
    p.g.setColour (p.t.ink);
    p.g.drawRect (r, p.t.ruleStroke);
    text (p, s, r, size, p.t.ink, juce::Justification::centred, true);
}

void display (const Pen& p, juce::Rectangle<float> r, const juce::StringArray& lines,
              float firstSize, float lineSize, const juce::String& id)
{
    auto& g = p.g;
    const auto& t = p.t;
    g.setColour (t.lcdEdge);
    g.fillRoundedRectangle (r, 3.0f);
    const auto bezel = r.reduced (t.lcdEdgeW);
    g.setColour (t.lcdBezel);
    g.fillRoundedRectangle (bezel, 2.5f);
    const auto glass = bezel.reduced (t.lcdBezelW);
    juce::ColourGradient gl (t.lcdGlassTop, 0.0f, glass.getY(), t.lcdGlassBottom, 0.0f, glass.getBottom(), false);
    g.setGradientFill (gl);
    g.fillRoundedRectangle (glass, 1.5f);
    juce::ColourGradient sh (juce::Colours::black.withAlpha (0.35f), 0.0f, glass.getY(),
                             juce::Colours::black.withAlpha (0.0f), 0.0f, glass.getY() + t.lcdShadowH, false);
    g.setGradientFill (sh);
    g.fillRect (glass.withHeight (t.lcdShadowH));

    p.mark ("display", id, lines.joinIntoString (" / "), r);
    const auto inner = glass.reduced (glass.getHeight() * 0.08f, glass.getHeight() * 0.06f);
    float total = 0.0f;
    for (int i = 0; i < lines.size(); ++i) total += (i == 0 ? firstSize : lineSize) * 1.18f;
    float y = inner.getCentreY() - total * 0.5f;
    Pen lcd = p;
    Tokens lt = t;
    if (t.faceLcd != nullptr) { lt.face = t.faceLcd; lt.faceBold = t.faceLcd; }
    Pen lp { p.g, lt, p.ledger, p.section };
    for (int i = 0; i < lines.size(); ++i)
    {
        const float sz = i == 0 ? firstSize : lineSize;
        text (lp, lines[i], { inner.getX(), y, inner.getWidth(), sz * 1.18f }, sz, t.lcdInk,
              juce::Justification::centredLeft, i == 0);
        y += sz * 1.18f;
    }
    juce::ignoreUnused (lcd);
}

} // namespace synthkit::draw
