// The token table a synth fills. Every colour, ratio and size a piece draws is
// one of these, so a synth's look is one table with a source line per value
// (Sub37: src/ui/Sub37Tokens.h, from its measured baseline), and a gate can read
// the same table the painter used.
#pragma once
#include <juce_graphics/juce_graphics.h>

namespace synthkit::draw {

struct Tokens
{
    // ground and print
    juce::Colour ground { 0xff141312 }, ink { 0xffe9e4d7 };
    juce::Colour titleFill { 0xffe9e4d7 }, titleInk { 0xff343032 };
    float titleBarH = 16.0f, titleSize = 12.0f, titleCorner = 1.0f, titleInset = 3.0f;
    float wellStroke = 1.0f, ruleStroke = 1.0f;

    // knob
    juce::Colour knobSkirt { 0xff161616 }, knobSkirtEdge { 0xff050505 }, knobChamfer { 0xff35342f };
    juce::Colour knobCapHi { 0xffc4c3bf }, knobCapMid { 0xffa9a8a3 }, knobCapLo { 0xff82817d };
    juce::Colour knobCentre { 0xffc8c7c3 }, knobPointer { 0xfff4f1ea };
    float knobCapRatio = 0.62f, pointerFrom = 0.25f, pointerTo = 0.86f, pointerWidth = 0.067f;
    float travelDeg = 140.0f, skirtShadow = 0.045f;

    // cap (rectangular button)
    juce::Colour capTop { 0xff2a2724 }, capBottom { 0xff171513 }, capEdge { 0xff000000 };
    juce::Colour capHighlight { 0x598e8584 };    // #8e8584 at 0.35
    juce::Colour accentLo { 0xffc67c12 }, accentHi { 0xffd19640 };
    float capCorner = 3.4f, capEdgeW = 1.0f, capHighlightH = 2.0f, capShadowH = 4.0f;

    // round button and lens
    juce::Colour roundFace { 0xff0d0d0d }, roundRing { 0xff161616 }, roundEdge { 0xff222222 };
    juce::Colour lensOnLo { 0xffe0800d }, lensOnHi { 0xfff3b232 }, lensOff { 0xff2a2620 };

    // light (LED): two looks, nothing between
    juce::Colour ledBezel { 0xff0a0a09 }, ledRing { 0xff2a2825 }, ledOff { 0xff4d4a1c }, ledOffHi { 0xff6b6726 };
    juce::Colour ledCore { 0xfffffb72 }, ledBody { 0xffc67c16 };
    float ledCoreFrac = 0.6f, ledBodyFrac = 1.1f, ledHaloFrac = 1.46f;

    // display
    juce::Colour lcdEdge { 0xff55615f }, lcdBezel { 0xff0b0809 };
    juce::Colour lcdGlassTop { 0xffcfdde1 }, lcdGlassBottom { 0xffb9ccd2 }, lcdInk { 0xff2c3f55 };
    float lcdEdgeW = 1.0f, lcdBezelW = 2.5f, lcdShadowH = 4.0f;

    // type
    juce::Typeface::Ptr face, faceBold, faceLcd;   // null: the platform sans / mono
    float capHeightRatio = 0.71f;                  // cap height over nominal size, measured
    float minText = 9.0f;                          // no word is drawn smaller than this
    float legendSize = 12.0f, capLabelSize = 12.0f, numeralSize = 9.0f;

    // readout (the value shown while a control moves)
    juce::Colour readoutFill { 0xf0141312 }, readoutInk { 0xfffffb72 };
};

} // namespace synthkit::draw
