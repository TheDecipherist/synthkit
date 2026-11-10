// Does the panel still get clicked where it is drawn once it stops fitting?
//
// A panel bigger than its window is scrolled rather than shrunk (synthkit
// panel/NativePanel.h, "the viewport"), and the whole feature turns on ONE thing:
// the point a mouse arrives at is in component pixels, and the hit boxes are in
// design units, so every handler has to put the scroll offset back before it asks
// which control it reached. Miss it in one handler and every knob under the cursor
// is the wrong one - and it still looks perfect at offset zero, which is where
// every existing gate, every render and every casual click happens to be.
//
// The other half is that this must change NOTHING for a panel that fits. Thirty-five
// units are on this component. So check 1 asserts, on the rect the editor actually
// gives the panel today, that the scale is the same float it always was, that
// controlBounds() is the same rectangle the pre-viewport formula produced, and that
// there is no offset to be had.
//
// Five checks:
//   1. IT FITS: scale is width/designWidth exactly, no scrolling on either axis,
//      controlBounds() matches the pre-viewport formula for every control, and an
//      offset cannot be forced onto it.
//   2. TOO TALL: squeeze the height and the panel scrolls vertically AT THE SAME
//      SCALE. Nothing shrinks; that is the point.
//   3. TOO SMALL TO READ: with a legibility floor declared, the scale stops falling
//      at the floor and the panel scrolls horizontally instead, and the smallest
//      legend lands on the screen at the floor's own pixel size.
//   4. THE CLICK FOLLOWS THE PAINT: at every scroll offset, taking each control's
//      drawn rectangle from controlBounds() and asking the panel what a click at its
//      centre reaches gives the same answer as at rest.
//   5. AND A REAL GESTURE DOES TOO: an actual mouseDown - plus a drag, for the
//      controls that only speak once they move - dispatched at that pixel emits that
//      control's id on onChange, so it is the handler being proved and not a second
//      copy of the arithmetic.
//
//   ExampleSynthViewportProbe             measure
//   ExampleSynthViewportProbe --selftest  make hit testing forget the offset and
//                                          require checks 4 and 5 to report it
//   ExampleSynthViewportProbe --png P     write P: the same panel fitting, then
//                                          scrolled to each end, one above the other.
//                                          Numbers do not show a scrollbar drawn in the
//                                          wrong place or a cue that never appears.
#include <juce_gui_extra/juce_gui_extra.h>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <NativePanel.h>
#include "BinaryData.h"

namespace {

int failures = 0;
void fail (const juce::String& what) { std::printf ("   FAIL %s\n", what.toRawUTF8()); ++failures; }

/** Where the plate's top-left sits inside the component. A plate SMALLER than its
    window is centred, and the sharp ceiling makes that the normal case: cap the scale
    below the window's ratio and the plate stops growing while the window does not.
    Computed from public API on purpose, so this states the policy instead of calling
    the implementation it is supposed to be checking. */
juce::Point<int> plateOrigin (const synthkit::NativePanel& panel)
{
    const auto c = panel.contentSize();
    return { juce::jmax (0, (panel.getWidth()  - c.x) / 2),
             juce::jmax (0, (panel.getHeight() - c.y) / 2) };
}

/** The formula controlBounds() used before the viewport existed, plus the centring the
    viewport added. Check 1 requires the new one to agree with it exactly on a panel
    that fits, which is the no-op claim. The centring term is not optional: without it
    this read every centred plate as 'every control moved', which is what it did on
    every Retina machine. */
juce::Rectangle<int> preViewportBounds (const juce::Rectangle<float>& drawn, float k,
                                        juce::Point<int> origin = {})
{
    return drawn.transformedBy (juce::AffineTransform::scale (k))
                .getSmallestIntegerContainer().translated (origin.x, origin.y);
}

struct Probe
{
    synthkit::NativePanel panel { SYNTHKIT_PANEL_ASSETS() };
    juce::String lastId;
    /** What each control resolved to with the panel at its origin, where a viewport
        fault cannot show. Both scrolled checks measure against THIS and not against a
        second live reading, or they would compare a broken answer with itself. */
    std::map<std::string, std::string> atRest;

    Probe() { panel.onChange = [this] (const synthkit::NativePanel::Event& e) { lastId = e.id; }; }

    /** What a click at the centre of each control's DRAWN rectangle reaches, keyed by
        the control it was drawn for. Computed through the same conversion the mouse
        goes through, from the same rectangle the editor is told to draw into. */
    std::map<std::string, std::string> reached() const
    {
        std::map<std::string, std::string> out;
        for (const auto& p : panel.placements())
        {
            if (p.id.isEmpty() || p.hit.isEmpty()) continue;
            const auto onScreen = panel.controlBounds (p.id).toFloat();
            out[p.id.toStdString()] = panel.idAt (panel.toDesign (onScreen.getCentre())).toStdString();
        }
        return out;
    }
};

void checkFits (Probe& pr)
{
    auto& panel = pr.panel;
    const int w = 1380;
    // Exactly what every unit's editor hands the panel today: the design aspect,
    // truncated to an int, which is why the fit test carries a couple of pixels of slack.
    const int h = (int) ((double) w * panel.designHeight() / panel.designWidth());
    panel.setBounds (0, 0, w, h);
    std::printf ("1. it fits: %dx%d component, %dx%d design\n", w, h,
                 panel.designWidth(), panel.designHeight());

    // Rule 1 of the scale policy is k = width/designWidth; rule 4 CAPS it at
    // sharpCeiling(), so no raster is ever upscaled past its own art. On a 2x export
    // and a 2x screen that cap is 1.0, which is BELOW this ratio, so a panel in a
    // window wider than its design correctly draws at 1.0 and centres. Asserting the
    // uncapped ratio is what made this probe fail on every Retina machine while
    // passing on a 1x one, for the eleven days nobody could reproduce it.
    const float kFit     = (float) w / (float) panel.designWidth();
    const float expected = juce::jmin (kFit, panel.sharpCeiling());
    if (panel.drawScale() != expected)
        fail ("scale is not the policy's on a panel that fits: " + juce::String (panel.drawScale(), 8)
              + " not " + juce::String (expected, 8)
              + " (fit " + juce::String (kFit, 8)
              + ", cap " + juce::String (panel.sharpCeiling(), 8) + ")");
    if (panel.scrollsHorizontally()) fail ("a panel that fits reports a horizontal scroll");
    if (panel.scrollsVertically())   fail ("a panel that fits reports a vertical scroll");

    panel.setViewOffset (999, 999);
    if (panel.viewOffset() != juce::Point<int> (0, 0))
        fail ("a panel that fits accepted a scroll offset");

    int checked = 0, moved = 0;
    for (const auto& p : panel.placements())
    {
        if (p.id.isEmpty()) continue;
        ++checked;
        if (panel.controlBounds (p.id) != preViewportBounds (p.drawn, expected, plateOrigin (panel))) ++moved;
    }
    if (moved > 0) fail (juce::String (moved) + " of " + juce::String (checked)
                         + " controlBounds() moved on a panel that fits");
    std::printf ("   scale %.6f (fit %.6f, sharp cap %.6f), %d controls, controlBounds unchanged: %s\n",
                 panel.drawScale(), kFit, panel.sharpCeiling(), checked, moved == 0 ? "yes" : "NO");
}

void checkTooTall (Probe& pr)
{
    auto& panel = pr.panel;
    const int w = 1380;
    const int full = panel.heightForWidth (w);
    const int band = full * 55 / 100;          // the HR-16 case: a band, not the panel's aspect
    const float before = panel.drawScale();
    panel.setBounds (0, 0, w, band);
    std::printf ("2. too tall: band %d px for a %d px panel\n", band, full);

    if (panel.drawScale() != before)
        fail ("the panel shrank to fit the height instead of scrolling");
    if (! panel.scrollsVertically()) fail ("a panel taller than its band does not scroll");
    if (panel.scrollsHorizontally()) fail ("a squeezed height started a horizontal scroll");
    const int want = panel.contentSize().y - band;
    panel.setViewOffset (0, 99999);
    if (panel.viewOffset().y != want)
        fail ("vertical travel is " + juce::String (panel.viewOffset().y) + ", expected " + juce::String (want));
    std::printf ("   scale held at %.6f, %d px of panel below the band\n", panel.drawScale(), want);
    panel.setViewOffset (0, 0);
}

void checkFloor (Probe& pr)
{
    auto& panel = pr.panel;
    // The megasynth case: the window cannot get wider, so the panel would otherwise
    // scale its legends away. 15 design units is the HR-16's and the megasynth's own
    // labelSize; 13 px is the floor this component recommends.
    constexpr float legend = 15.0f, floorPx = 13.0f;
    const int w = 620;
    panel.setBounds (0, 0, w, panel.heightForWidth (w));
    const float unfloored = panel.drawScale();
    panel.setLegibilityFloor (legend, floorPx);
    std::printf ("3. too small to read: %d px wide, %d px design\n", w, panel.designWidth());

    const float want = floorPx / legend;
    if (unfloored >= want)
        fail ("the probe's width does not actually fall below the floor - it proves nothing");
    if (std::abs (panel.drawScale() - want) > 1.0e-6f)
        fail ("scale " + juce::String (panel.drawScale(), 8) + " is not the floor " + juce::String (want, 8));
    const float onScreen = legend * panel.drawScale();
    if (std::abs (onScreen - floorPx) > 0.05f)
        fail ("the legend lands at " + juce::String (onScreen, 2) + " px, not " + juce::String (floorPx, 2));
    if (! panel.scrollsHorizontally()) fail ("the floor did not produce a horizontal scroll");
    std::printf ("   %.2f px legend without the floor, %.2f px with it; %d px of panel off to the right\n",
                 legend * unfloored, onScreen, panel.contentSize().x - w);
}

void checkClickFollowsPaint (Probe& pr, bool expectFailure)
{
    auto& panel = pr.panel;
    // Both axes scrolling at once, which is the case that catches an offset put back
    // on one axis only. The floor from check 3 is still in force, so the panel is
    // wider AND taller than this rect.
    const int w = 620;
    panel.setBounds (0, 0, w, panel.heightForWidth (w));
    panel.setViewOffset (0, 0);
    pr.atRest = pr.reached();
    const auto& atRest = pr.atRest;
    std::printf ("4. the click follows the paint: %d controls, %d x %d of panel to travel\n",
                 (int) atRest.size(), panel.contentSize().x - panel.getWidth(),
                 panel.contentSize().y - panel.getHeight());

    const int mx = panel.contentSize().x - panel.getWidth();
    const int my = panel.contentSize().y - panel.getHeight();
    const std::vector<juce::Point<int>> offsets {
        { 0, 0 }, { mx / 3, 0 }, { 0, my / 3 }, { mx / 2, my / 2 }, { mx, my }
    };

    int worst = 0;
    juce::String firstBad;
    for (const auto& off : offsets)
    {
        panel.setViewOffset (off.x, off.y);
        if (panel.viewOffset() != off)
        { fail ("offset " + off.toString() + " did not stick"); continue; }
        const auto now = pr.reached();
        int bad = 0;
        for (const auto& [id, was] : atRest)
        {
            const auto it = now.find (id);
            const std::string got = it == now.end() ? std::string() : it->second;
            if (got == was) continue;
            ++bad;
            if (firstBad.isEmpty())
                firstBad = juce::String (id) + " reached \"" + juce::String (got)
                           + "\" at offset " + off.toString() + ", \"" + juce::String (was) + "\" at rest";
        }
        std::printf ("   offset %-11s %d of %d controls reach a different control\n",
                     off.toString().toRawUTF8(), bad, (int) atRest.size());
        worst = juce::jmax (worst, bad);
    }
    if (worst > 0 && ! expectFailure)
        fail ("hit testing does not follow the viewport: " + firstBad);
    if (worst == 0 && expectFailure)
        fail ("hit testing was made to ignore the viewport and this probe did not notice");
    panel.setViewOffset (0, 0);
}

void checkRealEvent (Probe& pr, bool expectFailure)
{
    auto& panel = pr.panel;
    const int mx = panel.contentSize().x - panel.getWidth();
    const int my = panel.contentSize().y - panel.getHeight();
    // Sweep the whole travel: at any one offset most of the panel is off screen, and
    // a check that only ever fires on the handful of controls in the middle is a check
    // that mostly does not run.
    const std::vector<juce::Point<int>> offsets {
        { 0, 0 }, { mx / 2, 0 }, { mx, 0 }, { 0, my / 2 }, { mx / 2, my / 2 },
        { mx, my / 2 }, { 0, my }, { mx / 2, my }, { mx, my } };

    auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto ev = [&] (juce::Point<float> at, juce::Point<float> from)
    {
        return juce::MouseEvent (source, at, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                 &panel, &panel, juce::Time::getCurrentTime(), from,
                                 juce::Time::getCurrentTime(), 1, at != from);
    };

    int sent = 0, wrong = 0, underChrome = 0;
    juce::String firstBad;
    for (const auto& off : offsets)
    {
    panel.setViewOffset (off.x, off.y);
    for (const auto& p : panel.placements())
    {
        if (p.id.isEmpty() || p.kind == "screen" || p.kind == "static") continue;
        const auto onScreen = panel.controlBounds (p.id).toFloat();
        if (onScreen.isEmpty()) continue;
        const auto pt = onScreen.getCentre();
        if (! panel.getLocalBounds().toFloat().contains (pt)) continue;   // scrolled off screen
        // The bars are drawn over the panel and take a click first, by design. That is
        // the chrome answering, not a hit-test fault, so those points are not this
        // check's business.
        if (panel.isOnScrollBar (pt)) { ++underChrome; continue; }
        // The expectation is what this control resolved to AT REST, not a live reading:
        // a live one would go through the same conversion the defect is injected into
        // and agree with itself no matter how wrong it was. The control drawn at that
        // pixel may legitimately be overlapped by another, so the claim is that
        // scrolling does not change the answer - not that the answer names this control.
        const auto at = pr.atRest.find (p.id.toStdString());
        if (at == pr.atRest.end() || at->second.empty()) continue;
        const juce::String expected (at->second);

        // A knob or a fader says nothing until it MOVES - that is correct, and it is
        // why a press alone would leave most of the panel unchecked. Park it mid-travel
        // first, or a control already at its stop is turned into its own end and emits
        // nothing, which reads exactly like a dead hit box.
        const bool turns = p.kind == "knob" || p.kind == "fader" || p.kind == "readout";
        if (turns) panel.setValue (p.id, 0.5f);
        pr.lastId = {};
        panel.mouseDown (ev (pt, pt));
        if (turns) panel.mouseDrag (ev (pt - juce::Point<float> (0.0f, 40.0f), pt));
        panel.mouseUp (ev (turns ? pt - juce::Point<float> (0.0f, 40.0f) : pt, pt));
        ++sent;
        if (pr.lastId != expected)
        {
            ++wrong;
            if (firstBad.isEmpty())
                firstBad = "a " + juce::String (turns ? "turn" : "press") + " at " + pt.toString()
                           + " on \"" + p.id + "\" emitted \"" + pr.lastId + "\", expected \""
                           + expected + "\" at offset " + off.toString();
        }
    }
    }
    std::printf ("5. a real gesture: %d dispatched at the scrolled pixel, %d reached the wrong "
                 "control (%d skipped under the scroll chrome)\n", sent, wrong, underChrome);
    if (sent == 0) fail ("no control was reachable on screen - check 5 proved nothing");
    if (wrong > 0 && ! expectFailure) fail (firstBad);
    if (wrong == 0 && sent > 0 && expectFailure)
        fail ("a real gesture was made to ignore the viewport and this probe did not notice");
    panel.setViewOffset (0, 0);
}

/** THE PICTURE. Everything above is arithmetic, and arithmetic cannot see a bar drawn
    off the edge, a shaded cue that never appears, or a panel torn where the offset is
    applied twice. Three states down the page: the panel fitting its rect, the same panel
    too small for it and parked at the top left, and parked at the far corner. */
void writePng (Probe& pr, const juce::File& out)
{
    auto& panel = pr.panel;
    const int w = 620;
    struct Shot { const char* what; int w, h; float legend, floorPx; juce::Point<int> at; };
    const int fitW = 1380;
    const std::vector<Shot> shots {
        { "fits: no floor, no bars",              fitW, 0, 0.0f,  0.0f, { 0, 0 } },
        { "does not fit: parked at the origin",   w,    0, 15.0f, 13.0f, { 0, 0 } },
        { "does not fit: parked at the far corner", w,  0, 15.0f, 13.0f, { 99999, 99999 } },
    };

    std::vector<juce::Image> frames;
    int totalH = 0, maxW = 0;
    for (const auto& sh : shots)
    {
        panel.setLegibilityFloor (sh.legend, sh.floorPx);
        panel.setBounds (0, 0, sh.w, panel.heightForWidth (sh.w));
        panel.setViewOffset (sh.at.x, sh.at.y);
        juce::Image img (juce::Image::ARGB, panel.getWidth(), panel.getHeight(), true);
        { juce::Graphics g (img); panel.paintEntireComponent (g, true); }
        frames.push_back (img);
        totalH += img.getHeight() + 26;
        maxW = juce::jmax (maxW, img.getWidth());
    }

    juce::Image sheet (juce::Image::ARGB, maxW, totalH, true);
    {
        juce::Graphics g (sheet);
        g.fillAll (juce::Colour (0xff101318));
        int y = 0;
        for (size_t i = 0; i < frames.size(); ++i)
        {
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
            g.drawText (shots[i].what, 6, y + 3, maxW - 12, 20, juce::Justification::centredLeft);
            g.drawImageAt (frames[i], 0, y + 24);
            y += frames[i].getHeight() + 26;
        }
    }
    out.getParentDirectory().createDirectory();
    out.deleteFile();
    { juce::FileOutputStream fs (out); juce::PNGImageFormat().writeImageToStream (sheet, fs); }
    std::printf ("wrote %s  %dx%d\n", out.getFullPathName().toRawUTF8(),
                 sheet.getWidth(), sheet.getHeight());
    panel.setLegibilityFloor (0.0f, 0.0f);
}

} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const bool selftest = argc > 1 && std::strcmp (argv[1], "--selftest") == 0;

    Probe pr;
    if (argc > 2 && std::strcmp (argv[1], "--png") == 0)
    { writePng (pr, juce::File::getCurrentWorkingDirectory().getChildFile (argv[2])); return 0; }
    std::printf ("panel \"%s\" %dx%d, %d controls%s\n\n",
                 pr.panel.title().toRawUTF8(), pr.panel.designWidth(), pr.panel.designHeight(),
                 (int) pr.panel.placements().size(),
                 selftest ? "   [SELFTEST: hit testing has been made to ignore the viewport]" : "");
    if (pr.panel.placements().empty())
    { std::printf ("FAIL the panel parsed no controls at all\n"); return 1; }

    checkFits (pr);
    checkTooTall (pr);
    checkFloor (pr);
    // ONLY THE CHECKS AFTER THE INJECTION CAN SEE THE DEFECT. Counting the ones
    // before it means an unrelated red check is reported as a blind probe, which is
    // exactly what happened here: checkFits went red on Retina and the selftest
    // announced that it could not see a fault it had never been given.
    const int beforeInjection = failures;
    if (selftest) pr.panel.devIgnoreViewportForHits (true);
    checkClickFollowsPaint (pr, selftest);
    checkRealEvent (pr, selftest);

    if (selftest)
    {
        if (beforeInjection > 0)
            std::printf ("\nSELFTEST INCONCLUSIVE - %d check(s) failed BEFORE the defect was injected,"
                         " so this run says nothing about whether the probe can see it\n", beforeInjection);
        else
            std::printf ("\n%s\n", failures == 0
                         ? "the probe noticed the injected defect, as it must"
                         : "SELFTEST FAILED - this probe cannot see the fault it exists to catch");
    }
    else
        std::printf ("\n%s\n", failures == 0 ? "the viewport holds" : "VIEWPORT PROBE FAILED");
    return failures == 0 ? 0 : 1;
}
