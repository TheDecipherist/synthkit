#include "draw/Caps.h"
#include "draw/Light.h"
#include "draw/Tokens.h"
#include "draw/Type.h"

namespace synthkit::draw {


void cap (const Pen& p, juce::Rectangle<float> r, bool lit, bool accent, const juce::String& id)
{
    auto& g = p.g;
    const auto& t = p.t;
    // shadow below, fading over capShadowH
    juce::ColourGradient sh (juce::Colours::black.withAlpha (0.45f), 0.0f, r.getBottom(),
                             juce::Colours::black.withAlpha (0.0f), 0.0f, r.getBottom() + t.capShadowH, false);
    g.setGradientFill (sh);
    g.fillRect (r.getX() + 1.0f, r.getBottom(), r.getWidth() - 2.0f, t.capShadowH);
    // edge, body, highlight
    g.setColour (t.capEdge);
    g.fillRoundedRectangle (r, t.capCorner);
    const auto body = r.reduced (t.capEdgeW);
    // LIT IS THE FULL ORANGE BODY (the author, 2026-11-01): "ORANGE is for ON/ACTIVE, use
    // the full orange button, not the yellow border". Every cap: orange when lit,
    // the dark moulding when not (B-8). `accent` no longer changes the picture.
    juce::ignoreUnused (accent);
    juce::ColourGradient fill (lit ? t.accentHi : t.capTop, 0.0f, body.getY(),
                               lit ? t.accentLo : t.capBottom, 0.0f, body.getBottom(), false);
    g.setGradientFill (fill);
    g.fillRoundedRectangle (body, juce::jmax (0.0f, t.capCorner - t.capEdgeW));
    g.setColour (t.capHighlight);
    g.fillRect (body.getX() + t.capCorner * 0.6f, body.getY() + 0.5f,
                body.getWidth() - t.capCorner * 1.2f, t.capHighlightH);
    p.mark ("cap", id, lit ? "lit" : "unlit", r);
}

void roundButton (const Pen& p, juce::Point<float> c, float r, bool lit, const juce::String& id)
{
    auto& g = p.g;
    const auto& t = p.t;
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - r, c.y - r + r * 0.08f, 2.0f * r, 2.0f * r);
    g.setColour (t.roundEdge);
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
    g.setColour (t.roundRing);
    g.fillEllipse (c.x - r * 0.92f, c.y - r * 0.92f, r * 1.84f, r * 1.84f);
    const float fr = r * 0.74f;
    if (lit)
        glowDot (p, c, fr, t.ledBody, 1.0f / 0.74f * 1.15f, 0.55f);
    else
    {
        g.setColour (t.roundFace);
        g.fillEllipse (c.x - fr, c.y - fr, 2.0f * fr, 2.0f * fr);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawEllipse (c.x - fr, c.y - fr, 2.0f * fr, 2.0f * fr, r * 0.05f);
    }
    p.mark ("round", id, lit ? "lit" : "unlit", juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c));
}

void lens (const Pen& p, juce::Point<float> c, float r, bool on, const juce::String& id)
{
    auto& g = p.g;
    const auto& t = p.t;
    g.setColour (t.roundEdge);
    g.fillEllipse (c.x - r * 1.18f, c.y - r * 1.18f, r * 2.36f, r * 2.36f);
    if (on)
    {
        juce::ColourGradient glow (t.lensOnHi.withAlpha (0.5f), c.x, c.y, t.lensOnHi.withAlpha (0.0f),
                                   c.x + r * 1.9f, c.y, true);
        g.setGradientFill (glow);
        g.fillEllipse (c.x - r * 1.9f, c.y - r * 1.9f, r * 3.8f, r * 3.8f);
        juce::ColourGradient face (t.lensOnHi, c.x - r * 0.3f, c.y - r * 0.4f, t.lensOnLo, c.x + r, c.y + r, true);
        g.setGradientFill (face);
    }
    else
        g.setColour (t.lensOff);
    g.fillEllipse (c.x - r, c.y - r, 2.0f * r, 2.0f * r);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.45f : 0.10f));
    g.fillEllipse (c.x - r * 0.45f, c.y - r * 0.55f, r * 0.45f, r * 0.3f);
    p.mark ("lens", id, on ? "lit" : "unlit", juce::Rectangle<float> (2.0f * r, 2.0f * r).withCentre (c));
}

std::vector<juce::Rectangle<float>> selectorSegments (juce::Rectangle<float> r, int n, bool vertical)
{
    std::vector<juce::Rectangle<float>> out;
    for (int i = 0; i < n; ++i)
        out.push_back (vertical ? juce::Rectangle<float> (r.getX(), r.getY() + r.getHeight() * (float) i / (float) n,
                                                          r.getWidth(), r.getHeight() / (float) n)
                                : juce::Rectangle<float> (r.getX() + r.getWidth() * (float) i / (float) n, r.getY(),
                                                          r.getWidth() / (float) n, r.getHeight()));
    return out;
}

void selector (const Pen& p, juce::Rectangle<float> r, const juce::StringArray& labels,
               const std::vector<bool>& lit, bool vertical, float labelSize,
               const juce::StringArray& lampIds, const juce::String& id)
{
    auto& g = p.g;
    const auto& t = p.t;
    g.setColour (t.capEdge);
    g.fillRoundedRectangle (r, t.capCorner);
    const auto segs = selectorSegments (r.reduced (t.capEdgeW), labels.size(), vertical);
    for (int i = 0; i < labels.size(); ++i)
    {
        const bool on = i < (int) lit.size() && lit[(size_t) i];
        const auto s = segs[(size_t) i].reduced (0.5f);
        juce::ColourGradient fill (on ? t.capTop.brighter (0.25f) : t.capTop, 0.0f, s.getY(),
                                   t.capBottom, 0.0f, s.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRoundedRectangle (s, juce::jmax (0.0f, t.capCorner - 1.5f));
        const float lr = juce::jmin (s.getHeight(), s.getWidth()) * 0.16f;
        const auto lampAt = vertical ? juce::Point<float> (s.getX() + lr * 2.2f, s.getCentreY())
                                     : juce::Point<float> (s.getCentreX(), s.getY() + lr * 1.9f);
        light (p, lampAt, juce::jlimit (2.0f, 5.0f, lr), on, i < lampIds.size() ? lampIds[i] : juce::String());
        const auto labelBox = vertical ? s.withTrimmedLeft (lr * 4.0f)
                                       : s.withTrimmedTop (lr * 3.6f).reduced (1.0f, 0.0f);
        text (p, labels[i], labelBox, labelSize, on ? t.ledCore : t.ink, juce::Justification::centred);
    }
    p.mark ("selector", id, labels.joinIntoString ("|"), r);
}

void readout (const Pen& p, juce::Point<float> anchor, const juce::String& s, float size)
{
    const float w = widthOf (p, s, size, true) + size * 1.2f, h = size * 1.6f;
    const juce::Rectangle<float> r (anchor.x - w * 0.5f, anchor.y - h, w, h);
    p.g.setColour (p.t.readoutFill);
    p.g.fillRoundedRectangle (r, 3.0f);
    p.g.setColour (p.t.readoutInk.withAlpha (0.5f));
    p.g.drawRoundedRectangle (r, 3.0f, 1.0f);
    text (p, s, r, size, p.t.readoutInk, juce::Justification::centred, true);
}

} // namespace synthkit::draw
