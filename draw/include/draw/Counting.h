// A software renderer that COUNTS drawImage calls, for gates. Paint into a
// juce::Graphics made on this context and `images` says how many pictures the
// paint pasted; a panel drawn with this package says 0, and a gate that plants
// one image must see 1. Header only: a probe includes it, the plugin does not.
#pragma once
#include <juce_graphics/juce_graphics.h>

namespace synthkit::draw {

class CountingContext final : public juce::LowLevelGraphicsSoftwareRenderer
{
public:
    explicit CountingContext (const juce::Image& im) : juce::LowLevelGraphicsSoftwareRenderer (im) {}
    void drawImage (const juce::Image& im, const juce::AffineTransform& t) override
    {
        ++images;
        juce::LowLevelGraphicsSoftwareRenderer::drawImage (im, t);
    }
    int images = 0;
};

} // namespace synthkit::draw
