#include "NativePanel.h"
#include <algorithm>
#include <cstdio>

namespace synthkit {

// ---------------------------------------------------------------- pop-out --
/** THE LARGER CANVAS. [MAN] Serum 2 guide p. 194: "Click the button to access
    a dedicated LFO Editor featuring a larger canvas. You can use the same range
    of tools in the LFO Editor, with the advantage of being able to make finer
    adjustments." Ours is an overlay INSIDE the panel, not a window - supersynth
    shipped a DocumentWindow for its unit panels and got a window behind the
    main one and a dead QWERTY piano for it (the shell's FxOverlay note). The
    overlay covers the panel, dims it, and draws the SAME control - a copy of
    the Ctl with its plot moved to the big rect - through drawPathEditor and the
    same gesture functions, so nothing the small glass can do is missing here
    and nothing here can drift from it. Click outside the canvas or press
    Escape to close. */
struct NativePanel::PathPopout : public juce::Component
{
    PathPopout (NativePanel& p, const Ctl& src) : owner (p), big (src)
    {
        setWantsKeyboardFocus (true);
        setInterceptsMouseClicks (true, false);
    }
    void layout()
    {
        setBounds (owner.getLocalBounds());
        const float k = owner.scale();
        // The canvas: the panel less a margin, in design units, so drawPathEditor
        // paints it at plot * k exactly where this overlay's pixels are.
        const auto r = owner.getLocalBounds().toFloat().reduced (juce::jmin (60.0f, owner.getWidth() * 0.06f),
                                                                  juce::jmin (48.0f, owner.getHeight() * 0.08f));
        canvas = r;
        big.plot = { r.getX() / k, r.getY() / k, r.getWidth() / k, r.getHeight() / k };
        big.rect = big.plot; big.x = big.plot.getX(); big.y = big.plot.getY();
        big.w = big.plot.getWidth(); big.h = big.plot.getHeight();
        const float grow = juce::jlimit (1.0f, 2.2f, big.plot.getWidth() / juce::jmax (1.0f, srcPlotW));
        big.pointR = srcPointR * grow; big.handleR = srcHandleR * grow; big.lineWidth = srcLineW * juce::jmin (grow, 1.5f);
    }
    void sync()
    {
        // The small glass is the source of truth for everything but geometry:
        // mode, grid, cursor and trail follow it every frame.
        if (const auto* c = owner.pathCtl (big.id))
        {
            big.pathMode = c->pathMode; big.gridH = c->gridH; big.gridV = c->gridV;
            big.cursorOn = c->cursorOn; big.cursorX = c->cursorX; big.cursorY = c->cursorY;
            big.trail = c->trail;
        }
    }
    void paint (juce::Graphics& g) override
    {
        sync();
        g.fillAll (juce::Colours::black.withAlpha (0.72f));
        const float k = owner.scale();
        const auto plate = canvas.expanded (10.0f);
        g.setColour (juce::Colour (0xff1c1a2a));
        g.fillRoundedRectangle (plate, 8.0f);
        g.setColour (big.edge.withAlpha (0.6f));
        g.drawRoundedRectangle (plate, 8.0f, 1.5f);
        g.setColour (big.edge.withAlpha (0.9f));
        g.setFont (juce::Font (juce::FontOptions (12.0f * juce::jmax (1.0f, k))));
        g.drawText (big.id.upToFirstOccurrenceOf ("_", false, false).toUpperCase() + "  EDITOR    click outside or Esc to close",
                    plate.withHeight (22.0f).translated (12.0f, -26.0f), juce::Justification::centredLeft);
        owner.drawPathEditor (g, big, k);
    }
    juce::Point<float> design (const juce::MouseEvent& e) const { return owner.toDesign (e.position); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! canvas.contains (e.position)) { owner.closePathPopout(); return; }
        if (owner.pathSource) owner.pathBeginGesture (big, design (e), e.mods);
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (owner.pathSource) owner.pathDragGesture (big, design (e), e.mods);
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { owner.pathEndGesture(); repaint(); }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (canvas.contains (e.position)) owner.pathDoubleClick (big, design (e));
        repaint();
    }
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey) { owner.closePathPopout(); return true; }
        return false;
    }
    NativePanel& owner;
    Ctl big;
    juce::Rectangle<float> canvas;
    float srcPlotW = 0.0f, srcPointR = 4.0f, srcHandleR = 3.0f, srcLineW = 2.0f;
};

void NativePanel::openPathPopout (const juce::String& id)
{
    const auto* c = pathCtl (id);
    if (c == nullptr) return;
    closePathPopout();
    popout = std::make_unique<PathPopout> (*this, *c);
    popout->srcPlotW = c->plot.getWidth(); popout->srcPointR = c->pointR;
    popout->srcHandleR = c->handleR; popout->srcLineW = c->lineWidth;
    addAndMakeVisible (*popout);
    popout->layout();
    popout->toFront (true);
    popout->grabKeyboardFocus();
}

void NativePanel::closePathPopout()
{
    if (popout == nullptr) return;
    pathEndGesture();
    removeChildComponent (popout.get());
    popout.reset();
    repaint();
}

bool NativePanel::pathPopoutOpen() const noexcept { return popout != nullptr; }

NativePanel::~NativePanel() { closePathPopout(); }

namespace
{
    /** JUCE's binary-data mangler is not a plain "non-alphanumeric becomes underscore": it
        DROPS some separators (a hyphen disappears, so "text-inside" becomes "textinside")
        while turning others into underscores. So try the obvious name first and then compare
        both sides with every non-alphanumeric character removed, which matches whatever it
        chose. */
    juce::String resourceName (const juce::String& file)
    {
        juce::String s;
        for (auto c : file)
            s << (juce::CharacterFunctions::isLetterOrDigit (c) ? juce::String::charToString (c)
                                                                : juce::String ("_"));
        return s;
    }

    juce::String squashed (const juce::String& s)
    {
        juce::String o;
        for (auto c : s) if (juce::CharacterFunctions::isLetterOrDigit (c)) o << juce::String::charToString (c);
        return o.toLowerCase();
    }

    /** HOW TALL AN LCD DIGIT IS, as a fraction of its window's height.

        Measured on megasynth_design.webp, the screenshot the steel theme is
        sampled from: the digits in the OCT and DET fields span 136 of the field
        strip's 245 image px (0.555) and the letters of SAW in the WAVE A dropdown
        135 of 244 (0.553), so one factor serves the seven- and the fourteen-
        segment face alike. It maps straight onto the font height because DSEG's
        vertical metrics are ascent 1000, descent 0 on a 1000 em and its "8" and
        "~" span the whole em: a DSEG font of height h draws its digits exactly h
        tall, with no leading above or below to allow for. */
    constexpr float kLcdFill = 0.56f;

    /** THE GLOW BEHIND A LIT LCD GLYPH, in design px either side of the stroke. The
        design's digits bloom into the glass by about a pixel and a half; two passes,
        a wide faint one and a narrow brighter one, read as a bloom rather than as a
        bold outline, which a single stroke at one alpha does. */
    constexpr float kLcdGlowWide = 3.0f, kLcdGlowNarrow = 1.2f;

    juce::Point<float> polar (float cx, float cy, float r, float deg)
    {
        const float a = juce::degreesToRadians (deg - 90.0f);
        return { cx + r * std::cos (a), cy + r * std::sin (a) };
    }

    /** A font that is SQUEEZED, never clipped. JUCE would otherwise fit what it can and print
        an ellipsis, which reads as a shorter word rather than as a display that is too small. */
    juce::Font squeezedToFit (juce::Font f, const juce::String& text, float width)
    {
        const float want = juce::GlyphArrangement::getStringWidth (f, text);
        if (want > width && want > 0.0f) f = f.withHorizontalScale (width / want);
        return f;
    }
}

// ---------------------------------------------------------------------------
bool NativePanel::isValueCtl (const Ctl& c) noexcept
{
    // A FIELD IS A VALUE CONTROL. It prints a name and a number side by side
    // instead of a ring and a legend, but it is dragged, it has a range and it
    // has a default - and leaving it off this list is why MegaSynth's SEM A /
    // SEM B / FIN / DET drew their names, showed no number and did not respond:
    // it was not draggable, it got no cursor, and `setReadout` refused it.
    return c.kind == "knob" || c.kind == "fader" || c.kind == "readout"
        || c.kind == "field";
}

juce::Colour NativePanel::colourOf (const juce::var& v, juce::Colour fallback)
{
    const auto s = v.toString();
    if (s.startsWith ("#") && (s.length() == 7 || s.length() == 9))
        return juce::Colour::fromString (s.length() == 7 ? "ff" + s.substring (1) : s.substring (1));
    return fallback;
}

NativePanel::NativePanel (PanelAssets a, const juce::String& prefix)
    : assets (a), machine (prefix)
{
    setOpaque (true);
    // No panel control ever takes keyboard focus (playbook section 18: a BPM
    // slider once silenced a whole synth by stealing it from the keyboard).
    setWantsKeyboardFocus (false);
    setMouseClickGrabsKeyboardFocus (false);
    load();
}

void NativePanel::setMachine (const juce::String& prefix)
{
    if (prefix == machine) return;
    machine = prefix;
    sprites.clear(); ctls.clear(); index.clear(); bandGroups.clear();
    restLines.clear(); editLines.clear(); showingEdit = false;
    background = {}; bgScaled = {}; bgScaledFor = -1;
    dragIdx = hoverIdx = -1;
    // The other machine is a different size, so the window over it starts again at
    // its top left. A kept offset lands somewhere arbitrary on the new panel.
    viewX = viewY = 0; barDrag = 0; panning = false;
    load();
    resized();
    repaint();
}

// --- assets ----------------------------------------------------------------
juce::String NativePanel::asset (const juce::String& file) const
{
    return machine.isEmpty() ? file : machine + "_" + file;
}

const char* NativePanel::blob (const juce::String& file, int& size) const
{
    size = 0;
    if (assets.get == nullptr) return nullptr;
    if (const char* d = assets.get (resourceName (file).toRawUTF8(), size))
        return d;
    const auto want = squashed (file);
    for (int i = 0; i < assets.count; ++i)
        if (squashed (assets.names[i]) == want)
            if (const char* d = assets.get (assets.names[i], size))
                return d;
    size = 0;
    return nullptr;
}

juce::Image NativePanel::loadPng (const juce::String& file) const
{
    int size = 0;
    if (const char* d = blob (file, size))
        return juce::ImageFileFormat::loadFrom (d, (size_t) size);
    DBG ("NativePanel: image not found in binary data: " << file);
    return {};
}

NativePanel::Sprite* NativePanel::spriteFor (const juce::var& v)
{
    if (! v.isObject()) return nullptr;
    const auto name = v.getProperty ("name", "").toString();
    auto it = sprites.find (name);
    if (it == sprites.end())
    {
        Sprite sp;
        sp.img = loadPng (name);
        sp.w = (float) (double) v.getProperty ("w", 0.0);
        sp.h = (float) (double) v.getProperty ("h", 0.0);
        sp.ax = (float) (double) v.getProperty ("ax", 0.0);
        sp.ay = (float) (double) v.getProperty ("ay", 0.0);
        it = sprites.emplace (name, std::move (sp)).first;
    }
    return &it->second;
}

// --- loading ---------------------------------------------------------------
void NativePanel::load()
{
    artwork = false;
    if (loadNative (asset ("layout.json")))
    {
        artwork = true;
        return;
    }
    // The export has not been generated yet. Fall back to the SPEC, which is the
    // same file synthpanel-creator renders, so the unit is playable and every
    // binding is testable on day one. Two commands close the gap and neither of
    // them is a code change:  npm run panel:check && npm run panel:native
    juce::Logger::writeToLog ("NativePanel: no " + asset ("layout.json")
                              + " in the binary data - drawing panel/panel.json as a "
                                "placeholder. Run `npm run panel:native` and rebuild.");
    loadSpec();
}

bool NativePanel::loadNative (const juce::String& layoutFile)
{
    int size = 0;
    const char* data = blob (layoutFile, size);
    if (data == nullptr || size <= 0) return false;

    const auto json = juce::JSON::parse (juce::String::fromUTF8 (data, size));
    auto* arr = json.getProperty ("controls", {}).getArray();
    if (arr == nullptr) return false;

    // THE ANCHORS: jack positions the raster has already drawn and a cable painter
    // needs. Absent on a panel with no jacks, so an older export loads exactly as it
    // always did.
    anchors.clear();
    if (auto* an = json.getProperty ("anchors", {}).getArray())
        for (const auto& v : *an)
            anchors[v.getProperty ("id", {}).toString()] =
                { (float) (double) v.getProperty ("x", 0.0), (float) (double) v.getProperty ("y", 0.0) };

    designW = (int) json.getProperty ("size", {}).getProperty ("width", designW);
    designH = (int) json.getProperty ("size", {}).getProperty ("height", designH);
    // AT WHAT SCALE THE PICTURES WERE RENDERED. It is what says how far this
    // panel can be drawn before a raster has to be invented rather than
    // resampled - see scale().
    assetScale = (float) (double) json.getProperty ("scale", 2.0);
    const auto name = json.getProperty ("name", "").toString();
    if (name.isNotEmpty()) panelTitle = name;

    background = loadPng (asset ("background.png"));
    if (! background.isValid())
        juce::Logger::writeToLog ("NativePanel: " + asset ("background.png")
                                  + " is not embedded - the printed matter will be missing.");

    const auto theme = json.getProperty ("theme", {});
    // The two numbers the legibility floor is made of. `minSize` is the SMALLEST
    // legend the panel prints, in DESIGN units - not `labelSize`, which is only the
    // default one: on a plate that prints 11-unit legends under 20-unit ones,
    // scaling against the 20 puts every 11 off the bottom of what a person can read.
    // An export made before synthpanel-creator carried `minSize` has labelSize only.
    // `minLegendPx` is how few screen pixels of that legend this panel will tolerate
    // before it stops shrinking and starts scrolling. Absent - which is every panel
    // authored before the viewport - there is no floor and nothing changes.
    labelDesign = (float) (double) theme.getProperty ("minSize",
                                     theme.getProperty ("labelSize", 0.0));
    minLegend   = (float) (double) theme.getProperty ("minLegendPx", 0.0);
    const auto accentA = colourOf (theme.getProperty ("accentA", ""), juce::Colours::red);
    const auto accentACore = colourOf (theme.getProperty ("accentACore", ""), juce::Colours::white);
    ink = accentA;
    ground = colourOf (theme.getProperty ("plate", ""), juce::Colour (0xff1b2230));

    // ---- THE PAGE MAP, before the controls, because a control names its page --
    pageList.clear();
    if (auto* pg = json.getProperty ("pages", {}).getArray())
        for (const auto& pv : *pg)
        {
            Page p;
            p.id = pv.getProperty ("id", "").toString();
            p.select = pv.getProperty ("select", "").toString();
            p.view = (int) pv.getProperty ("view", -1);
            p.viewSection = pv.getProperty ("viewSection", "").toString();
            if (auto* r = pv.getProperty ("rect", {}).getArray(); r != nullptr && r->size() == 4)
                p.rect = { (float) (double) (*r)[0], (float) (double) (*r)[1],
                           (float) (double) (*r)[2], (float) (double) (*r)[3] };
            if (auto* nm = pv.getProperty ("names", {}).getArray())
                for (const auto& n : *nm) p.names.add (n.toString());
            if (auto* im = pv.getProperty ("images", {}).getArray())
                for (const auto& i : *im)
                {
                    // PAGE 0 IS THE BASE and its entry is null on purpose, so the
                    // vector's indices line up with the page numbers and nothing
                    // has to remember to add one.
                    const auto file = i.toString();
                    p.images.push_back (file.isEmpty() ? juce::Image() : loadPng (asset (file)));
                    if (file.isNotEmpty() && ! p.images.back().isValid())
                        juce::Logger::writeToLog ("NativePanel: page image " + asset (file)
                                                  + " is not embedded - that page will draw as the first.");
                }
            pageList.push_back (std::move (p));
        }

    // THE STEEL THEME'S WINDOW FIELDS, read the same way for a field and a
    // readout and for a scope and a path editor, because the two pairs are the
    // same display in two sizes and reading them twice is how they drift apart.
    const auto readLcd = [] (Ctl& c, const juce::var& v, const char* defaultFace)
    {
        c.lcd = (bool) v.getProperty ("lcd", false);
        c.lcdInk = colourOf (v.getProperty ("lcdInk", ""), c.text);
        // The unlit segments default to a faint copy of the ink rather than to a
        // grey, so a theme that names only the ink still ghosts in its own colour.
        c.lcdInkDim = colourOf (v.getProperty ("lcdInkDim", ""), c.lcdInk.withAlpha (0.18f));
        c.lcdFont = v.getProperty ("lcdFont", defaultFace).toString();
    };
    const auto readTrace = [] (Ctl& c, const juce::var& v, double defaultWidth)
    {
        // `trace` absent means the line keeps the colour the export always
        // carried in `color`; `traceGlow` absent - a transparent colour - or a
        // zero glow width means no glow pass, which is the plain stroke of before.
        c.trace = colourOf (v.getProperty ("trace", ""), c.text);
        c.traceGlow = colourOf (v.getProperty ("traceGlow", ""), juce::Colour());
        c.traceWidth = (float) (double) v.getProperty ("traceWidth", defaultWidth);
        c.traceGlowWidth = (float) (double) v.getProperty ("traceGlowWidth", 0.0);
    };

    for (const auto& v : *arr)
    {
        Ctl c;
        c.id = v.getProperty ("id", "").toString();
        c.kind = v.getProperty ("kind", "").toString();
        // WHICH BAND A LIST ROW NAMES. The spec loader read this from the day
        // the handle kind landed and THIS one never did, so in every exported
        // panel - which is every shipping plugin - a handle named an empty
        // band: the press searched the band groups for "" and found nothing,
        // and listY0, the map of row positions the drag measures against, was
        // built from the same empty string and stayed empty. The chain list
        // could not be dragged at all, which is what the author reported three times
        // while every reading of the drag code said it was fine. A field that
        // one loader reads and the other does not is invisible until something
        // asks for it.
        c.listOf = v.getProperty ("listOf", "").toString();
        c.label = v.getProperty ("label", "").toString();
        c.group = v.getProperty ("group", "").toString();
        c.x = (float) (double) v.getProperty ("x", 0.0);
        c.y = (float) (double) v.getProperty ("y", 0.0);
        c.w = (float) (double) v.getProperty ("w", 0.0);
        c.h = (float) (double) v.getProperty ("h", 0.0);
        c.page = (int) v.getProperty ("page", -1);
        c.pageSection = v.getProperty ("pageSection", "").toString();
        // AND WHICH VIEW OF THE PLATE, which is a SECOND and independent axis: a
        // control can be on view 2 and on page 1 of a section that lives there,
        // and it is visible only when both agree. Folding the two into one field
        // would mean a paged section could not live anywhere but the first view.
        c.view = (int) v.getProperty ("view", -1);
        c.viewSection = v.getProperty ("viewSection", "").toString();
        // AND WHICH BAND, the third visibility axis: a dynamic well's control is
        // there only while its band is stacked, and reflow has already moved it.
        c.band = v.getProperty ("band", "").toString();
        // A KEY'S NOTE COMES OUT OF THE EXPORT, never out of arithmetic here.
        // synthpanel-creator's keys.js already resolved it - it is the function
        // that decided where the key is - and a second derivation from the note
        // name and the octave is the same sum done twice in two languages, with
        // an octave between the two answers and nothing able to notice.
        c.midi = (int) v.getProperty ("midi", -1);
        c.black = (bool) v.getProperty ("black", false);
        // A CONTROL'S OWN PALETTE, so one section of a plate can be a different
        // instrument. The raster already draws the printed matter with it; this
        // is the other half, because every MOVING part - the knob face, the
        // pointer, the arc, the lamp - is drawn here. A palette one of the two
        // honours is a blue panel with the wrong knobs on it.
        //
        // It only shifts the FALLBACKS. A control that names a colour outright
        // still gets the one it named, so nothing that was explicit changes.
        auto accA = accentA, accACore = accentACore;
        if (auto pal = v.getProperty ("palette", {}); pal.isObject())
        {
            accA     = colourOf (pal.getProperty ("accentA", ""), accA);
            accACore = colourOf (pal.getProperty ("accentACore", ""), accACore);
        }
        // A LADDER METER'S CELLS. EchoFold's IN/OUT meters are six discs per
        // column, each row its own colour, and the unlit discs are printed by
        // the panel itself - so the live control carries only the row count and
        // the lit colours and lights the cells in place. Without this branch the
        // painter drew a continuous gradient bar straight over the ladder.
        if (c.kind == "meter")
        {
            c.rect = { c.x, c.y, c.w, c.h };
            c.text = colourOf (v.getProperty ("color", ""), accentA);
            c.segRows = (int) v.getProperty ("rows", 0);
            c.segFill = (float) (double) v.getProperty ("cellFill", 0.43);
            if (auto* cl = v.getProperty ("cells", {}).getArray())
                for (const auto& n : *cl) c.segColours.add (colourOf (n.toString(), c.text));
        }
        if (c.kind == "pageSelect")
        {
            c.pageSection = v.getProperty ("section", "").toString();
            if (auto* nm = v.getProperty ("labels", {}).getArray())
                for (const auto& n : *nm) c.pageNames.add (n.toString());
            c.rect = { c.x, c.y, c.w, c.h };
            c.textSize = (float) (double) v.getProperty ("size", 12.0);
            c.text = colourOf (v.getProperty ("color", ""), accentA);
        }
        // THE BOX THE LIVE VALUE IS CENTRED IN, when the printed half owns part
        // of the window - a legend at the left, a caret at the right. The kit
        // exports the rect it actually left free rather than the ingredients,
        // because the two halves computing it separately is how a sub dropdown
        // came to read "SUBsquare" and then, once the legend was right, to print
        // its value under the caret.
        // HOW A WINDOW CASES ITS VALUE. EchoFold's chips read "Time", "Note",
        // "Hartz" - Title case, never the list's own lower case and never caps -
        // and which case a design uses is the design's to say, not every unit's
        // to remember when it writes its labels.
        c.textCase = v.getProperty ("textCase", "").toString();
        if (auto* tr = v.getProperty ("textRect", {}).getArray(); tr != nullptr && tr->size() == 4)
            c.textRect = { (float) (double) (*tr)[0], (float) (double) (*tr)[1],
                           (float) (double) (*tr)[2], (float) (double) (*tr)[3] };
        // A KNOB WITH A PRINTED SCALE gets the same box for a different job:
        // the kit printed the two ENDS of the range under the knob and left the
        // middle empty, and this is that middle. The ink is the accent rather
        // than the legend's white, because the value is the one number on the
        // control that changes and the panel it was measured from prints it in
        // amber for exactly that reason.
        if (c.kind == "knob" && ! c.textRect.isEmpty())
        {
            c.textSize = (float) (double) v.getProperty ("valueSize", 9.0);
            c.text = colourOf (v.getProperty ("valueInk", ""), accACore);
        }
        c.r = (float) (double) v.getProperty ("r", 0.0);
        c.value = c.defaultValue = (float) (double) v.getProperty ("value", 0.5);
        c.bipolar = (bool) v.getProperty ("bipolar", false);
        {   // "on"/"active" are sprite OBJECTS for a cap and bools for a lamp; only a real bool is a state
            const auto onV = v.getProperty ("on", false), actV = v.getProperty ("active", false);
            c.invertLit = (bool) v.getProperty ("invertLit", false);
            c.lit = (bool) v.getProperty ("lit", false)
                    || (onV.isBool() && (bool) onV) || (actV.isBool() && (bool) actV);
        }
        c.sprite = spriteFor (v.getProperty ("sprite", {}));
        c.off = spriteFor (v.getProperty ("off", {}));
        c.on  = spriteFor (v.getProperty ("on", {}));
        c.track = spriteFor (v.getProperty ("track", {}));
        c.handle = spriteFor (v.getProperty ("handle", {}));
        c.positions = juce::jmax (2, (int) v.getProperty ("positions", 2));
        c.pos = juce::jlimit (0, c.positions - 1, (int) v.getProperty ("pos", 0));
        if (auto* fr = v.getProperty ("frames", {}).getArray())
            for (const auto& f : *fr) c.frames.push_back (spriteFor (f));
        if (c.frames.empty()) { c.frames.push_back (c.off); c.frames.push_back (c.on); }
        if (c.kind == "toggle") c.lit = c.pos == c.positions - 1;

        if (const auto p = v.getProperty ("pointer", {}); p.isObject())
        {
            c.pointer.style = p.getProperty ("style", "bar").toString();
            c.pointer.r1 = (float) (double) p.getProperty ("r1", 0.0);
            c.pointer.r2 = (float) (double) p.getProperty ("r2", 0.0);
            c.pointer.width = (float) (double) p.getProperty ("width", 2.4);
            c.pointer.dist = (float) (double) p.getProperty ("dist", 0.0);
            c.pointer.w = (float) (double) p.getProperty ("w", 0.0);
            c.pointer.h = (float) (double) p.getProperty ("h", 0.0);
            c.pointer.colour = colourOf (p.getProperty ("color", ""), accACore);
            if (auto* ang = p.getProperty ("angles", {}).getArray(); ang != nullptr && ang->size() == 2)
            { c.pointer.a0 = (float) (double) (*ang)[0]; c.pointer.a1 = (float) (double) (*ang)[1]; }
            // A SWITCH, not a knob: how many positions it has. The pointer then
            // lands ON a printed position instead of between two of them, and a
            // drag moves it one detent at a time. Zero means continuous.
            c.pointer.steps = (int) p.getProperty ("steps", 0);
        }
        if (const auto ac = v.getProperty ("arc", {}); ac.isObject())
        {
            c.arc.on = true;
            c.arc.r = (float) (double) ac.getProperty ("r", 0.0);
            c.arc.width = (float) (double) ac.getProperty ("width", 3.0);
            c.arc.origin = (float) (double) ac.getProperty ("origin", 0.0);
            c.arc.style = ac.getProperty ("style", "").toString();
            c.arc.colour = colourOf (ac.getProperty ("color", ""), accA);
            if (auto* ang = ac.getProperty ("angles", {}).getArray(); ang != nullptr && ang->size() == 2)
            { c.arc.a0 = (float) (double) (*ang)[0]; c.arc.a1 = (float) (double) (*ang)[1]; }
            // THE GLOW IS PART OF WHAT "steel" MEANS, so a steel arc that names no
            // glow still gets the theme's own (6 px at 0.45) rather than a bare line;
            // any other style defaults to none and draws exactly as before.
            const bool steel = c.arc.style == "steel";
            c.arc.glowWidth   = (float) (double) ac.getProperty ("glowWidth",   steel ? 6.0  : 0.0);
            c.arc.glowOpacity = (float) (double) ac.getProperty ("glowOpacity", steel ? 0.45 : 0.0);
            c.arc.glowColour  = colourOf (ac.getProperty ("glowColor", ""), c.arc.colour);
        }
        if (const auto rg = v.getProperty ("ring", {}); rg.isObject())
        {
            c.ring.n = (int) rg.getProperty ("n", 15);
            c.ring.radius = (float) (double) rg.getProperty ("radius", 0.0);
            c.ring.r = (float) (double) rg.getProperty ("r", 3.4);
            c.ring.single = rg.getProperty ("style", "bar").toString() == "single";
            c.ring.bezel = colourOf (rg.getProperty ("bezel", ""), juce::Colours::black);
            c.ring.off = colourOf (rg.getProperty ("off", ""), juce::Colours::darkgrey);
            c.ring.lit = colourOf (rg.getProperty ("lit", ""), accA);
            c.ring.core = colourOf (rg.getProperty ("core", ""), accACore);
        }
        if (auto* tr = v.getProperty ("travel", {}).getArray(); tr != nullptr && tr->size() == 2)
        { c.travel0 = (float) (double) (*tr)[0]; c.travel1 = (float) (double) (*tr)[1]; }
        c.horizontal = (bool) v.getProperty ("horizontal", false);
        c.cross = (float) (double) v.getProperty ("cross", 30.0);

        if (auto* rr = v.getProperty ("rect", {}).getArray(); rr != nullptr && rr->size() == 4)
            c.rect = { (float) (double) (*rr)[0], (float) (double) (*rr)[1],
                       (float) (double) (*rr)[2], (float) (double) (*rr)[3] };
        if (c.kind == "readout")
        {
            c.value_text = v.getProperty ("text", "").toString();
            c.prefix = v.getProperty ("prefix", "").toString();
            c.textSize = (float) (double) v.getProperty ("size", 12.0);
            c.text = colourOf (v.getProperty ("color", ""), accentA);
            // THE LIST THE CARET PROMISES. A readout carrying choices is a
            // DROPDOWN and a click opens them; one carrying none is a window
            // that is pressed. The author: "dropdowns still dont open an actual
            // dropdown menu" - they stepped the parameter instead, which is the
            // same fault the page select had, and the names were only ever in
            // the host's bindings where the panel could not see them.
            if (auto* ch = v.getProperty ("choices", {}).getArray())
                for (const auto& t : *ch) c.choices.add (t.toString());
            c.menuTarget = v.getProperty ("menuTarget", "").toString();
            // A DROPDOWN'S WINDOW IS AN LCD on the steel theme and shows a WORD, so
            // its face defaults to the fourteen-segment one. `lcd` absent is every
            // panel authored before the theme, and the sans is what they get.
            readLcd (c, v, "DSEG14 Classic");
        }
        if (c.kind == "tabStrip")
        {
            // The plate, the dividers and every NAME are printed; only which one
            // is lit is live. A strip is an instance selector - see the note in
            // the kit - so it drives the same page machinery a pageSelect does.
            // A STRIP THAT PAGES A SECTION NAMES IT; ONE THAT DOES NOT, DOESN'T.
            //
            // This assignment used to be unconditional, and it overwrote the
            // `pageSection` read from the layout a hundred lines above. Three
            // strips deliberately ship with no `section` - EchoFold's LFO and
            // ENV selectors and its COMP DEST cell choose an INSTANCE, not a
            // page - so all three ended up with an EMPTY pageSection while
            // keeping their `page`. Three things then went wrong at once:
            //
            //   `visible()` scans pageList for "" , finds nothing, and FAILS
            //   OPEN - so they drew and hit-tested on all nine bay pages, over
            //   the top of every page's header bar;
            //   `setPage("")` matches no section and returns silently - so a
            //   click could never select anything;
            //   `pageOf("")` answers 0 - so the unit reading the selected LFO
            //   got LFO 1 for ever, and the strip could not even look moved.
            //
            // The author, twice: "I still cant select ENV 1, 2, 3, 4 or LFO 1,2,3,4",
            // and "the LFO 1 and ENV 1 tabs/buttons are visible on all pages
            // COMP/SAMPLES etc". One overwrite, all of it.
            if (const auto sec = v.getProperty ("section", "").toString(); sec.isNotEmpty())
                c.pageSection = sec;
            // WHAT IT SELECTS, which may not be where it lives. A strip that
            // names one gets that section registered here with a slot per cell,
            // because nothing else in the layout declares it: it is a selection
            // index, not a set of drawn pages.
            c.selects = v.getProperty ("selects", "").toString();
            // WHERE THIS STRIP'S FIRST CELL SITS IN A SHARED SELECTION. Two
            // strips that pick one thing between them - EchoFold's LFO 1-4 on
            // one row and ENV 1-4 on the next - are eight choices, not two sets
            // of four. With each strip owning its own selection BOTH always
            // showed one lit cell, so an LFO and an ENV looked selected at the
            // same time. The author: "the current design looks like both LFO1 and ENV1
            // is 'selected' which is wrong. Each LFO/ENV is its own complete
            // thing." Sharing the selection and giving the second strip a base
            // of 4 makes the eight cells one choice, and a strip whose range
            // does not hold the selection lights NOTHING.
            c.selectBase = (int) v.getProperty ("selectBase", 0);
            if (c.selects.isNotEmpty())
            {
                // THE SELECTION HOLDS EVERY STRIP THAT SHARES IT, so its slot
                // count is the highest `selectBase + cells` of any of them, not
                // the first strip's cell count. `setPage` wraps modulo that
                // count, so a selection sized to four would have turned the ENV
                // row's slots 4..7 straight back into the LFO row's 0..3 and
                // the two strips would have gone on fighting over one choice.
                // THE CELL COUNT FROM THE LAYOUT, not from `c.pageNames`, which
                // this runs BEFORE: a tabStrip's labels are read further down,
                // so pageNames is still empty here and the selection came out
                // sized 5 instead of 8 (4 + max(1, 0)). setPage wraps modulo
                // that size, so clicking ENV 2 landed on slot 0 and ENV 4 on
                // slot 2 - the two rows still fighting over one choice, by a
                // different route.
                int cellCount = 0;
                if (const auto* ln = v.getProperty ("labels", juce::var()).getArray())
                    cellCount = ln->size();
                if (cellCount <= 0)
                    if (const auto* nm = v.getProperty ("names", juce::var()).getArray())
                        cellCount = nm->size();
                const int need = c.selectBase + juce::jmax (1, juce::jmax (cellCount, c.pageNames.size()));
                bool known = false;
                for (auto& p2 : pageList)
                    if (p2.id == c.selects)
                    {
                        known = true;
                        if ((int) p2.images.size() < need) p2.images.resize ((size_t) need);
                        break;
                    }
                if (! known)
                {
                    Page np; np.id = c.selects; np.current = 0;
                    np.images.resize ((size_t) need);
                    pageList.push_back (np);
                }
            }
            c.textSize = (float) (double) v.getProperty ("size", 10.0);
            c.hasDot = (bool) v.getProperty ("dot", false);
            c.text = colourOf (v.getProperty ("color", ""), accentA);
            c.fillLit = colourOf (v.getProperty ("fillLit", ""), accentA);
            // THE PILL, the steel theme's tab: fully rounded, haloed, no notch. Each
            // ink falls back to the one the older export carried, so a strip that
            // names only `color` and `fillLit` lights exactly as it did.
            c.pill = (bool) v.getProperty ("pill", false);
            // TRUE UNLESS THE LAYOUT SAYS OTHERWISE, so every strip that was
            // draggable before this flag existed still is.
            c.assignable = (bool) v.getProperty ("assign", true);
            c.textLit = colourOf (v.getProperty ("textLit", ""), c.text);
            c.ringLit = colourOf (v.getProperty ("ringLit", ""), c.textLit);
            c.glowLit = colourOf (v.getProperty ("glowLit", ""), c.fillLit);
            c.glowWidth = (float) (double) v.getProperty ("glowWidth", c.pill ? 4.0 : 0.0);
            c.pillRadius = (float) (double) v.getProperty ("radius", 0.0);
            c.ringDim = colourOf (v.getProperty ("ring", ""), c.text);
            if (auto* cs = v.getProperty ("cells", {}).getArray())
                for (const auto& cc : *cs)
                    if (auto* q = cc.getArray(); q != nullptr && q->size() == 4)
                        c.cells.add ({ (float) (double) (*q)[0], (float) (double) (*q)[1],
                                       (float) (double) (*q)[2], (float) (double) (*q)[3] });
            if (auto* ln = v.getProperty ("labels", {}).getArray())
                for (const auto& l : *ln) c.pageNames.add (l.toString());
        }
        if (c.kind == "field")
        {
            c.label = v.getProperty ("label", "").toString();
            c.value_text = v.getProperty ("text", "").toString();
            c.prefix = v.getProperty ("prefix", "").toString();
            c.textSize = (float) (double) v.getProperty ("size", 11.0);
            c.text = colourOf (v.getProperty ("color", ""), accentA);
            // A FIELD SHOWS A NUMBER, so its LCD face defaults to the seven-segment one.
            readLcd (c, v, "DSEG7 Classic");
        }
        if (c.kind == "scope")
        {
            // A DISPLAY THE HOST PAINTS AND THE PANEL ONLY RESERVES. Everything
            // the renderer could draw - bezel, glass, graticule, centre line -
            // is already in the background, so all that is carried here is where
            // it is, what colour its trace is, and which tap feeds it. What
            // `source` MEANS is the machine's business: on megasynth it is an
            // oscillator lane, and nothing in this file knows what a lane is.
            c.style = v.getProperty ("source", "").toString();
            // HOW TO DRAW WHAT COMES BACK. A TRACE is a signal: it swings either
            // side of a centre line and is normalised by its own peak. A CURVE
            // is a function: it stands on the floor of the window and 0 means
            // nothing rather than silence. A filter's response drawn as a trace
            // would be centred on half its own gain, which is not a picture of
            // anything.
            c.value_text = v.getProperty ("mode", "trace").toString();
            c.text = colourOf (v.getProperty ("color", ""), accentA);
            readTrace (c, v, 1.4);
        }
        if (c.kind == "pathEditor")
        {
            // A WINDOW WHOSE GRATICULE IS NOT PRINTED, which makes it the one
            // exception to "everything the renderer could draw is already in the
            // background". Serum 2 user guide p. 192 on GRID: "The visual grid
            // background in the LFO graph changes as you adjust this setting" -
            // the divisions are a parameter, so the kit prints the bezel, the
            // glass and the centre line and leaves the grid to this file.
            c.text = colourOf (v.getProperty ("color", ""), accentA);
            c.pointFill = colourOf (v.getProperty ("pointFill", ""), accentACore);
            c.edge = colourOf (v.getProperty ("grid", ""), juce::Colours::grey);
            c.gridH = juce::jmax (1, (int) v.getProperty ("gridH", 8));
            c.gridV = juce::jmax (1, (int) v.getProperty ("gridV", 8));
            c.pointR = (float) (double) v.getProperty ("pointR", 4.0);
            c.handleR = (float) (double) v.getProperty ("handleR", 3.0);
            c.lineWidth = (float) (double) v.getProperty ("width", 2.0);
            c.gridOpacity = (float) (double) v.getProperty ("gridOpacity", 0.16);
            if (auto* pl = v.getProperty ("plot", {}).getArray(); pl != nullptr && pl->size() == 4)
                c.plot = { (float) (double) (*pl)[0], (float) (double) (*pl)[1],
                           (float) (double) (*pl)[2], (float) (double) (*pl)[3] };
            // An export made before the kit carried `plot` still draws, at the
            // kit's own inset. Losing the window entirely would be a worse
            // answer than agreeing with the old geometry by construction.
            if (c.plot.isEmpty()) c.plot = c.rect.reduced (c.pointR + 1.0f);
            readTrace (c, v, c.lineWidth);
        }
        if (c.kind == "stepGrid")
        {
            // A PATTERN WINDOW, AND ITS DIVISIONS ARE NOT PRINTED EITHER. The
            // column count follows the pattern's LENGTH and its step value and
            // the row count follows its range, all of which the player changes
            // while the plugin runs - so the kit prints the bezel and the glass
            // and leaves the grid to this file, exactly as it does for the LFO
            // graph and for the same reason ([MAN] Serum 2 guide p. 192).
            c.text      = colourOf (v.getProperty ("color", ""), accentA);
            c.cellFill  = colourOf (v.getProperty ("cellFill", ""), accentA);
            c.cellCore  = colourOf (v.getProperty ("cellCore", ""), accentACore);
            c.edge      = colourOf (v.getProperty ("grid", ""), juce::Colours::grey);
            c.gridMajor = colourOf (v.getProperty ("gridMajor", ""), c.edge);
            c.playInk   = colourOf (v.getProperty ("playhead", ""), accentACore);
            c.rowTint   = colourOf (v.getProperty ("rowTint", ""), juce::Colours::black);
            c.rows = juce::jlimit (1, 128, (int) v.getProperty ("rows", 12));
            c.cols = juce::jlimit (1, kMaxGridCells, (int) v.getProperty ("cols", 16));
            c.beat = juce::jmax (0, (int) v.getProperty ("beat", 4));
            c.cellGap = (float) (double) v.getProperty ("gap", 1.0);
            c.gridOpacity = (float) (double) v.getProperty ("gridOpacity", 0.18);
            c.lane = (bool) v.getProperty ("lane", false);
            c.span = (bool) v.getProperty ("span", false);
            c.gridBase = (int) v.getProperty ("firstRow", -1);
            if (auto* pl = v.getProperty ("plot", {}).getArray(); pl != nullptr && pl->size() == 4)
                c.plot = { (float) (double) (*pl)[0], (float) (double) (*pl)[1],
                           (float) (double) (*pl)[2], (float) (double) (*pl)[3] };
            // An export made before the kit carried `plot` still draws, at the
            // kit's own inset, for the reason the path editor's fallback exists.
            if (c.plot.isEmpty()) c.plot = c.rect.reduced (2.0f);
        }
        if (c.kind == "list")
        {
            // A LIST'S ROWS ARE NOT PRINTED EITHER, and for the same reason as a
            // step grid's cells: they are the machine's bank and the selection
            // is the machine's current program, both of which move while the
            // plugin runs. The kit prints the bezel, the glass, the heading and
            // the column dividers - all of which are geometry - and leaves every
            // name to this file.
            c.text     = colourOf (v.getProperty ("color", ""), accentA);
            c.textDim  = colourOf (v.getProperty ("textDim", ""), juce::Colours::grey);
            c.cellFill = colourOf (v.getProperty ("cellFill", ""), accentA);
            c.cellCore = colourOf (v.getProperty ("cellCore", ""), accentACore);
            c.edge     = colourOf (v.getProperty ("grid", ""), juce::Colours::grey);
            c.rows    = juce::jlimit (1, kMaxListRows, (int) v.getProperty ("rows", 8));
            c.columns = juce::jlimit (1, 8, (int) v.getProperty ("columns", 1));
            c.rowH    = (float) (double) v.getProperty ("rowH", 16.0);
            c.headerH = (float) (double) v.getProperty ("headerH", 0.0);
            c.colW    = (float) (double) v.getProperty ("colW", 0.0);
            c.label   = v.getProperty ("header", "").toString();
            c.gridOpacity = (float) (double) v.getProperty ("gridOpacity", 0.3);
            if (auto* pl = v.getProperty ("plot", {}).getArray(); pl != nullptr && pl->size() == 4)
                c.plot = { (float) (double) (*pl)[0], (float) (double) (*pl)[1],
                           (float) (double) (*pl)[2], (float) (double) (*pl)[3] };
            if (c.plot.isEmpty()) c.plot = c.rect.reduced (2.0f);
            if (c.colW <= 0.0f) c.colW = c.plot.getWidth() / (float) juce::jmax (1, c.columns);
        }
        if (c.kind == "handle")
        {
            c.text = colourOf (v.getProperty ("text", ""), accentA);
            c.textSize = (float) (double) v.getProperty ("size", 0.0);
        }
        if (c.kind == "screen")
        {
            c.style = v.getProperty ("style", "glow").toString();
            // 0 means "derive it from the box", which is every screen authored
            // before this and is what drawScreen still does without one.
            c.textSize = (float) (double) v.getProperty ("size", 0.0);
            // THE LINE PITCH, as a multiple of the font size. 0 keeps the even
            // split this drew before; see drawScreen.
            c.lineHeight = (float) (double) v.getProperty ("lineHeight", 0.0);
            if (auto* st = v.getProperty ("stops", {}).getArray(); st != nullptr && st->size() >= 2)
            { c.bg0 = colourOf ((*st)[0][1], juce::Colours::black);
              c.bg1 = colourOf ((*st)[(int) st->size() - 1][1], juce::Colours::black); }
            c.text = colourOf (v.getProperty ("text", ""), accentA);
            c.edge = colourOf (v.getProperty ("edge", ""), juce::Colours::grey);
            if (auto* ln = v.getProperty ("lines", {}).getArray())
                for (const auto& l : *ln) c.lines.add (l.toString());
            // A SCREEN MAY BE A SEGMENT DISPLAY. `lcd: true` on the export gives
            // it the same face and the same ghosted unlit segments a readout has,
            // which is what the PSP 85's big amber time window is: digits on
            // black, not a line of text on glass. Absent, nothing changes.
            readLcd (c, v, "DSEG14 Classic");
        }
        finishCtl (c);
    }
    if (auto* m = mainScreen()) restLines = m->lines;

    // ---- THE BAND MAP: one group per section whose wells are dynamic --------
    // Parsed AFTER the controls, because the reflow below moves member controls
    // that have to exist first.
    bandGroups.clear();
    if (auto* bg = json.getProperty ("bands", {}).getArray())
        for (const auto& gv : *bg)
        {
            BandGroup gp;
            gp.section = gv.getProperty ("section", "").toString();
            gp.view = (int) gv.getProperty ("view", -1);
            gp.viewSection = gv.getProperty ("viewSection", "").toString();
            gp.gap = (float) (double) gv.getProperty ("gap", 8.0);
            if (auto* r = gv.getProperty ("rect", {}).getArray(); r != nullptr && r->size() == 4)
                gp.rect = { (float) (double) (*r)[0], (float) (double) (*r)[1],
                            (float) (double) (*r)[2], (float) (double) (*r)[3] };
            if (auto* bs = gv.getProperty ("bands", {}).getArray())
                for (const auto& bv : *bs)
                {
                    Band b;
                    b.id = bv.getProperty ("id", "").toString();
                    b.gate = bv.getProperty ("gate", "").toString();
                    if (auto* r = bv.getProperty ("rect", {}).getArray(); r != nullptr && r->size() == 4)
                        b.rect = { (float) (double) (*r)[0], (float) (double) (*r)[1],
                                   (float) (double) (*r)[2], (float) (double) (*r)[3] };
                    if (auto* r = bv.getProperty ("imageRect", {}).getArray(); r != nullptr && r->size() == 4)
                        b.imageRect = { (float) (double) (*r)[0], (float) (double) (*r)[1],
                                        (float) (double) (*r)[2], (float) (double) (*r)[3] };
                    if (b.imageRect.isEmpty()) b.imageRect = b.rect.expanded (4.0f);
                    const auto file = bv.getProperty ("image", "").toString();
                    if (file.isNotEmpty())
                    {
                        b.img = loadPng (asset (file));
                        if (! b.img.isValid())
                            juce::Logger::writeToLog ("NativePanel: band image " + asset (file)
                                                      + " is not embedded - that band will draw its controls on bare plate.");
                    }
                    gp.bands.push_back (std::move (b));
                }
            gp.order.resize (gp.bands.size());
            for (size_t i = 0; i < gp.order.size(); ++i) gp.order[i] = (int) i;
            bandGroups.push_back (std::move (gp));
        }
    // The opening state: every gate reads what the layout authored (off, for a
    // rack that ships engaged strips absent), and the unit's first push reflows.
    // THE LIST ROWS: a handle's authored centre is its band's slot in the list.
    listY0.clear(); listDy.clear();
    for (const auto& c : ctls)
        if (c.kind == "handle" && c.listOf.isNotEmpty())
            listY0[c.listOf] = c.rect.getCentreY();
    reflowBands();
    reflowLists();
    return true;
}

/** The SPEC, drawn as plain shapes at its own coordinates. Same ids, same events, same hit
    areas as the export - only the picture is missing, and it says so on its own face. */
void NativePanel::loadSpec()
{
    int size = 0;
    const char* data = blob ("panel.json", size);
    if (data == nullptr || size <= 0)
    {
        juce::Logger::writeToLog ("NativePanel: panel/panel.json is not in the binary data "
                                  "either - there is nothing to draw.");
        return;
    }
    const auto spec = juce::JSON::parse (juce::String::fromUTF8 (data, size));
    designW = (int) spec.getProperty ("size", {}).getProperty ("width", designW);
    designH = (int) spec.getProperty ("size", {}).getProperty ("height", designH);
    const auto t = spec.getProperty ("header", {}).getProperty ("title", "").toString();
    if (t.isNotEmpty()) panelTitle = t;
    const auto theme = spec.getProperty ("theme", {});
    if (theme.isObject())
    {
        // In a SPEC the type sizes are nested under "fonts"; in an export they are
        // flattened into the theme. Same two numbers, two shapes.
        const auto fonts = theme.getProperty ("fonts", {});
        labelDesign = (float) (double) fonts.getProperty ("minSize",
                                         fonts.getProperty ("labelSize", 0.0));
        minLegend   = (float) (double) fonts.getProperty ("minLegendPx", 0.0);
        ink = colourOf (theme.getProperty ("accentA", ""), juce::Colour (0xffe0a34a));
        ground = colourOf (theme.getProperty ("plate", ""), juce::Colour (0xff1b2230));
    }

    auto* sections = spec.getProperty ("sections", {}).getArray();
    if (sections == nullptr) return;
    for (const auto& s : *sections)
    {
        auto* cs = s.getProperty ("controls", {}).getArray();
        if (cs == nullptr) continue;
        for (const auto& v : *cs)
        {
            Ctl c;
            c.id = v.getProperty ("id", "").toString();
            if (c.id.isEmpty()) continue;
            const auto type = v.getProperty ("type", "").toString();
            // The spec's vocabulary, in the export's terms. knobBig is a knob
            // with a bigger radius, not a second kind of thing.
            c.kind = (type == "knob" || type == "knobBig") ? "knob"
                   : (type == "fader" || type == "cap" || type == "selCap" || type == "toggle"
                      || type == "screen" || type == "readout" || type == "led" || type == "button"
                      || type == "handle")
                         ? type : "static";
            c.listOf = v.getProperty ("listOf", "").toString();
            c.label = v.getProperty ("label", "").toString();
            if (c.label.isEmpty()) c.label = v.getProperty ("text", "").toString();
            c.group = v.getProperty ("group", "").toString();
            c.x = (float) (double) v.getProperty ("x", 0.0);
            c.y = (float) (double) v.getProperty ("y", 0.0);
            c.w = (float) (double) v.getProperty ("w", 0.0);
            c.h = (float) (double) v.getProperty ("h", 0.0);
            c.r = (float) (double) v.getProperty ("r", 0.0);
            c.horizontal = (bool) v.getProperty ("horizontal", false);
            c.cross = 22.0f;
            if (auto* rr = v.getProperty ("rect", {}).getArray(); rr != nullptr && rr->size() == 4)
            {
                c.rect = { (float) (double) (*rr)[0], (float) (double) (*rr)[1],
                           (float) (double) (*rr)[2], (float) (double) (*rr)[3] };
                c.x = c.rect.getX(); c.y = c.rect.getY();
                c.w = c.rect.getWidth(); c.h = c.rect.getHeight();
            }
            c.style = v.getProperty ("style", "glow").toString();
            c.text = ink;
            c.bg0 = juce::Colour (0xff0a0d12); c.bg1 = juce::Colour (0xff0a0d12);
            c.edge = ink.withAlpha (0.5f);
            finishCtl (c);
        }
    }
}

/** Where a control is DRAWN and where it is CLICKED, from the same numbers, once.

    The hit area is what it draws plus a small ring of slack and no more: a hit box far
    larger than the thing drawn is how one control swallows the ones beside it and they all
    report as dead. The controls probe asserts exactly this. */
void NativePanel::finishCtl (Ctl& c)
{
    if (c.kind == "screen" || c.kind == "readout" || c.kind == "field"
        || c.kind == "scope" || c.kind == "tabStrip" || c.kind == "pathEditor"
        || c.kind == "stepGrid" || c.kind == "list" || c.kind == "handle")
        c.drawn = c.rect;
    else if (c.kind == "fader")
        c.drawn = c.horizontal ? juce::Rectangle<float> (c.x, c.y - c.cross / 2, c.w, c.cross)
                               : juce::Rectangle<float> (c.x - c.cross / 2, c.y, c.cross, c.h);
    else if (c.kind == "led" && c.w > 0 && c.h > 0)
        c.drawn = { c.x - c.w / 2, c.y - c.h / 2, c.w, c.h };   // a rectangular lamp is centred too
    else if (c.r > 0)
        c.drawn = { c.x - c.r, c.y - c.r, 2 * c.r, 2 * c.r };
    else
        c.drawn = { c.x, c.y, c.w, c.h };

    if (c.kind == "screen" || c.kind == "static" || c.id.isEmpty())
        c.hit = {};                                      // printed matter is not clickable
    else if (c.kind == "knob" || c.kind == "button") c.hit = c.drawn.expanded (8.0f);
    else if (c.kind == "led")                        c.hit = c.drawn.expanded (5.0f);
    else if (c.kind == "readout" || c.kind == "field") c.hit = c.drawn.expanded (3.0f);
    else if (c.kind == "fader")                      c.hit = c.drawn.expanded (2.0f);
    else                                             c.hit = c.drawn;

    if (c.id.isNotEmpty()) index[c.id] = (int) ctls.size();
    ctls.push_back (std::move (c));
}

// --- what is where ---------------------------------------------------------
std::vector<NativePanel::Placement> NativePanel::placements() const
{
    std::vector<Placement> out;
    out.reserve (ctls.size());
    for (const auto& c : ctls)
    {
        // A HIDDEN BAND'S CONTROLS ARE NOT PLACED. A page or a view travels as a
        // field because a checker can bring it up itself (setPage) and then hold
        // the control to "clickable where drawn"; a band's gate is a PARAMETER
        // the machine owns, and the panel flipping it locally to place a control
        // would desync the plate from the engine. So a control whose band is off
        // the plate is off this list too - it is not drawn and not clickable,
        // and reporting authored coordinates for it would be the stale-layout
        // lie this list exists to catch. It reappears, at its RUNTIME position,
        // the moment its band is stacked.
        if (c.band.isNotEmpty())
        {
            bool hidden = false;
            for (const auto& gp : bandGroups)
                for (const auto& b : gp.bands)
                    if (b.id == c.band && ! b.shown) hidden = true;
            if (hidden) continue;
        }
        out.push_back ({ c.id, c.kind, c.pageSection, c.viewSection,
                         c.drawn, c.hit, c.page, c.view });
    }
    return out;
}

juce::String NativePanel::idAt (juce::Point<float> p) const
{
    const int i = hitCtl (p);
    return i < 0 ? juce::String() : ctls[(size_t) i].id;
}

void NativePanel::devMoveHit (const juce::String& id, float dx, float dy)
{
    const auto it = index.find (id); if (it == index.end()) return;
    ctls[(size_t) it->second].hit.translate (dx, dy);
}

bool NativePanel::controlLit (const juce::String& id) const noexcept
{
    const auto it = index.find (id);
    return it != index.end() && ctls[(size_t) it->second].lit;
}

float NativePanel::controlValue (const juce::String& id) const noexcept
{
    const auto it = index.find (id);
    return it != index.end() ? ctls[(size_t) it->second].value : -1.0f;
}

juce::Rectangle<int> NativePanel::controlBounds (const juce::String& id) const
{
    const auto it = index.find (id);
    if (it == index.end()) return {};
    // VIEWPORT COORDINATES. A plugin that draws into this region - an LCD, a scope,
    // a menu anchored on the control the hardware puts it on - has to follow what is
    // on screen; a rectangle that stays put while the panel moves under it is worse
    // than no rectangle, because it still looks right at offset zero.
    return ctls[(size_t) it->second].drawn
             .transformedBy (juce::AffineTransform::scale (scale())
                               .translated ((float) (centringOffset().x - viewX), (float) (centringOffset().y - viewY)))
             .getSmallestIntegerContainer();
}

juce::String NativePanel::labelOf (const juce::String& id) const
{
    const auto it = index.find (id);
    return it == index.end() ? juce::String() : ctls[(size_t) it->second].label.replace ("\n", " ");
}

void NativePanel::debugDump() const
{
    std::fprintf (stderr, "panel \"%s\" %dx%d artwork=%d controls=%d\n",
                  panelTitle.toRawUTF8(), designW, designH, (int) artwork, (int) ctls.size());
    for (const auto& c : ctls)
        std::fprintf (stderr, "ctl %-20s kind=%-8s drawn=%.0f,%.0f %.0fx%.0f hit=%.0f,%.0f %.0fx%.0f\n",
                      c.id.toRawUTF8(), c.kind.toRawUTF8(),
                      c.drawn.getX(), c.drawn.getY(), c.drawn.getWidth(), c.drawn.getHeight(),
                      c.hit.getX(), c.hit.getY(), c.hit.getWidth(), c.hit.getHeight());
}

// --- state -----------------------------------------------------------------
void NativePanel::setValue (const juce::String& id, float v01)
{
    const auto it = index.find (id); if (it == index.end()) return;
    auto& c = ctls[(size_t) it->second];
    const float v = juce::jlimit (0.0f, 1.0f, v01);
    if (c.kind == "toggle" && c.positions > 2)
    {   // a lever has positions, not a continuum
        const int p = juce::roundToInt (v * (float) (c.positions - 1));
        if (p == c.pos) return;
        c.pos = p; c.lit = p == c.positions - 1; c.value = v; repaintCtl (c);
        maybeReflowGate (id);
        return;
    }
    if (std::abs (v - c.value) < 1e-6f) return;
    c.value = v; repaintCtl (c);
    maybeReflowGate (id);
}

/** WHICH KEY IS UNDER A POINT. Backwards through the list, so the BLACKS are
    tested first: they are drawn last because they overlap the whites either side
    of them, and a hit test that ran forwards would answer "white" for every
    click on a black key - the same order, and the same reason, as hitCtl. */
int NativePanel::keyAt (juce::Point<float> design) const noexcept
{
    for (int i = (int) ctls.size() - 1; i >= 0; --i)
    {
        const auto& c = ctls[(size_t) i];
        if (c.kind != "key" || c.midi < 0 || ! visible (c)) continue;
        if (c.drawn.contains (design)) return c.midi;
    }
    return -1;
}

/** HOW HARD THE KEY WAS STRUCK, from where down it the pointer landed.

    [MAN] Serum 2 user guide, the on-screen keyboard: a click near the TIP of a
    key is loud and one near the BASE is quiet, which is the lever a real key is
    - the further from the pivot, the more of the throw. It is not decoration: a
    fixed velocity gives an on-screen keyboard exactly one dynamic, and every
    patch that answers to velocity then has one sound from the panel.

    The floor is not zero. A click on the very top row of a key would be a note
    on at velocity 0, which is a note OFF in MIDI - so the quietest a key can be
    played is kKeyVelocityFloor, and the loudest is 1. [EAR] 0.25, which is the
    softest a bank program still speaks at; what would retire it is a velocity
    curve pushed down from the unit, which is a seam nothing has asked for yet. */
float NativePanel::keyVelocity (const Ctl& c, juce::Point<float> design) const noexcept
{
    static constexpr float kKeyVelocityFloor = 0.25f;
    const float h = juce::jmax (1.0f, c.drawn.getHeight());
    const float down = juce::jlimit (0.0f, 1.0f, (design.y - c.drawn.getY()) / h);
    return kKeyVelocityFloor + (1.0f - kKeyVelocityFloor) * down;
}

/** THE GESTURE, and it is ONE function for the press and for every step of a
    drag - which is what makes a glissando the same thing as a click that has not
    moved yet. Crossing into a new key releases the one being held and plays the
    new one, in that order: a finger dragged along a keybed never holds two keys,
    and releasing first is what keeps a monophonic patch from re-triggering
    backwards. Landing on the key already held does nothing at all, so a drag
    inside one key is not a stutter of note-ons. */
void NativePanel::keyGesture (juce::Point<float> design)
{
    const int note = keyAt (design);
    if (note == noteHeld) return;
    keyRelease();
    if (note < 0) return;
    for (auto& c : ctls)
        if (c.kind == "key" && c.midi == note && visible (c))
        {
            noteHeld = note;
            if (! c.lit) { c.lit = true; repaintCtl (c); }
            if (onNote) onNote (note, keyVelocity (c, design), true);
            return;
        }
}

/** The held note let go, wherever the pointer is. The lamp goes out with it:
    the panel lit the key because it is playing it, so the panel is the one that
    has to put it out - a key left lit by a gesture that ended is the same fault
    as a lamp lit by a refresh over a dead button. */
void NativePanel::keyRelease()
{
    if (noteHeld < 0) return;
    const int note = noteHeld;
    noteHeld = -1;
    for (auto& c : ctls)
        if (c.kind == "key" && c.midi == note && c.lit) { c.lit = false; repaintCtl (c); }
    if (onNote) onNote (note, 0.0f, false);
}

void NativePanel::setKeyLit (int midiNote, bool lit)
{
    for (auto& c : ctls)
        if (c.kind == "key" && c.midi == midiNote && c.lit != lit) { c.lit = lit; repaintCtl (c); }
}

float NativePanel::valueOf (const juce::String& id) const
{
    for (const auto& c : ctls)
        if (c.id == id && isValueCtl (c))
            return c.value;
    return -1.0f;
}

juce::Array<NativePanel::ProbeCtl> NativePanel::probeList() const
{
    juce::Array<ProbeCtl> out;
    for (const auto& c : ctls)
    {
        // The centre a click should land on. A knob and a jack are placed BY their
        // centre; everything with a width is placed by its corner.
        const auto centre = (c.w > 0.0f && c.h > 0.0f)
                          ? juce::Point<float> { c.x + c.w * 0.5f, c.y + c.h * 0.5f }
                          : juce::Point<float> { c.x, c.y };
        out.add ({ c.id, c.kind, centre });
    }
    return out;
}

bool NativePanel::anchorOf (const juce::String& id, juce::Point<float>& out) const
{
    if (const auto it = anchors.find (id); it != anchors.end()) { out = it->second; return true; }
    for (const auto& c : ctls) if (c.id == id) { out = { c.x, c.y }; return true; }
    return false;
}

void NativePanel::setCable (int slot, bool on, juce::Colour colour)
{
    if (slot < 0 || slot >= (int) cables.size()) return;
    if (cables[(size_t) slot].on == on && cables[(size_t) slot].colour == colour) return;
    cables[(size_t) slot] = { on, colour };
    repaint();
}

bool NativePanel::cableDrawable (int slot) const
{
    if (slot < 0 || slot >= (int) cables.size() || ! cables[(size_t) slot].on) return false;
    juce::Point<float> a, b;
    return anchorOf (cableSrcPrefix + juce::String (slot), a)
        && anchorOf (cableDstPrefix + juce::String (slot), b);
}

void NativePanel::paintCables (juce::Graphics& g, float k) const
{
    for (int i = 0; i < (int) cables.size(); ++i)
    {
        if (! cables[(size_t) i].on) continue;
        juce::Point<float> a, b;
        if (! anchorOf (cableSrcPrefix + juce::String (i), a)) continue;
        if (! anchorOf (cableDstPrefix + juce::String (i), b)) continue;
        // A HANG WHOSE DEPTH DIFFERS PER SLOT, so two cables never trace over one
        // another and a fan-out from one source reads as several curves rather than
        // as one thick line.
        const float sag = 18.0f + (float) (i % 4) * 13.0f;
        const float dx = (b.x - a.x) * 0.28f;
        juce::Path pth;
        pth.startNewSubPath (a.x * k, a.y * k);
        pth.cubicTo ((a.x + dx) * k, (a.y + sag) * k,
                     (b.x - dx) * k, (b.y + sag) * k, b.x * k, b.y * k);
        g.setColour (cables[(size_t) i].colour.withAlpha (0.9f));
        g.strokePath (pth, juce::PathStrokeType (2.6f * k, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        // AND THE PLUG IN EACH SOCKET, so the cable reads as inserted rather than as
        // a line that happens to end at a hole.
        for (const auto& pt : { a, b })
        {
            const float r = 5.0f * k;
            g.setColour (cables[(size_t) i].colour);
            g.fillEllipse (pt.x * k - r, pt.y * k - r, 2 * r, 2 * r);
            g.setColour (juce::Colour (0xff0a0c12));
            g.fillEllipse (pt.x * k - r * 0.42f, pt.y * k - r * 0.42f, r * 0.84f, r * 0.84f);
        }
    }
}

void NativePanel::setLit (const juce::String& id, bool lit)
{
    const auto it = index.find (id); if (it == index.end()) return;
    auto& c = ctls[(size_t) it->second];
    // AN INVERTED LAMP LIGHTS WHEN ITS PARAMETER IS ZERO. A bypass is stored as
    // "is it bypassed", but the lamp a player reads is "is it ON" - Serum's
    // enable LED, lit green on a module that is working. Without this the lamp
    // would be lit on exactly the modules that are doing nothing, which is the
    // most confusing thing a panel can do.
    if (c.invertLit) lit = ! lit;
    if (c.lit == lit) return;
    c.lit = lit; repaintCtl (c);
    maybeReflowGate (id);
}

NativePanel::Ctl* NativePanel::mainScreen()
{
    Ctl* best = nullptr;
    for (auto& c : ctls)
        if (c.kind == "screen"
            && (best == nullptr
                || c.rect.getWidth() * c.rect.getHeight() > best->rect.getWidth() * best->rect.getHeight()))
            best = &c;
    return best;
}

juce::String NativePanel::mainScreenId() const
{
    return const_cast<NativePanel*> (this)->mainScreen() != nullptr
             ? const_cast<NativePanel*> (this)->mainScreen()->id : juce::String();
}

void NativePanel::setScreen (const juce::String& id, const juce::String& l1, const juce::String& l2)
{
    const auto it = index.find (id); if (it == index.end()) return;
    auto& c = ctls[(size_t) it->second]; if (c.kind != "screen") return;
    lastWritten = id;
    juce::StringArray lines; lines.add (l1); if (l2.isNotEmpty()) lines.add (l2);
    if (c.lines == lines) return;
    c.lines = lines; repaintCtl (c);
    if (&c == mainScreen()) { restLines = lines; showingEdit = false; stopTimer(); }
}

void NativePanel::setScreen (const juce::String& l1, const juce::String& l2)
{
    juce::StringArray lines; lines.add (l1); if (l2.isNotEmpty()) lines.add (l2);
    setScreenLines (lines);
}

void NativePanel::setScreenText (const juce::String& id, const juce::String& line)
{
    const auto it = index.find (id); if (it == index.end()) return;
    auto& c = ctls[(size_t) it->second]; if (c.kind != "screen") return;
    lastWritten = id;
    if (c.lines.size() == 1 && c.lines[0] == line) return;
    c.lines.clear(); c.lines.add (line);
    if (&c == mainScreen()) restLines = c.lines;
    repaintCtl (c);
}

void NativePanel::setScreenLines (const juce::StringArray& lines)
{
    restLines = lines;
    if (showingEdit) return;
    if (auto* m = mainScreen()) { m->lines = restLines; repaintCtl (*m); }
    if (! artwork) repaint();
}

void NativePanel::showEdit (const juce::String& label, const juce::String& value)
{
    juce::StringArray lines; lines.add (label); lines.add (value);
    showEditLines (lines);
}

void NativePanel::showEditLines (const juce::StringArray& lines)
{
    editLines = lines; showingEdit = true;
    if (auto* m = mainScreen()) { m->lines = editLines; repaintCtl (*m); }
    if (! artwork) repaint();
    startTimer (1500);
}

void NativePanel::setHandleSource (HandleSource s)
{
    handleSource = std::move (s);
    repaintDisplays();
}

void NativePanel::setLcdTypefaces (juce::Typeface::Ptr digits, juce::Typeface::Ptr alphanumeric)
{
    lcdDigits = std::move (digits);
    lcdAlnum = std::move (alphanumeric);
    repaint();
}

void NativePanel::setScopeSource (ScopeSource s)
{
    scopeSource = std::move (s);
    // ONLY RUN A CLOCK IF THERE IS SOMETHING FOR IT TO DO. A machine with no
    // displays never starts one, and a machine that has them stops paying for it
    // the moment its editor goes away.
    const bool any = std::any_of (ctls.begin(), ctls.end(),
                                  [] (const Ctl& c) { return c.kind == "scope"; });
    if (scopeSource && any) refresher.startTimerHz (30);
    else                    refresher.stopTimer();
}

void NativePanel::setPathSource (PathSource s)
{
    pathSource = std::move (s);
    // NO CLOCK. A trace changes without anybody touching a control and needs
    // one; a path changes only when somebody draws on it, and every gesture
    // below repaints what it touched. Starting a 30 Hz timer for a picture that
    // is identical 29 frames out of 30 would be paying the scope's price for
    // none of the scope's reason.
    repaintDisplays();
}

void NativePanel::setPathTool (int tool)
{
    const int t = juce::jlimit ((int) kPathPoint, (int) kPathRampDown, tool);
    if (t == pathToolNow) return;
    pathToolNow = t;
    // THE SELECTION BELONGS TO THE POINT TOOL. Picking up a Ramp pointer and
    // finding three points still highlighted says those points are about to move,
    // and they are not: the next click fills a grid column.
    pathSel.clear();
    repaintDisplays();
}

NativePanel::Ctl* NativePanel::pathCtl (const juce::String& id)
{
    const auto it = index.find (id);
    if (it == index.end()) return nullptr;
    auto& c = ctls[(size_t) it->second];
    return c.kind == "pathEditor" ? &c : nullptr;
}

void NativePanel::setPathGrid (const juce::String& id, int gridH, int gridV)
{
    auto* c = pathCtl (id); if (c == nullptr) return;
    // ZERO MEANS "LEAVE THAT AXIS ALONE", so a unit that offers one spinner does
    // not have to know what the other one was set to in the panel spec.
    if (gridH > 0) c->gridH = juce::jmin (gridH, 64);
    if (gridV > 0) c->gridV = juce::jmin (gridV, 64);
    repaintCtl (*c);
}

juce::Point<int> NativePanel::pathGrid (const juce::String& id) const
{
    const auto it = index.find (id);
    if (it == index.end()) return {};
    const auto& c = ctls[(size_t) it->second];
    return c.kind == "pathEditor" ? juce::Point<int> { c.gridH, c.gridV } : juce::Point<int> {};
}

void NativePanel::setPathMode (const juce::String& id, int mode)
{
    if (auto* c = pathCtl (id))
    {
        const int m = juce::jlimit ((int) kPathTime, (int) kPathTrace, mode);
        if (c->pathMode == m) return;
        c->pathMode = m;
        // A structural change under a gesture or a selection: both are dropped,
        // for the reason the double-click drops pathSel - an index into one
        // reading of the list means nothing in another.
        if (dragPath == id) pathEndGesture();
        if (pathSelOn == id) { pathSel.clear(); pathSelOn = {}; }
        repaintCtl (*c);
    }
}

int NativePanel::pathMode (const juce::String& id) const
{
    const auto it = index.find (id);
    if (it == index.end()) return kPathTime;
    const auto& c = ctls[(size_t) it->second];
    return c.kind == "pathEditor" ? c.pathMode : (int) kPathTime;
}

void NativePanel::setPathCursor (const juce::String& id, float x, float y, bool on)
{
    if (auto* c = pathCtl (id))
    {
        const float nx = juce::jlimit (0.0f, 1.0f, x), ny = juce::jlimit (0.0f, 1.0f, y);
        // Repaint only when the dot has actually moved a pixel's worth, for the
        // reason setModulation does: this is called at display rate.
        const float px = juce::jmax (1.0f, c->plot.getWidth()), py = juce::jmax (1.0f, c->plot.getHeight());
        const bool moved = on != c->cursorOn
                        || std::abs (nx - c->cursorX) * px >= 0.5f
                        || std::abs (ny - c->cursorY) * py >= 0.5f;
        c->cursorOn = on; c->cursorX = nx; c->cursorY = ny;
        if (moved) repaintCtl (*c);
    }
}

void NativePanel::setPathTrace (const juce::String& id, const float* xy, int pairs)
{
    if (auto* c = pathCtl (id))
    {
        const int n = xy != nullptr ? juce::jmax (0, pairs) : 0;
        c->trail.assign (xy, xy + (size_t) n * 2);
        if (c->pathMode == kPathTrace) repaintCtl (*c);
    }
}

void NativePanel::setPathBackdrop (const juce::String& id, const float* v, int n)
{
    if (auto* c = pathCtl (id))
    {
        const int k = v != nullptr ? juce::jmax (0, n) : 0;
        // REPAINT ONLY WHEN IT CHANGED. This arrives at display rate from a
        // running engine, and a window that repaints on every push costs the
        // same whether the audio moved or not.
        if ((int) c->backdrop.size() == k
            && (k == 0 || std::equal (c->backdrop.begin(), c->backdrop.end(), v)))
            return;
        c->backdrop.assign (v, v + (size_t) k);
        repaintCtl (*c);
    }
}

juce::Point<float> NativePanel::figurePoint (const PathPoint& a, const PathPoint& b, float t) noexcept
{
    const float cx = b.x - a.x, cy = b.y - a.y;
    const float mx = (a.x + b.x) * 0.5f, my = (a.y + b.y) * 0.5f;
    const float k = a.curve * kFigureBulge;
    const float px = mx - cy * k, py = my + cx * k;
    const float s = 1.0f - t;
    return { s * s * a.x + 2.0f * s * t * px + t * t * b.x,
             s * s * a.y + 2.0f * s * t * py + t * t * b.y };
}

// --- the tab strip's live layer ----------------------------------------------
namespace
{
    int cellValue (const juce::Array<int>& a, int cell, int fallback)
    {
        return cell >= 0 && cell < a.size() ? a.getReference (cell) : fallback;
    }
    void setCellValue (juce::Array<int>& a, int cell, int v, int fallback)
    {
        if (cell < 0) return;
        while (a.size() <= cell) a.add (fallback);
        a.set (cell, v);
    }
}

void NativePanel::setTabHandles (const juce::String& stripId, int cell, int handles)
{
    const auto it = index.find (stripId);
    if (it == index.end()) return;
    auto& c = ctls[(size_t) it->second];
    if (c.kind != "tabStrip") return;
    const int h = juce::jlimit (1, 2, handles);
    if (cellValue (c.tabHandles, cell, 1) == h) return;
    setCellValue (c.tabHandles, cell, h, 1);
    repaintCtl (c);
}

int NativePanel::tabHandles (const juce::String& stripId, int cell) const
{
    const auto it = index.find (stripId);
    if (it == index.end()) return 1;
    return cellValue (ctls[(size_t) it->second].tabHandles, cell, 1);
}

juce::Rectangle<int> NativePanel::tabHandleBounds (const juce::String& stripId, int cell, int handle) const
{
    const auto it = index.find (stripId);
    if (it == index.end()) return {};
    const auto& c = ctls[(size_t) it->second];
    if (c.kind != "tabStrip") return {};
    const auto r = tabGripRect (c, tabCellRect (c, cell), juce::jlimit (0, 1, handle));
    // The same map controlBounds uses: design to component, less the viewport.
    return r.transformedBy (juce::AffineTransform::scale (scale())
                                .translated ((float) (centringOffset().x - viewX), (float) (centringOffset().y - viewY)))
            .getSmallestIntegerContainer();
}

void NativePanel::setTabCount (const juce::String& stripId, int cell, int count)
{
    const auto it = index.find (stripId);
    if (it == index.end()) return;
    auto& c = ctls[(size_t) it->second];
    if (c.kind != "tabStrip") return;
    const int n = juce::jmax (0, count);
    if (cellValue (c.tabCounts, cell, 0) == n) return;
    setCellValue (c.tabCounts, cell, n, 0);
    repaintCtl (c);
}

int NativePanel::tabCount (const juce::String& stripId, int cell) const
{
    const auto it = index.find (stripId);
    if (it == index.end()) return 0;
    return cellValue (ctls[(size_t) it->second].tabCounts, cell, 0);
}

juce::Rectangle<float> NativePanel::tabCellRect (const Ctl& c, int cell) const
{
    const int n = juce::jmax (1, c.pageNames.size());
    if (cell >= 0 && cell < c.cells.size() && ! c.cells.getReference (cell).isEmpty())
        return c.cells.getReference (cell);
    const float cw = c.rect.getWidth() / (float) n;
    return { c.rect.getX() + cw * (float) juce::jlimit (0, n - 1, cell), c.rect.getY(), cw, c.rect.getHeight() };
}

/** THE KIT'S OWN RING FORMULA, copied per style from synthpanel-creator's
    navy.js (tabStripPill and tabStrip), so the number lands inside the glyph
    the raster printed: a pill cell's ring has radius min(4.5, h * 0.28) at
    right - r - 7, an older strip's min(5, h * 0.3) at right - r - 5, and either
    is drawn only when the cell has room beside its name. */
juce::Point<float> NativePanel::tabRing (const Ctl& c, juce::Rectangle<float> cell, int cellIndex,
                                         float& radius, bool& room) const
{
    const auto name = cellIndex >= 0 && cellIndex < c.pageNames.size() ? c.pageNames[cellIndex] : juce::String();
    // A STRIP THAT CANNOT BE DRAGGED DRAWS NO RING. The ring IS the drag handle,
    // so printing one over a page selector promises a gesture that does nothing.
    if (! c.assignable) { radius = 0.0f; room = false; return {}; }
    if (c.pill)
    {
        radius = juce::jmin (4.5f, cell.getHeight() * 0.28f);
        const float est = (float) name.length() * c.textSize * 0.6f;
        room = ! c.hasDot && cell.getWidth() - est - 16.0f > radius * 2.0f;
        return { cell.getRight() - radius - 7.0f, cell.getCentreY() };
    }
    radius = juce::jmin (5.0f, cell.getHeight() * 0.3f);
    const float est = (float) name.length() * c.textSize * 0.56f;
    room = ! c.hasDot && cell.getWidth() - est - 12.0f > radius * 2.0f;
    return { cell.getRight() - radius - 5.0f, cell.getCentreY() };
}

/** THE X AND Y GRIPS sit between the name and the ring glyph, Y nearest the
    ring, each a small rounded tag as tall as the cell allows. */
juce::Rectangle<float> NativePanel::tabGripRect (const Ctl& c, juce::Rectangle<float> cell, int handle) const
{
    float gr = 0.0f; bool room = false;
    const auto ring = tabRing (c, cell, -1, gr, room);
    const float h = juce::jmax (6.0f, cell.getHeight() - 4.0f);
    const float w = juce::jmax (9.0f, h * 1.1f);
    const float gap = 2.0f;
    const float rightEdge = ring.x - gr - 4.0f;              // clear of the glyph
    const float x = rightEdge - w - (handle == 0 ? w + gap : 0.0f);
    return { x, cell.getCentreY() - h * 0.5f, w, h };
}

int NativePanel::tabHandleAt (const Ctl& c, int cell, juce::Point<float> design) const
{
    if (ignoreTabHandles || cellValue (c.tabHandles, cell, 1) < 2) return 0;
    return tabGripRect (c, tabCellRect (c, cell), 1).contains (design) ? 1 : 0;
}

juce::String NativePanel::tabDragName() const
{
    if (tabPressIdx < 0) return {};
    const auto& c = ctls[(size_t) tabPressIdx];
    auto name = tabPressCell >= 0 && tabPressCell < c.pageNames.size() ? c.pageNames[tabPressCell] : juce::String();
    if (cellValue (c.tabHandles, tabPressCell, 1) >= 2) name << (tabPressHandle == 1 ? " Y" : " X");
    return name;
}

void NativePanel::repaintDisplays()
{
    if (popout != nullptr) popout->repaint();
    const float k = scale();
    for (const auto& c : ctls)
    {
        // A PATH EDITOR IS A DISPLAY TOO. It does not need the 30 Hz clock - a
        // path only changes when somebody draws on it, and the gestures repaint
        // themselves - but it must answer the public call, or a host that pushes
        // a new patch repaints every window on the panel except the one that
        // just changed.
        // A STEP GRID IS A DISPLAY TOO, and it is the one here that really does
        // need the clock: the playhead moves while nobody is touching anything.
        // A LIST IS A DISPLAY TOO. It does not need the clock either - a bank
        // changes when a program is written - but it must answer the public
        // call, or loading a patch repaints every window except the browser the
        // player is looking at.
        if ((c.kind != "scope" && c.kind != "pathEditor" && c.kind != "stepGrid"
             && c.kind != "list")
            || ! visible (c)) continue;
        const auto r = (c.rect.isEmpty() ? juce::Rectangle<float> (c.x, c.y, c.w, c.h) : c.rect)
                           .transformedBy (juce::AffineTransform::scale (k)
                                               .translated ((float) (centringOffset().x - viewX), (float) (centringOffset().y - viewY)));
        repaint (r.getSmallestIntegerContainer());
    }
}

void NativePanel::timerCallback()
{
    stopTimer(); showingEdit = false;
    if (auto* m = mainScreen()) { m->lines = restLines; repaintCtl (*m); }
    if (! artwork) repaint();
}

/** A NUMBER SHORT ENOUGH TO PRINT UNDER A KNOB.

    A parameter's own text is made to be read in a host's automation list, where
    six significant digits and scientific notation are fine. Between the two
    printed ends of a knob's scale there is room for about four characters, and
    what arrived there was `7.15256e-07` - a gain of zero, rendered as the widest
    string in the strip. So the panel shortens what it is given: this is a
    PRESENTATION decision and it belongs to the thing doing the presenting,
    which is also why every unit gets it without writing a formatter.

    Anything that is not a number is passed through untouched - a knob whose
    value is a name ("SINE") is showing the right thing already. */
static juce::String compactValue (const juce::String& t)
{
    const auto s = t.trim();
    if (s.isEmpty()) return s;
    int i = 0;
    if (s[i] == '-' || s[i] == '+') ++i;
    bool digit = false;
    for (; i < s.length(); ++i)
    {
        const auto ch = s[i];
        if (juce::CharacterFunctions::isDigit (ch)) { digit = true; continue; }
        if (ch == '.' || ch == 'e' || ch == 'E') continue;
        if ((ch == '-' || ch == '+') && i > 0 && (s[i - 1] == 'e' || s[i - 1] == 'E')) continue;
        break;
    }
    if (! digit) return s;
    const double v = s.substring (0, i).getDoubleValue();
    const auto rest = s.substring (i).trim();
    const double a = std::abs (v);
    juce::String n;
    if (a >= 10000.0)   n = juce::String (v / 1000.0, a >= 100000.0 ? 0 : 1) + "k";
    else if (a >= 100.0) n = juce::String ((juce::int64) std::llround (v));
    else if (a >= 10.0)  n = juce::String (v, 1);
    else if (a >= 0.1)   n = juce::String (v, 2);
    else if (a > 0.0)    n = juce::String (v, 3);
    else                 n = "0";
    if (n.containsChar ('.'))
    {
        while (n.endsWithChar ('0')) n = n.dropLastCharacters (1);
        if (n.endsWithChar ('.'))    n = n.dropLastCharacters (1);
    }
    return rest.isEmpty() ? n : n + rest;
}

juce::String NativePanel::readoutOf (const juce::String& id) const
{
    for (const auto& c : ctls) if (c.id == id) return c.value_text;
    return {};
}

void NativePanel::setReadout (const juce::String& id, const juce::String& text)
{
    const auto it = index.find (id); if (it == index.end()) return;
    auto& c = ctls[(size_t) it->second];
    // A KNOB TAKES ONE TOO, when the kit left it a box for it. This is what
    // puts the live number between the printed ends of a hardware-style scale,
    // and it costs a unit nothing: an editor already pushes a formatted value
    // for every bound control, so the knobs start reading the moment the panel
    // exports the box.
    if (c.kind != "readout" && c.kind != "field"
        && ! (c.kind == "knob" && ! c.textRect.isEmpty())) return;
    // EVERY NUMBER IS COMPACTED, not only a knob's. A fader or a readout printed
    // whatever the editor formatted, which for a gain sitting at zero was
    // "7.15256e-07" across EchoFold's GAIN faders - the author: "how come it looks
    // complete broken". compactValue leaves anything that does not begin with a
    // digit alone, so a choice label ("Spring", "Wet") is untouched.
    const auto shown = compactValue (text);
    if (c.value_text == shown) return;
    c.value_text = shown; repaintCtl (c);
}

// --- painting --------------------------------------------------------------
bool NativePanel::isInsideAScreen (const Ctl& c) const noexcept
{
    if (c.kind == "screen" || c.drawn.isEmpty()) return false;
    for (const auto& s : ctls)
        if (s.kind == "screen" && ! s.drawn.isEmpty() && s.drawn.contains (c.drawn))
            return true;
    return false;
}

void NativePanel::setMeter (const juce::String& id, float level01)
{
    for (auto& c : ctls)
        // A LADDER ON AN LCD is typed led, because that is what the panel
        // format has for a lit cell, and it is still a meter: the M-ONE's INPUT
        // and DYNAMIC ladders are both drawn inside the display and both move
        // with a level. Matching only kind == meter left every one of them
        // frozen at whatever the export happened to write.
        if (c.id == id && (c.kind == "meter" || (c.kind == "led" && isInsideAScreen (c))))
        {
            const float v = juce::jlimit (0.0f, 1.0f, level01);
            if (std::abs (v - c.value) < 0.004f) return;    // below a pixel: no repaint
            c.value = v;
            repaint (c.drawn.getSmallestIntegerContainer().expanded (2));
            return;
        }
}

void NativePanel::setModulation (const juce::String& id, float offset01, float lo01, float hi01)
{
    const auto it = index.find (id);
    if (it == index.end()) return;
    auto& c = ctls[(size_t) it->second];
    Ctl::Mod m;
    m.on = true;
    m.offset = offset01;
    m.lo = juce::jmin (0.0f, lo01);
    m.hi = juce::jmax (0.0f, hi01);
    // REPAINT ONLY WHAT MOVED. An editor calls this for every destination
    // thirty times a second; a knob whose LFO is at the same place it was, or
    // whose source is an envelope that has settled, must cost nothing. 0.002 of
    // a 270-degree sweep on a 16 px knob is well under a quarter of a pixel.
    constexpr float eps = 0.002f;
    if (c.mod.on && std::abs (c.mod.offset - m.offset) < eps
        && std::abs (c.mod.lo - m.lo) < eps && std::abs (c.mod.hi - m.hi) < eps) return;
    c.mod = m;
    repaintCtl (c);
}

std::array<float, 3> NativePanel::modulationOf (const juce::String& id) const
{
    for (const auto& c : ctls)
        if (c.id == id) return { c.mod.on ? c.mod.offset : 0.0f, c.mod.lo, c.mod.hi };
    return { 0.0f, 0.0f, 0.0f };
}

void NativePanel::clearModulation (const juce::String& id)
{
    const auto it = index.find (id);
    if (it == index.end()) return;
    auto& c = ctls[(size_t) it->second];
    if (! c.mod.on) return;
    c.mod = {};
    repaintCtl (c);
}

juce::Rectangle<float> NativePanel::boundsOf (const Ctl& c) const
{
    // What has to be REPAINTED when this control changes: what it draws, plus room for the
    // label the kit prints beneath it, its indicator ring, and the pointer's overhang.
    if (c.kind == "screen" || c.kind == "readout" || c.kind == "field"
        || c.kind == "tabStrip" || c.kind == "scope"
        || c.kind == "pathEditor" || c.kind == "stepGrid"
        || c.kind == "list") return c.rect.expanded (8.0f);
    if (c.kind == "knob" || c.kind == "button" || c.kind == "led")
    {
        const float R = juce::jmax (c.r, juce::jmax (c.w, c.h) * 0.5f)
                        + (c.ring.n > 0 ? c.ring.radius + 6.0f : 14.0f);
        return juce::Rectangle<float> (c.x - R, c.y - R, 2 * R, 2 * R);
    }
    return c.drawn.expanded (20.0f);
}

void NativePanel::repaintCtl (const Ctl& c)
{
    repaint (boundsOf (c).transformedBy (juce::AffineTransform::scale (scale())
                                           .translated ((float) (centringOffset().x - viewX), (float) (centringOffset().y - viewY)))
                         .getSmallestIntegerContainer().expanded (2));
}

void NativePanel::resized()
{
    if (popout != nullptr) popout->layout();
    bgScaledFor = -1;
    for (auto& [n, sp] : sprites) sp.scaledFor = -1;
    clampView();
}

// --- the viewport ----------------------------------------------------------
namespace {
/** Nobody ever wants a panel drawn four times its design size; this only exists so
    a nonsense floor cannot ask for a two-gigabyte background. */
constexpr float kMaxScale = 4.0f;
/** A parent hands the panel `height = trunc (width * designH / designW)`, so the
    content can come out a pixel taller than the component with nothing wrong at
    all. Below this much overflow there is nothing to scroll and no bar is drawn. */
constexpr int kFitSlack = 2;
}

float NativePanel::scale() const noexcept
{
    const float kFit = designW > 0 ? (float) getWidth() / (float) designW : 1.0f;
    // NO FLOOR DECLARED, NO CHANGE. This is the branch every panel authored before
    // the viewport takes, and it returns precisely what this function always did.
    const float want = (minLegend <= 0.0f || labelDesign <= 0.0f)
                         ? kFit
                         : juce::jmax (kFit, juce::jmin (kMaxScale, minLegend / labelDesign));
    return juce::jmin (want, sharpCeiling());
}

/** THE LARGEST SCALE AT WHICH NOTHING IS INVENTED.
    
    This panel draws PICTURES, and a picture has as many pixels as it was
    rendered with. Downscaling one is a resampler's easy case and looks the way
    the design looks; upscaling one is the resampler guessing, and it reads as
    soft however good the guess. The author, more than once: "why is our UI always
    blurry... why does this never happen on the synth shell?" It never happens
    to the shell because the shell draws with PRIMITIVES, which have no
    resolution to run out of.

    So the panel refuses to be drawn past the point where its own art runs out:
    the export scale over the display's, which at a 3x export on a 2x screen is
    1.5 and at a 2x export on the same screen is 1.0. Past that the window still
    grows and the plate simply stops growing with it. A panel that wants to fill
    a larger window sharply asks its kit for a larger export, which is a number
    in one place rather than a blur nobody can find. */
float NativePanel::sharpCeiling() const noexcept
{
    const float dev = juce::jmax (1.0f, deviceScale());
    return juce::jmax (1.0f, assetScale / dev);
}

int NativePanel::maxUsefulWidth() const noexcept
{
    return juce::jmax (1, juce::roundToInt ((float) designW * sharpCeiling()));
}

int NativePanel::scaleKey() const noexcept { return juce::roundToInt (scale() * 8192.0f); }

juce::Point<int> NativePanel::contentSize() const noexcept
{
    const float k = scale();
    return { juce::roundToInt ((float) designW * k), juce::roundToInt ((float) designH * k) };
}

bool NativePanel::scrollsHorizontally() const noexcept { return contentSize().x - getWidth()  > kFitSlack; }
bool NativePanel::scrollsVertically()   const noexcept { return contentSize().y - getHeight() > kFitSlack; }
int  NativePanel::maxViewX() const noexcept { return scrollsHorizontally() ? contentSize().x - getWidth()  : 0; }
int  NativePanel::maxViewY() const noexcept { return scrollsVertically()   ? contentSize().y - getHeight() : 0; }

int NativePanel::heightForWidth (int w) const noexcept
{
    return designW > 0 ? (int) ((double) w * (double) designH / (double) designW) : 0;
}

juce::Point<float> NativePanel::toDesign (juce::Point<float> p) const noexcept
{
    const float k = scale();
    if (k <= 0.0f) return p;
    // The selftest branch: hit testing that forgets the offset. It is here rather
    // than in the probe because the defect has to happen in the REAL conversion to
    // prove the real conversion is what stops it.
    if (ignoreViewForHits) return p / k;
    // THE SAME OFFSET THE PAINT USES, with its sign the other way round: a
    // plate drawn centred is a plate whose controls are all where the centring
    // put them, and hit testing that forgot it would be the fault this branch
    // exists to catch, in a new place.
    const auto off = centringOffset();
    return (p + juce::Point<float> ((float) (viewX - off.x), (float) (viewY - off.y))) / k;
}

/** WHERE THE PLATE SITS WHEN IT IS SMALLER THAN ITS WINDOW.

    It cannot always fill one: the scale is held at the point its own art runs
    out (sharpCeiling), so a window wider than that gets a plate that has
    stopped growing. Left at the top-left corner that reads as a layout fault;
    CENTRED it reads as a machine on a bench, which is what it is. The offsets
    are NEGATIVE view positions, so the same subtraction every paint already
    does moves the drawing right and down rather than scrolling it. */
juce::Point<int> NativePanel::centringOffset() const noexcept
{
    const auto c = contentSize();
    return { juce::jmax (0, (getWidth()  - c.x) / 2),
             juce::jmax (0, (getHeight() - c.y) / 2) };
}

void NativePanel::setViewOffset (int x, int y)
{
    const int nx = juce::jlimit (0, maxViewX(), x), ny = juce::jlimit (0, maxViewY(), y);
    if (nx == viewX && ny == viewY) return;
    viewX = nx; viewY = ny;
    repaint();
}

void NativePanel::clampView() { setViewOffset (viewX, viewY); }

void NativePanel::scrollToControl (const juce::String& id)
{
    const auto it = index.find (id); if (it == index.end()) return;
    const auto b = ctls[(size_t) it->second].drawn.expanded (12.0f)
                     .transformedBy (juce::AffineTransform::scale (scale()));
    int nx = viewX, ny = viewY;
    if (b.getRight()  > (float) (viewX + getWidth()))  nx = juce::roundToInt (b.getRight()) - getWidth();
    if (b.getX()      < (float) nx)                    nx = juce::roundToInt (b.getX());
    if (b.getBottom() > (float) (viewY + getHeight())) ny = juce::roundToInt (b.getBottom()) - getHeight();
    if (b.getY()      < (float) ny)                    ny = juce::roundToInt (b.getY());
    setViewOffset (nx, ny);
}

void NativePanel::setLegibilityFloor (float legendDesignUnits, float screenPx)
{
    const float d = juce::jmax (0.0f, legendDesignUnits), px = juce::jmax (0.0f, screenPx);
    if (std::abs (px - minLegend) < 1.0e-4f && std::abs (d - labelDesign) < 1.0e-4f) return;
    minLegend = px; labelDesign = d;
    bgScaledFor = -1;
    for (auto& [n, sp] : sprites) sp.scaledFor = -1;
    clampView();
    repaint();
}

void NativePanel::devIgnoreViewportForHits (bool b) { ignoreViewForHits = b; }

bool NativePanel::isOnScrollBar (juce::Point<float> p) const { return overBar (p) != 0; }

float NativePanel::barThickness() const noexcept
{
    return juce::jlimit (7.0f, 13.0f, (float) juce::jmin (getWidth(), getHeight()) * 0.012f);
}

juce::Rectangle<float> NativePanel::barTrack (bool horizontal) const
{
    const float t = barThickness(), pad = 3.0f;
    const float W = (float) getWidth(), H = (float) getHeight();
    if (horizontal)
    {
        if (! scrollsHorizontally()) return {};
        const float endPad = scrollsVertically() ? t + pad * 2.0f : pad;   // do not run under the other bar
        return { pad, H - t - pad, juce::jmax (1.0f, W - pad - endPad), t };
    }
    if (! scrollsVertically()) return {};
    const float endPad = scrollsHorizontally() ? t + pad * 2.0f : pad;
    return { W - t - pad, pad, t, juce::jmax (1.0f, H - pad - endPad) };
}

juce::Rectangle<float> NativePanel::barThumb (bool horizontal) const
{
    const auto track = barTrack (horizontal); if (track.isEmpty()) return {};
    const float visible = (float) (horizontal ? getWidth() : getHeight());
    const float content = (float) (horizontal ? contentSize().x : contentSize().y);
    const float span    = horizontal ? track.getWidth() : track.getHeight();
    // The thumb's LENGTH is the fraction of the panel on screen - that is the part
    // that says how much more there is - with a floor so it stays grabbable on a
    // panel eight lanes wide.
    const float len   = juce::jlimit (juce::jmin (span, 22.0f), span,
                                      span * visible / juce::jmax (1.0f, content));
    const int   maxOff = horizontal ? maxViewX() : maxViewY();
    const float t01   = maxOff > 0 ? (float) (horizontal ? viewX : viewY) / (float) maxOff : 0.0f;
    const float at    = t01 * (span - len);
    return horizontal ? juce::Rectangle<float> (track.getX() + at, track.getY(), len, track.getHeight())
                      : juce::Rectangle<float> (track.getX(), track.getY() + at, track.getWidth(), len);
}

/** THE SCROLL HAS TO BE VISIBLE. An affordance nobody can see is one nobody uses,
    and a panel that silently hides half of itself reads as a broken panel. Two cues,
    because they answer two different questions: the shading along an edge says WHICH
    WAY there is more, and the bar says how much of the panel you are looking at and
    takes you there. Both are drawn only while that axis actually overflows. */
void NativePanel::paintViewportCues (juce::Graphics& g)
{
    const bool sx = scrollsHorizontally(), sy = scrollsVertically();
    if (! sx && ! sy) return;

    const float W = (float) getWidth(), H = (float) getHeight();
    const float fade = juce::jlimit (10.0f, 30.0f, juce::jmin (W, H) * 0.022f);
    // AGAINST THE PLATE, not against black. A black haze on a dark navy panel is a
    // black haze on a dark navy panel: the first version of this drew four of them
    // and not one was visible in the render. Take the colour that contrasts with the
    // panel's own ground and the cue works on a black Nord and a cream Juno alike.
    const auto edgeInk = ground.contrasting (0.85f);
    const auto shade = [&] (juce::Rectangle<float> r, juce::Point<float> from, juce::Point<float> to)
    {
        g.setGradientFill (juce::ColourGradient (edgeInk.withAlpha (0.28f), from,
                                                 edgeInk.withAlpha (0.0f),  to, false));
        g.fillRect (r);
    };
    if (sx && viewX > 0)          shade ({ 0.0f, 0.0f, fade, H }, { 0.0f, 0.0f }, { fade, 0.0f });
    if (sx && viewX < maxViewX()) shade ({ W - fade, 0.0f, fade, H }, { W, 0.0f }, { W - fade, 0.0f });
    if (sy && viewY > 0)          shade ({ 0.0f, 0.0f, W, fade }, { 0.0f, 0.0f }, { 0.0f, fade });
    if (sy && viewY < maxViewY()) shade ({ 0.0f, H - fade, W, fade }, { 0.0f, H }, { 0.0f, H - fade });

    const auto bar = [&] (bool horizontal)
    {
        const auto track = barTrack (horizontal); if (track.isEmpty()) return;
        const auto thumb = barThumb (horizontal);
        const float r = juce::jmin (track.getWidth(), track.getHeight()) * 0.5f;
        g.setColour (juce::Colours::black.withAlpha (0.38f));
        g.fillRoundedRectangle (track, r);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (track, r, 1.0f);
        g.setColour (ink.withAlpha (barDrag == (horizontal ? 1 : 2) ? 0.98f : 0.75f));
        g.fillRoundedRectangle (thumb, r);
    };
    bar (true);
    bar (false);
}

/** HOW MANY DEVICE PIXELS ONE LOGICAL PIXEL IS on the screen this panel is on.

    THE BLUR HAD ONE CAUSE AND THIS IS IT. Every raster on the panel - the
    background, the page images, the sprites - was rescaled to LOGICAL pixels
    (1400 wide at scale 1) and handed to JUCE, which then scaled it back up by
    two for a Retina display. Half the resolution the exporter had rendered was
    thrown away before the screen ever saw it, and the flat Serum theme hid the
    loss because flat edges survive a resample; brushed steel and 10 px type do
    not. The author, holding the app next to the vector render of the same plate: "the
    difference is huge. its blurry."

    So a raster is now rescaled to DEVICE pixels and drawn into its LOGICAL rect,
    which on a 2x display is the exporter's own 2x pixels landing one to one. */
float NativePanel::deviceScale() const noexcept
{
    if (auto* d = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
        return (float) juce::jmax (1.0, d->scale);
    return 1.0f;
}

/** Draw a raster that was scaled for the device into a logical rectangle. */
static void drawRaster (juce::Graphics& g, const juce::Image& im, juce::Rectangle<float> dest)
{
    if (! im.isValid()) return;
    // AT FULL OPACITY, ALWAYS. juce::Graphics::drawImage paints through the
    // CURRENT FILL's alpha, and every caller here is preceded by something that
    // set a translucent colour - so a raster inherited whatever alpha the last
    // brush happened to carry. The band stack showed it plainly: each strip
    // draws its plate and then its grip dots at alpha 0.42, so the FIRST strip
    // came out right and every strip under it came out at 42 per cent over the
    // plate. The author, 2026-09-20: "the TAPE effect seem like it has an opacity
    // ontop or something. it doesnt look like its normal BLUE anymore?? its
    // like faded" and then, exactly: "if any of our other plugins are above
    // TAPE something odd happens to the color of all the FX units below the
    // first unit." Page images and the background share this helper and were
    // one careless setColour away from the same fault.
    g.setOpacity (1.0f);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (im, dest, juce::RectanglePlacement::stretchToFit);
}

const juce::Image& NativePanel::scaledSprite (Sprite& sp, float k)
{
    const int key = scaleKey();
    if (sp.scaledFor != key && sp.img.isValid())
    {
        // AT DEVICE RESOLUTION, not logical - see deviceScale().
        const float dev = deviceScale();
        const int w = juce::jmax (1, juce::roundToInt (sp.w * k * dev));
        const int h = juce::jmax (1, juce::roundToInt (sp.h * k * dev));
        sp.scaled = sp.img.rescaled (w, h, juce::Graphics::highResamplingQuality);
        sp.scaledFor = key;
    }
    return sp.scaled;
}
void NativePanel::paint (juce::Graphics& g)
{
    const float k = scale();
    // A SCROLLING PANEL IS LAID OUT TO A BAND, not to its own aspect, so the parent
    // can give this component more room than the panel fills. An opaque component
    // that leaves pixels unpainted shows whatever was in the buffer.
    const auto content = contentSize();
    if (content.x < getWidth() || content.y < getHeight()) g.fillAll (ground);

    {
        // setOrigin, not an AffineTransform: it moves getClipBounds() with it, so the
        // partial-repaint rejection below keeps working, and the offset is integral so
        // a scrolled sprite still lands on a pixel boundary and stays sharp. At offset
        // zero - which is every panel that fits - this is the identity.
        juce::Graphics::ScopedSaveState ss (g);
        g.setOrigin (-viewX, -viewY);
        paintPanel (g, k);
    }
    paintViewportCues (g);
    // The source's name beside the pointer and the + over a target, in
    // component coordinates: it follows the hand, not the panel under it.
    if (tabDragging) paintModDrag (g);
}

/** IS THIS CONTROL ON THE PAGE ITS SECTION IS SHOWING?

    A control with no page (-1) is always visible, which is every control on a
    panel that does not page and most of one that does. */
bool NativePanel::visible (const Ctl& c) const noexcept
{
    // BOTH AXES HAVE TO AGREE. The view says which plate is showing and the page
    // says which well of a section is; a control on view 2, page 1 is drawn only
    // when view 2 is showing AND its section is on page 1. Checking one and
    // returning would draw every hidden view's controls over the visible one the
    // day a paged section moved off the first view.
    // A CONTROL ON THE OPEN LAYER'S VIEW IS VISIBLE whatever view the plate is
    // showing: the layer draws that view over the plate (paintLayer).
    if (c.view >= 0 && ! onLayer (c))
    {
        bool found = false;
        for (const auto& p : pageList)
            if (p.id == c.viewSection) { if (p.current != c.view) return false; found = true; break; }
        juce::ignoreUnused (found);
    }
    // THE THIRD AXIS: a band's control is there only while its band is stacked.
    // The gate and the overflow rule are reflowBands'; this only reads `shown`.
    if (c.band.isNotEmpty())
        for (const auto& gp : bandGroups)
            for (const auto& b : gp.bands)
                if (b.id == c.band && ! b.shown) return false;
    if (c.page < 0) return true;
    for (const auto& p : pageList)
        if (p.id == c.pageSection) return p.current == c.page;
    // A PAGE WITH NO SECTION TO ASK IS HIDDEN, not shown. This used to fail
    // OPEN, and a control that says "I am on page 3" while naming a section
    // nobody has heard of is a control whose page nothing can satisfy - so
    // showing it on all of them is the one answer that is wrong on every page
    // but its own. Failing closed makes a mis-tagged control DISAPPEAR, which
    // is a fault somebody reports in a minute, rather than have it sit
    // invisibly over eight other pages' controls eating their clicks.
    return false;
}

juce::String NativePanel::valueTextOf (const juce::String& id) const noexcept
{
    for (const auto& c : ctls) if (c.id == id) return c.value_text;
    return {};
}

bool NativePanel::isVisibleCtl (const juce::String& id) const noexcept
{
    for (const auto& c : ctls) if (c.id == id) return visible (c);
    return false;
}

juce::MouseCursor NativePanel::getMouseCursor()
{
    // THIS IS THE ONLY THING THAT DECIDES THE CURSOR. JUCE asks the component
    // for it, and because this override exists every setMouseCursor() call in
    // mouseMove is dead code - which is why a button never showed a pointer
    // however carefully mouseMove set one. The author, twice: "make the cursor at least
    // a pointer when I hover something I can actually click. and a dragger when
    // its a knob that can be moved", then "not a single button shows the cursor
    // pointer". Both halves are answered here rather than in mouseMove.
    const auto p = getMouseXYRelative().toFloat();
    if (overBar (p) != 0) return juce::MouseCursor::PointingHandCursor;
    const int i = hitCtl (toDesign (p));
    if (i >= 0)
    {
        const auto& c = ctls[(size_t) i];
        // A thing you DRAG: a knob, a fader, a readout, a field.
        if (isValueCtl (c))
            return (c.kind == "fader" && c.horizontal)
                     ? juce::MouseCursor::LeftRightResizeCursor
                     : juce::MouseCursor::UpDownResizeCursor;
        // A thing you DRAW ON.
        if (c.kind == "pathEditor" || c.kind == "stepGrid")
            return juce::MouseCursor::CrosshairCursor;
        // A thing you CLICK. Everything with an id that is not a label, a lamp
        // or printed matter: caps, buttons, toggles, keys, selector caps. The
        // test is the hit area, because a control with one is by definition
        // something a click reaches - a lamp or a legend has none.
        // AN INDICATOR IS NOT A CONTROL. led, lamp, meter and readout are things
    // the panel SHOWS; none of them takes a click, so none should offer a hand
    // cursor. The M-ONE put 26 display parts on its panel - the algo grid, the
    // status flags, the input ladders - and every one of them invited a click
    // that does nothing.
    //
    // AND ANYTHING DRAWN INSIDE A SCREEN IS PART OF THAT SCREEN. A machine with
    // a graphic display builds it out of ordinary controls, because that is
    // what the panel format has - the M-ONE's algo grid is nine caps per engine
    // and its status flags are four more - but a cap inside the LCD is a
    // SEGMENT, not a key. It is drawn there, it lights there, and it is never
    // pressed. The rule is geometric and needs no new type: if a control sits
    // inside a screen's rect, it is display.
    const bool indicator = c.kind == "led" || c.kind == "lamp"
                        || c.kind == "meter" || c.kind == "readout"
                        || isInsideAScreen (c);
    if (c.id.isNotEmpty() && ! c.hit.isEmpty() && ! indicator)
            return juce::MouseCursor::PointingHandCursor;
    }
    // Bare panel on a machine bigger than its window is a HANDLE.
    if (scrollsHorizontally() || scrollsVertically())
        return juce::MouseCursor::DraggingHandCursor;
    return juce::MouseCursor::NormalCursor;
}

int NativePanel::choiceCountOf (const juce::String& id) const noexcept
{
    for (const auto& c : ctls) if (c.id == id) return c.choices.size();
    return 0;
}

int NativePanel::pageOf (const juce::String& sectionId) const noexcept
{
    for (const auto& p : pageList) if (p.id == sectionId) return p.current;
    return 0;
}

juce::StringArray NativePanel::pageNames (const juce::String& sectionId) const
{
    // the export's own list first, for the same reason pageNamed reads it: a
    // layer view (openLayer) is a page that is not on the strip
    for (const auto& p : pageList)
        if (p.id == sectionId && ! p.names.isEmpty())
            return p.names;
    for (const auto& c : ctls)
        if (c.pageSection == sectionId && ! c.pageNames.isEmpty())
            return c.pageNames;
    return {};
}

int NativePanel::pageNamed (const juce::String& sectionId, const juce::String& name) const
{
    // THE EXPORT'S OWN LIST FIRST: a page a layer opens (openLayer) is not on
    // the tab strip at all, so the strip's names would answer -1 for it.
    for (const auto& p : pageList)
        if (p.id == sectionId && p.names.contains (name))
            return p.names.indexOf (name);
    for (const auto& c : ctls)
        if (c.pageSection == sectionId && ! c.pageNames.isEmpty())
            return c.pageNames.indexOf (name);
    return -1;
}

void NativePanel::setPage (const juce::String& sectionId, int page)
{
    for (auto& p : pageList)
        if (p.id == sectionId)
        {
            const int n = juce::jmax (1, (int) p.images.size());
            const int want = ((page % n) + n) % n;
            if (want == p.current) return;
            p.current = want;
            if (sectionId == layerSectionId) closeLayer();   // a view switch under a layer is a way out of it
            repaint();
            return;
        }
}

void NativePanel::setPageMark (const juce::String& sectionId, bool anyOffDefault)
{
    auto& m = pageMark[sectionId];
    if (m == anyOffDefault) return;
    m = anyOffDefault;
    repaint();
}

/** The page images, over the base, at the page's own rect.

    THEY ARE RESCALED ONCE PER SCALE AND BLITTED, exactly the way the background
    is, and that is not an optimisation. Drawn with `drawImage` and
    `stretchToFit` the graphics context resamples them on every repaint with its
    own default quality, while the background had already been through
    `rescaled (..., highResamplingQuality)`. Two different resamplers on two
    halves of the same plate is two different-looking typefaces, and that is what
    The author saw: *"I also just noticed that other pages fonts are different than the
    first page."* Same path, same quality, same result.

    AND THE PAGE CUTS RATHER THAN SLIDES. It slid in from the right for 120 ms,
    which sounds better than it looked: page 0 has no image of its own - it IS
    the background - so a move to or from it animated a rectangle of flat ground
    against a picture, and the section appeared to flash empty. The author: *"also when
    using the dropdown to change pages the animation looks very odd."* A cut is
    honest about what is happening, and the thing that made the slide worth
    having - the hit regions switching on the first frame - is now simply true.
*/
void NativePanel::paintPages (juce::Graphics& g, float k)
{
    for (auto& p : pageList)
    {
        if (p.current <= 0 || p.current >= (int) p.images.size()) continue;
        const auto& src = p.images[(size_t) p.current];
        if (! src.isValid()) continue;
        // ONLY ON ITS OWN VIEW. A section's page image is printed legends for
        // that section's open well, and a section on a view that is not showing
        // has nothing showing; the controls already test both axes (visible())
        // and the page image was the one thing that did not. The author: "osc when on
        // another page than MAIN and going to MATRIX the actual section still
        // shows from the osc other page."
        if (p.view >= 0)
        {
            bool hidden = false;
            for (const auto& v : pageList)
                if (v.id == p.viewSection) { hidden = v.current != p.view; break; }
            if (hidden) continue;
        }

        // The destination in whole pixels: a page drawn at a fractional offset is
        // a page whose type is resampled against the background's, which is the
        // other half of why the two looked different.
        const auto r = p.rect.transformedBy (juce::AffineTransform::scale (k)).toNearestInt();
        if (r.isEmpty()) continue;

        if (p.scaledFor != scaleKey() || p.scaledPage != p.current
            || p.scaled.getWidth() != r.getWidth() || p.scaled.getHeight() != r.getHeight())
        {
            const float dev = deviceScale();
            p.scaled = src.rescaled (juce::roundToInt (r.getWidth() * dev), juce::roundToInt (r.getHeight() * dev),
                                     juce::Graphics::highResamplingQuality);
            p.scaledFor = scaleKey();
            p.scaledPage = p.current;
        }
        drawRaster (g, p.scaled, r.toFloat());
    }
}

// ---- dynamic wells (bands) --------------------------------------------------
//
// A band is a well the export marked `showWhen`: it is on the plate only while
// its GATE control reads on, and the visible ones stack from the section's band
// area top in the CURRENT band order (the export's until setBandOrder says
// otherwise) with the export's gap between them. The author: "FXs rack modules should
// only be visible when they are actually active."
//
// THE WHOLE MECHANISM IS THE REFLOW. When a gate changes, each band is given
// its place in the stack and its member controls are moved WHOLE - x stays, y
// shifts by the same delta for the drawn rect, the hit rect, the travel of a
// fader, a strip's cells, a window's plot - so every other function in this
// file (painting, hit-testing, controlBounds, value pushes, repaints) keeps
// working on coordinates that are simply correct, and none of them ever learns
// that bands exist beyond the one `shown` test in visible().

/** ON is "lit, or a value above one half". A lamp cap carries `lit` (already
    through invertLit); a value control pushed with setValue carries the value;
    either one says the gate is engaged. An unknown id reads off, so a stale
    gate name hides its band rather than pinning it on. */
bool NativePanel::gateOn (const juce::String& controlId) const noexcept
{
    const auto it = index.find (controlId);
    if (it == index.end()) return false;
    const auto& c = ctls[(size_t) it->second];
    return c.lit || c.value > 0.5f;
}

void NativePanel::maybeReflowGate (const juce::String& id)
{
    for (const auto& gp : bandGroups)
        for (const auto& b : gp.bands)
            if (b.gate == id) { reflowBands(); return; }
}

/** Move one band's member controls by `d` design px down the plate, WHOLE:
    everything that is expressed in y moves by exactly the same delta, so the
    drawn thing, the clickable thing and the repainted thing stay one thing. */
void NativePanel::shiftBandControls (const juce::String& wellId, float d)
{
    for (auto& c : ctls)
    {
        if (c.band != wellId) continue;
        c.y += d;
        c.rect.translate (0.0f, d);
        c.textRect.translate (0.0f, d);
        c.plot.translate (0.0f, d);
        c.drawn.translate (0.0f, d);
        // THE SELFTEST'S DEFECT, on request: a reflow that moves what is drawn
        // and forgets what is clicked, which is exactly the fault a probe's
        // hit-at-runtime-position check exists to catch.
        if (! bandHitsStale) c.hit.translate (0.0f, d);
        if (! c.horizontal) { c.travel0 += d; c.travel1 += d; }   // a fader's travel is along y
        for (auto& cell : c.cells) cell.translate (0.0f, d);
    }
}

void NativePanel::shiftListControls (const juce::String& wellId, float d)
{
    for (auto& c : ctls)
    {
        if (c.listOf != wellId) continue;
        c.y += d;
        c.rect.translate (0.0f, d);
        c.textRect.translate (0.0f, d);
        c.plot.translate (0.0f, d);
        c.drawn.translate (0.0f, d);
        if (! bandHitsStale) c.hit.translate (0.0f, d);
        if (! c.horizontal) { c.travel0 += d; c.travel1 += d; }
        for (auto& cell : c.cells) cell.translate (0.0f, d);
    }
}

void NativePanel::reflowLists()
{
    // THE SLOTS ARE THE ROWS AS AUTHORED, top to bottom in the export's band
    // order; the band at order position k takes slot k. Rows of bands with no
    // handle are left where they are.
    for (auto& gp : bandGroups)
    {
        std::vector<float> slots;
        for (const auto& b : gp.bands)
        {
            const auto it = listY0.find (b.id);
            if (it != listY0.end()) slots.push_back (it->second);
        }
        if (slots.size() != gp.bands.size()) continue;   // not a listed group
        std::sort (slots.begin(), slots.end());
        for (size_t k = 0; k < gp.order.size(); ++k)
        {
            const auto& id = gp.bands[(size_t) gp.order[k]].id;
            const float want = slots[k] - listY0[id];
            const float have = listDy.count (id) ? listDy[id] : 0.0f;
            if (std::abs (want - have) > 1.0e-3f)
            {
                shiftListControls (id, want - have);
                listDy[id] = want;
            }
        }
    }
}

void NativePanel::drawHandle (juce::Graphics& g, Ctl& c, float k)
{
    // THE GRIP AND THE NAME, live, at wherever the order has put this row. A
    // grip is three short bars, the sign every list uses for "drag me"; the
    // name is the unit's, in the legend face, and the row it is on IS the
    // unit's place in the chain.
    const auto r = c.rect.transformedBy (juce::AffineTransform::scale (k));
    const bool dragging = listDrag && bandDragGroup >= 0
                       && bandGroups[(size_t) bandDragGroup].bands[(size_t) bandDragBand].id == c.listOf;
    const bool hot = dragging || (hoverIdx >= 0 && &ctls[(size_t) hoverIdx] == &c);
    if (dragging)
    {
        g.setColour (c.text.withAlpha (0.10f));
        g.fillRoundedRectangle (r, 3.0f * k);
    }
    const float gx = r.getX() + 4.0f * k, gw = 11.0f * k, cy = r.getCentreY();
    g.setColour (c.text.withAlpha (hot ? 0.95f : 0.55f));
    for (int i = -1; i <= 1; ++i)
        g.fillRoundedRectangle (gx, cy + (float) i * 3.5f * k - 0.75f * k, gw, 1.5f * k, 0.75f * k);
    const float size = (c.textSize > 0.0f ? c.textSize : 10.0f) * k;
    g.setColour (c.text.withAlpha (hot ? 1.0f : 0.85f));
    g.setFont (juce::Font (juce::FontOptions (size, juce::Font::bold)));
    g.drawText (c.label.toUpperCase(), r.withTrimmedLeft (gw + 10.0f * k), juce::Justification::centredLeft, false);
}

void NativePanel::reflowBands()
{
    bool changed = false;
    for (auto& gp : bandGroups)
    {
        // THE STACK'S HEIGHT FIRST, so the scroll can be clamped before a band
        // is placed: a stack that shrank (a gate went off) must not stay
        // scrolled past its own end.
        float total = 0.0f; int shown = 0;
        for (const int bi : gp.order)
        {
            const auto& b = gp.bands[(size_t) bi];
            if (forceBandGates || gateOn (b.gate)) { total += b.rect.getHeight() + gp.gap; ++shown; }
        }
        gp.stackH = shown > 0 ? total - gp.gap : 0.0f;
        const float maxScroll = juce::jmax (0.0f, gp.stackH - gp.rect.getHeight());
        if (gp.scroll > maxScroll || gp.scroll < 0.0f) { gp.scroll = juce::jlimit (0.0f, maxScroll, gp.scroll); changed = true; }
        float at = gp.rect.getY() - gp.scroll;
        for (const int bi : gp.order)
        {
            auto& b = gp.bands[(size_t) bi];
            const bool on = forceBandGates || gateOn (b.gate);
            // EVERY BAND THAT IS ON IS PLACED, in the current order, and the
            // stack SCROLLS when it is taller than the area: the area clips
            // what is placed outside it (paintBands, paintPanel, hitCtl) and
            // the wheel over the area moves the stack. The old rule dropped the
            // bands that did not fit; the author: "we just add a scroll bar in the
            // FXs rack so if it becomes too long the user just scrolls."
            const bool show = on;
            // A hidden band goes back to its AUTHORED coordinates, so a band's
            // offset always means "where the stack put it" and never "where a
            // stack that no longer exists once put it".
            const float want = show ? at - b.rect.getY() : 0.0f;
            if (show) at += b.rect.getHeight() + gp.gap;
            if (show != b.shown) changed = true;
            b.shown = show;
            if (std::abs (want - b.dy) > 1.0e-3f)
            {
                shiftBandControls (b.id, want - b.dy);
                b.dy = want;
                changed = true;
            }
        }
    }
    // A reflow is a gate click or a program change, never a per-frame event, so
    // repainting the plate whole is the honest price of never chasing a stale
    // union of before-and-after rects.
    if (changed) repaint();
}

const NativePanel::BandGroup* NativePanel::groupOfBand (const juce::String& wellId) const noexcept
{
    for (const auto& gp : bandGroups)
        for (const auto& b : gp.bands)
            if (b.id == wellId) return &gp;
    return nullptr;
}

bool NativePanel::bandGroupShowing (const BandGroup& gp) const noexcept
{
    if (gp.view < 0) return true;
    for (const auto& v : pageList)
        if (v.id == gp.viewSection) return v.current == gp.view;
    return true;
}

bool NativePanel::bandShown (const juce::String& wellId) const noexcept
{
    for (const auto& gp : bandGroups)
        for (const auto& b : gp.bands)
            if (b.id == wellId) return b.shown;
    return false;
}

juce::Rectangle<float> NativePanel::bandBounds (const juce::String& wellId) const noexcept
{
    for (const auto& gp : bandGroups)
        for (const auto& b : gp.bands)
            if (b.id == wellId) return b.shown ? b.rect.translated (0.0f, b.dy)
                                               : juce::Rectangle<float>();
    return {};
}

void NativePanel::setBandOrder (const juce::String& sectionId, const juce::StringArray& wellIds)
{
    for (auto& gp : bandGroups)
    {
        if (gp.section != sectionId) continue;
        // The named ids first, in the given order; anything unnamed keeps its
        // export order after them, so a unit that reorders three of eleven
        // strips does not have to spell out the other eight.
        std::vector<int> order;
        for (const auto& id : wellIds)
            for (size_t i = 0; i < gp.bands.size(); ++i)
                if (gp.bands[i].id == id) { order.push_back ((int) i); break; }
        for (size_t i = 0; i < gp.bands.size(); ++i)
            if (std::find (order.begin(), order.end(), (int) i) == order.end())
                order.push_back ((int) i);
        if (order != gp.order)
        {
            gp.order = std::move (order);
            reflowBands();
            reflowLists();
            repaint();
        }
        return;
    }
}

void NativePanel::devForceBandGatesOn (bool b)
{
    forceBandGates = b;
    reflowBands();
}

/** The visible bands' images, over the base, each at wherever the stack put it.
    Rescaled once per scale and blitted, the way the background and the page
    images are, and for the same reason: two resamplers on one plate is two
    different-looking typefaces. */
void NativePanel::paintBands (juce::Graphics& g, float k)
{
    for (auto& gp : bandGroups)
    {
        // Only on its own view, exactly as a page image is.
        if (gp.view >= 0)
        {
            bool hidden = false;
            for (const auto& v : pageList)
                if (v.id == gp.viewSection) { hidden = v.current != gp.view; break; }
            if (hidden) continue;
        }
        juce::Graphics::ScopedSaveState ss (g);
        const auto area = gp.rect.transformedBy (juce::AffineTransform::scale (k));
        g.reduceClipRegion (area.toNearestInt());
        for (const int bi : gp.order)
        {
            auto& b = gp.bands[(size_t) bi];
            if (! b.shown || ! b.img.isValid()) continue;
            const auto r = b.imageRect.translated (0.0f, b.dy)
                             .transformedBy (juce::AffineTransform::scale (k)).toNearestInt();
            if (r.isEmpty()) continue;
            const float dev = deviceScale();
            if (b.scaledFor != scaleKey()
                || b.scaled.getWidth() != juce::roundToInt ((float) r.getWidth() * dev))
            {
                b.scaled = b.img.rescaled (juce::roundToInt ((float) r.getWidth() * dev),
                                           juce::roundToInt ((float) r.getHeight() * dev),
                                           juce::Graphics::highResamplingQuality);
                b.scaledFor = scaleKey();
            }
            drawRaster (g, b.scaled, r.toFloat());
            // THE GRIP, DRAWN. The author, 2026-09-17: "many times when I try to turn a
            // knob the module moves. A module should only be draggable by a
            // dragg handle. Make that on the left side." It used to be the
            // band's whole left 110 design px wherever no control claimed the
            // point, which is an invisible target that overlapped the first
            // knob's approach. Now it is a narrow strip at the left edge with
            // three dots on it - the grip every list with draggable rows has -
            // and it is the ONLY place a drag starts.
            // NOT DRAWN WHEN IT CANNOT BE USED. A unit whose strips are
            // reordered from a list switches this grip off
            // (setBandGripEnabled false), and it was still being painted: a
            // translucent band with three dots down every strip's left edge,
            // which is a handle that answers nothing. The author, 2026-09-21: "why is
            // there that weird opacity overlay strip on the left side?"
            if (! bandGripEnabled) continue;
            const auto grip = juce::Rectangle<float> (r.getX() + 1.0f, r.getY() + 3.0f,
                                                      kBandGripPx * k - 2.0f,
                                                      (float) r.getHeight() - 6.0f);
            const bool lit = bandDragGroup >= 0 && bandDragBand == bi
                          && &gp == &bandGroups[(size_t) bandDragGroup];
            g.setColour (juce::Colours::white.withAlpha (lit ? 0.22f : 0.08f));
            g.fillRoundedRectangle (grip, 2.0f * k);
            g.setColour (juce::Colours::white.withAlpha (lit ? 0.75f : 0.42f));
            const float dot = juce::jmax (1.2f, 1.6f * k);
            for (int d = -1; d <= 1; ++d)
                g.fillEllipse (grip.getCentreX() - dot * 0.5f,
                               grip.getCentreY() - dot * 0.5f + (float) d * dot * 2.6f,
                               dot, dot);
        }
    }
}

void NativePanel::paintPanel (juce::Graphics& g, float k)
{
    if (! artwork)
    {
        paintPlaceholder (g, k);
        return;
    }
    if (bgScaledFor != scaleKey() && background.isValid())
    {
        // Scale UNIFORMLY from the design width. Rescaling to getHeight() stretches the
        // printed matter to a different scale from the sprites drawn on top, and the labels
        // then sit under their own controls.
        const float dev = deviceScale();
        const int ww = juce::jmax (1, juce::roundToInt ((float) designW * k * dev));
        const int hh = juce::jmax (1, juce::roundToInt ((float) designH * k * dev));
        bgScaled = background.rescaled (ww, hh, juce::Graphics::highResamplingQuality);
        bgScaledFor = scaleKey();
    }
    if (bgScaled.isValid())
        drawRaster (g, bgScaled, { 0.0f, 0.0f, (float) designW * k, (float) designH * k });
    else g.fillAll (ground);
    paintPages (g, k);
    paintBands (g, k);
    // CABLES GO UNDER THE CONTROLS. A cable that crosses a chip has to pass BEHIND
    // it, or the patchbay reads as string laid over the panel rather than as leads
    // plugged into it. Drawn after the printed matter and before every moving part.
    paintCables (g, k);

    const auto clip = g.getClipBounds().toFloat();
    for (auto& c : ctls)
    {
        if (! visible (c) || onLayer (c)) continue;   // the layer's own controls are painted by paintLayer, over the scrim
        if (! boundsOf (c).transformedBy (juce::AffineTransform::scale (k)).intersects (clip)) continue;
        if (c.band.isNotEmpty())
        {
            // clipped to the band area, which is what makes a scrolled rack a
            // rack and not a strip drawn over the keyboard
            if (const auto* gp = groupOfBand (c.band); gp != nullptr)
            {
                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (gp->rect.transformedBy (juce::AffineTransform::scale (k)).toNearestInt());
                drawCtl (g, c, k);
                continue;
            }
        }
        drawCtl (g, c, k);
    }
    paintBandScrollbars (g, k);   // over the strips, or the strips cover it
    if (layerView >= 0) paintLayer (g, k);
}

void NativePanel::paintBandScrollbars (juce::Graphics& g, float k)
{
    // THE SCROLLBAR, only when there is something to scroll: a thin track at
    // the area's right edge with a thumb the size of the visible share.
    for (const auto& gp : bandGroups)
    {
        if (! bandGroupShowing (gp) || gp.stackH <= gp.rect.getHeight() + 0.5f) continue;
        const auto area = gp.rect.transformedBy (juce::AffineTransform::scale (k));
        const float tw = juce::jmax (3.0f, 5.0f * k);
        const auto track = area.withWidth (tw).withX (area.getRight() - tw - 2.0f * k).reduced (0.0f, 2.0f * k);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (track, tw * 0.5f);
        const float share = gp.rect.getHeight() / gp.stackH;
        const float th = juce::jmax (18.0f * k, track.getHeight() * share);
        const float ty = track.getY() + (track.getHeight() - th) * (gp.scroll / juce::jmax (1.0f, gp.stackH - gp.rect.getHeight()));
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillRoundedRectangle (track.withY (ty).withHeight (th), tw * 0.5f);
    }
}

juce::Rectangle<float> NativePanel::reveal (const juce::String& id)
{
    const auto it = index.find (id);
    if (it == index.end()) return {};
    const auto& c = ctls[(size_t) it->second];
    if (c.band.isNotEmpty())
        for (auto& gp : bandGroups)
        {
            bool mine = false;
            for (const auto& b : gp.bands) if (b.id == c.band) mine = true;
            if (! mine) continue;
            const auto r = boundsOf (c);
            float want = gp.scroll;
            if (r.getBottom() > gp.rect.getBottom()) want += r.getBottom() - gp.rect.getBottom() + 4.0f;
            if (r.getY() < gp.rect.getY())            want -= gp.rect.getY() - r.getY() + 4.0f;
            want = juce::jlimit (0.0f, juce::jmax (0.0f, gp.stackH - gp.rect.getHeight()), want);
            if (std::abs (want - gp.scroll) > 1.0e-3f) { gp.scroll = want; reflowBands(); }
            break;
        }
    return boundsOf (ctls[(size_t) it->second]);
}

void NativePanel::drawCtl (juce::Graphics& g, Ctl& c, float k)
{
    {
        if (c.kind == "knob")            drawKnob (g, c, k);
        else if (c.kind == "fader")      drawFader (g, c, k);
        else if (c.kind == "toggle")     drawToggle (g, c, k);
        else if (c.kind == "screen")     drawScreen (g, c, k);
        else if (c.kind == "handle")     drawHandle (g, c, k);
        // ON THE GLASS a readout is printed text, not a moulded window - so it
        // is tested BEFORE drawReadout, which gives it a recessed housing that
        // belongs on the metal and buries it under the LCD.
        else if (c.kind == "readout" && isInsideAScreen (c)) drawGlassText (g, c, k);
        else if (c.kind == "readout")    drawReadout (g, c, k);
        else if (c.kind == "scope")      drawScope (g, c, k);
        else if (c.kind == "pathEditor") drawPathEditor (g, c, k);
        else if (c.kind == "stepGrid")   drawStepGrid (g, c, k);
        else if (c.kind == "list")       drawList (g, c, k);
        else if (c.kind == "meter")      drawMeter (g, c, k);
        else if (c.kind == "field")      drawField (g, c, k);
        else if (c.kind == "tabStrip")   drawTabStrip (g, c, k);
        else if (c.kind == "pageSelect")
        {
            // THE WINDOW AND THE CARET ARE PRINTED; the NAME and the DOT are
            // live. The name changes when the page does, and the dot is what
            // stops a page being a place to hide a control: it lights when
            // something on a page you cannot see is off its default.
            const int page = pageOf (c.pageSection);
            const auto name = page >= 0 && page < c.pageNames.size() ? c.pageNames[page]
                                                                     : juce::String();
            const auto r = c.rect.transformedBy (juce::AffineTransform::scale (k));
            g.setColour (c.text);
            g.setFont (juce::Font (juce::FontOptions (c.textSize * k, juce::Font::bold)));
            g.drawText (name, r.withTrimmedLeft (10.0f * k), juce::Justification::centredLeft, false);
            const auto mark = pageMark.find (c.pageSection);
            if (mark != pageMark.end() && mark->second)
                g.fillEllipse (r.getRight() - 29.0f * k, r.getCentreY() - 3.0f * k,
                               6.0f * k, 6.0f * k);
        }
        else if (c.kind == "static" || (c.kind == "button" && c.on == nullptr && c.off == nullptr))
        {
            // A latching round button that exports BOTH lamp states is drawn as a pair so its
            // ring lights when it is engaged; an older panel exports only `sprite`.
            if (c.sprite != nullptr)
            {
                const auto& im = scaledSprite (*c.sprite, k);
                if (im.isValid()) drawRaster (g, im, { (c.x - c.sprite->ax) * k, (c.y - c.sprite->ay) * k,
                                                        c.sprite->w * k, c.sprite->h * k });
            }
        }
        // A DISPLAY SEGMENT IS DRAWN FLAT, and never from a sprite. The
        // exporter gives every cap a moulded three-dimensional button and
        // bakes its legend into background.png - both right for the metal and
        // both wrong inside an LCD, where the screen is drawn over the
        // background at runtime and a segment is a flat cell of printed glass.
        // The M-ONE's algo grid went out as eighteen little plastic keys
        // floating in the display, with no text on any of them.
        //
        // Lit means INVERTED, which is what a segment does: the cell fills and
        // the text drops out of it. [MAN] the M-ONE display diagram shows
        // exactly that - the running algorithm's cell is solid with its name
        // knocked out white.
        else if (isInsideAScreen (c) && (c.kind == "cap" || c.kind == "selCap"
                                         || c.kind == "toggle"))
        {
            // c.drawn is DESIGN space and this loop paints in design * k, which
            // boundsOf() does not do - it answers the repaint question, in
            // design units, and using it here drew every segment off the panel.
            const juce::Rectangle<float> b { c.drawn.getX() * k, c.drawn.getY() * k,
                                             c.drawn.getWidth() * k, c.drawn.getHeight() * k };
            const auto ink = lcdInk();
            if (c.lit) { g.setColour (ink.withAlpha (0.92f)); g.fillRect (b); }
            else       { g.setColour (ink.withAlpha (0.30f)); g.drawRect (b, juce::jmax (1.0f, k)); }
            if (! c.label.isEmpty())
            {
                g.setColour (c.lit ? juce::Colours::black.withAlpha (0.88f)
                                   : ink.withAlpha (0.78f));
                const auto label = c.label.replace ("\n", " ");
                g.setFont (squeezedToFit (juce::Font (juce::FontOptions (
                                              juce::jmax (5.0f, b.getHeight() * 0.74f),
                                              juce::Font::bold)),
                                          label, b.getWidth() - 2.0f));
                g.drawText (label, b, juce::Justification::centred, false);
            }
        }
        // A READOUT ON THE GLASS is printed text, not a moulded window: no
        // bezel, no plate, just LCD ink. Drawn here because the sprite path
        // gives it a recessed housing that belongs on the metal.
        // A LADDER ON THE GLASS is a stack of LCD cells, not a grey plastic
        // bar. The M-ONE's INPUT meters and its two DYNAMIC meters are both
        // this, and both came out as moulded strips lying on the display.
        else if (isInsideAScreen (c) && (c.kind == "meter" || c.kind == "led"))
        {
            const juce::Rectangle<float> b { c.drawn.getX() * k, c.drawn.getY() * k,
                                             c.drawn.getWidth() * k, c.drawn.getHeight() * k };
            const auto ink = lcdInk();
            const int rows = juce::jmax (6, c.segRows > 0 ? c.segRows : 12);
            const float cell = b.getHeight() / (float) rows;
            const float on = juce::jlimit (0.0f, 1.0f, c.value) * (float) rows;
            for (int i = 0; i < rows; ++i)
            {
                // row 0 at the BOTTOM, which is how a level ladder reads
                const auto r = juce::Rectangle<float> (b.getX(), b.getBottom() - (float) (i + 1) * cell,
                                                       b.getWidth(), cell).reduced (0.0f, cell * 0.18f);
                g.setColour (ink.withAlpha ((float) i < on ? 0.90f : 0.22f));
                g.fillRect (r);
            }
        }
        else                             drawSpritePair (g, c, k, c.lit);

    }
}

/** The ink colour of the first screen on the panel, for anything drawn onto the
    glass. Falls back to a legible LCD green when the panel has no screen. */
juce::Colour NativePanel::lcdInk() const
{
    for (const auto& c : ctls)
        if (c.kind == "screen" && c.text != juce::Colour())
            return c.text;
    return juce::Colour (0xffd8e02b);
}

void NativePanel::openLayer (const juce::String& viewSection, int view)
{
    closePathPopout();
    layerSectionId = viewSection; layerView = view;
    // WHERE THE LAYER IS: the union of its controls' bounds, with room above
    // for the section titles the view image prints and a margin all round,
    // kept inside the view area. The rest of the view image is bare plate and
    // stays under the scrim.
    juce::Rectangle<float> u;
    for (const auto& c : ctls)
        if (onLayer (c)) { const auto b = boundsOf (c); u = u.isEmpty() ? b : u.getUnion (b); }
    if (u.isEmpty()) { layerView = -1; return; }
    // THE FULL WIDTH OF THE VIEW AREA, from its top: the view image prints the
    // section titles and the row legends ("left", "lane 1") to the left of the
    // controls, and a frame hugging the controls cut them off. Only the height
    // is the controls' own, so the bare plate below stays under the scrim.
    for (const auto& p : pageList)
        if (p.id == viewSection && ! p.rect.isEmpty())
            u = p.rect.withBottom (juce::jmin (p.rect.getBottom(), u.getBottom() + 24.0f));
    layerRect = u;
    repaint();
}

void NativePanel::closeLayer()
{
    if (layerView < 0) return;
    layerView = -1; layerSectionId.clear();
    repaint();
}

void NativePanel::paintLayer (juce::Graphics& g, float k)
{
    // the scrim, over everything under the layer
    g.fillAll (juce::Colours::black.withAlpha (0.62f));
    const auto r = layerRect.transformedBy (juce::AffineTransform::scale (k)).toNearestInt();
    if (r.isEmpty()) return;
    // the view's own page image, the part of it the layer covers, rescaled the
    // way paintPages does and cached the same way
    for (auto& p : pageList)
    {
        if (p.id != layerSectionId || layerView >= (int) p.images.size()) continue;
        const auto& src = p.images[(size_t) layerView];
        if (! src.isValid()) break;
        const auto full = p.rect.transformedBy (juce::AffineTransform::scale (k)).toNearestInt();
        if (layerScaledFor != scaleKey() || layerScaledView != layerView
            || layerScaled.getWidth() != juce::roundToInt (full.getWidth() * deviceScale()))
        {
            const float dev = deviceScale();
            layerScaled = src.rescaled (juce::roundToInt (full.getWidth() * dev), juce::roundToInt (full.getHeight() * dev),
                                        juce::Graphics::highResamplingQuality);
            layerScaledFor = scaleKey(); layerScaledView = layerView;
        }
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (r);
        drawRaster (g, layerScaled, full.toFloat());
        break;
    }
    // a frame, so the layer reads as a plate lifted off the panel
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawRoundedRectangle (r.toFloat().expanded (1.0f), 6.0f * k, juce::jmax (1.0f, 1.5f * k));
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.setFont (juce::Font (juce::FontOptions (11.0f * juce::jmax (1.0f, k))));
    g.drawText ("click outside to close", r.toFloat().removeFromBottom (18.0f * k).withTrimmedRight (12.0f * k),
                juce::Justification::centredRight);
    const auto clip = g.getClipBounds().toFloat();
    for (auto& c : ctls)
    {
        if (! onLayer (c) || ! visible (c)) continue;
        if (! boundsOf (c).transformedBy (juce::AffineTransform::scale (k)).intersects (clip)) continue;
        drawCtl (g, c, k);
    }
}

void NativePanel::drawSpritePair (juce::Graphics& g, Ctl& c, float k, bool on)
{
    Sprite* sp = on ? c.on : c.off;
    if (sp == nullptr) sp = c.off != nullptr ? c.off : c.on;
    if (sp == nullptr) return;
    const auto& im = scaledSprite (*sp, k); if (! im.isValid()) return;
    // Caps and toggles anchor at their top-left, LEDs at their centre: the anchor says which.
    // The image is already scaled, so draw it 1:1 - a drawImage with an explicit target
    // rectangle silently paints nothing.
    drawRaster (g, im, { (c.x - sp->ax) * k, (c.y - sp->ay) * k, sp->w * k, sp->h * k });
}

void NativePanel::drawToggle (juce::Graphics& g, Ctl& c, float k)
{
    // A centre-off switch has its own frame per lever position; drawing it as a two-state
    // control loses the middle one, which is the state most of these switches sit in.
    Sprite* sp = c.pos >= 0 && c.pos < (int) c.frames.size() ? c.frames[(size_t) c.pos] : nullptr;
    if (sp == nullptr) { drawSpritePair (g, c, k, c.lit); return; }
    const auto& im = scaledSprite (*sp, k); if (! im.isValid()) return;
    drawRaster (g, im, { (c.x - sp->ax) * k, (c.y - sp->ay) * k, sp->w * k, sp->h * k });
}

void NativePanel::drawKnob (juce::Graphics& g, Ctl& c, float k)
{
    const float cx = c.x * k, cy = c.y * k;
    // THE VALUE THE KNOB SHOWS. Unmodulated it is the value. Modulated it is
    // value + what the matrix is adding right now, and the ring, the arc and the
    // pointer all follow it - the author: *"if the lfo is manipulating cutoff the actual
    // cutoff knob in filters should be animating according to the lfo."* The
    // value itself is untouched: a drag still starts from where the knob is SET,
    // and the tick drawn below is where that is.
    const float live = c.mod.on ? juce::jlimit (0.0f, 1.0f, c.value + c.mod.offset) : c.value;
    // THE RIM THE HALO AND THE TICK SIT ON: the arc's own track when the knob has
    // one, else just outside the pointer on an older panel that draws none.
    const bool onArc = c.arc.on;
    const float rimA0 = onArc ? c.arc.a0 : c.pointer.a0;
    const float rimA1 = onArc ? c.arc.a1 : c.pointer.a1;
    const float rimR  = (onArc ? c.arc.r : juce::jmax (c.pointer.r2, c.pointer.dist) + 3.0f) * k;
    const auto  rimInk = onArc ? c.arc.colour : c.pointer.colour;
    if (c.ring.n > 1)
    {
        const int litCount = juce::roundToInt (live * (float) (c.ring.n - 1));
        // Every circle is a fraction of the LIT lamp, which is the one size the
        // export sends. A ring of 49 - one lamp per semitone across a +-24
        // transpose - shrinks all four together rather than keeping a full-size
        // bezel that overlaps its neighbours.
        const float rLit   = c.ring.r * k;
        const float rBezel = c.ring.r * (4.6f / 3.4f) * k;
        const float rOff   = c.ring.r * (3.2f / 3.4f) * k;
        const float rCore  = c.ring.r * (1.8f / 3.4f) * k;
        for (int i = 0; i < c.ring.n; ++i)
        {
            const float a = -135.0f + 270.0f * (float) i / (float) (c.ring.n - 1);
            const auto p = polar (cx, cy, c.ring.radius * k, a);
            g.setColour (c.ring.bezel);
            g.fillEllipse (p.x - rBezel, p.y - rBezel, 2 * rBezel, 2 * rBezel);
            // SINGLE lights the lamp AT the value; the bar lights every lamp up to
            // it. Drawing a positional control as a bar reports a different number
            // to the player at every setting.
            if (c.ring.single ? i == litCount : i <= litCount)
            {
                g.setColour (c.ring.lit);
                g.fillEllipse (p.x - rLit, p.y - rLit, 2 * rLit, 2 * rLit);
                g.setColour (c.ring.core);
                g.fillEllipse (p.x - rCore, p.y - rCore, 2 * rCore, 2 * rCore);
            }
            else
            {
                g.setColour (c.ring.off);
                g.fillEllipse (p.x - rOff, p.y - rOff, 2 * rOff, 2 * rOff);
            }
        }
    }
    if (c.sprite != nullptr)
    {
        const auto& im = scaledSprite (*c.sprite, k);
        if (im.isValid()) drawRaster (g, im, { (c.x - c.sprite->ax) * k, (c.y - c.sprite->ay) * k,
                                                c.sprite->w * k, c.sprite->h * k });
    }
    // The lit arc sits OUTSIDE the face, so it goes on after the body sprite and under the
    // pointer. It is deliberately not baked into the sprite: knobs of one radius SHARE a
    // sprite, so a baked arc showed one reading on all of them and never moved.
    // THE SERUM ARC: from the control's own NEUTRAL to its value, round-capped,
    // sitting ON the light ring track the sprite already printed. One drawing
    // covers both cases a Serum panel shows - a level knob sweeping from the
    // bottom-left stop, and a pan knob at centre showing a single tick at twelve
    // o'clock - because the only difference between them is where zero is.
    //
    // THE STEEL ARC is the same sweep with a soft GLOW under it: a wider,
    // translucent stroke of the same path in the theme's halo colour, drawn
    // first, so the crisp arc reads as a lit filament riding on the rim rather
    // than as a line painted on it. Measured off the design at 2 px of arc over
    // 6 px of glow at just under half opacity; the layout carries the numbers.
    if (c.mod.on)
    {
        // THE HALO: the RANGE the modulation can reach, as a wide translucent
        // band on the rim from value + lo to value + hi. [MAN] Serum 2 user guide
        // pp. 195-196: "a blue halo appears around the knob ... defining how much
        // influence the LFO has over the control (knob) position ... the halo
        // shrinks or expands to show the range of modulation." In the theme's
        // own glow colour, which the loader has already defaulted to the arc's
        // when the export names none, so an older theme still gets a halo in the
        // colour its arc lights. Drawn FIRST so the arc and pointer ride on it.
        const float lo = juce::jlimit (0.0f, 1.0f, c.value + c.mod.lo);
        const float hi = juce::jlimit (0.0f, 1.0f, c.value + c.mod.hi);
        const float aLo = rimA0 + (rimA1 - rimA0) * lo, aHi = rimA0 + (rimA1 - rimA0) * hi;
        const float hw = juce::jmax (onArc && c.arc.glowWidth > 0.0f ? c.arc.glowWidth
                                                                     : c.arc.width * 2.5f, 4.0f) * k;
        g.setColour ((onArc ? c.arc.glowColour : rimInk).withAlpha (0.38f));
        // A round cap needs length, so a range too small to stroke is drawn as
        // the dot the cap would have made - the same rule the serum arc's
        // neutral follows below.
        if (aHi - aLo < 0.75f)
        {
            const auto pt = polar (cx, cy, rimR, aLo);
            g.fillEllipse (pt.x - hw * 0.5f, pt.y - hw * 0.5f, hw, hw);
        }
        else
        {
            juce::Path halo;
            halo.addCentredArc (cx, cy, rimR, rimR, 0.0f,
                                juce::degreesToRadians (aLo), juce::degreesToRadians (aHi), true);
            g.strokePath (halo, juce::PathStrokeType (hw, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
        }
    }
    if (c.arc.on && (c.arc.style == "serum" || c.arc.style == "steel"))
    {
        const float span = c.arc.a1 - c.arc.a0;
        const float aOrg = c.arc.a0 + span * c.arc.origin;
        const float aVal = c.arc.a0 + span * live;
        const float w = c.arc.width * k;
        const float r = c.arc.r * k;
        const bool glow = c.arc.glowWidth > 0.0f && c.arc.glowOpacity > 0.0f;
        const float gw = c.arc.glowWidth * k;
        const auto glowInk = c.arc.glowColour.withAlpha (juce::jlimit (0.0f, 1.0f, c.arc.glowOpacity));
        juce::Path p;
        // A ROUND CAP NEEDS LENGTH, and at the neutral there is none - so the
        // degenerate case is drawn as the dot the cap would have made, rather
        // than as nothing at all. Without this a pan knob at centre shows an
        // empty ring and reads as broken.
        if (std::abs (aVal - aOrg) < 0.75f)
        {
            const auto pt = polar (cx, cy, r, aOrg);
            if (glow)
            {
                g.setColour (glowInk);
                g.fillEllipse (pt.x - gw * 0.5f, pt.y - gw * 0.5f, gw, gw);
            }
            g.setColour (c.arc.colour);
            g.fillEllipse (pt.x - w * 0.5f, pt.y - w * 0.5f, w, w);
        }
        else
        {
            p.addCentredArc (cx, cy, r, r, 0.0f,
                             juce::degreesToRadians (juce::jmin (aOrg, aVal)),
                             juce::degreesToRadians (juce::jmax (aOrg, aVal)), true);
            if (glow)
            {
                g.setColour (glowInk);
                g.strokePath (p, juce::PathStrokeType (gw, juce::PathStrokeType::curved,
                                                          juce::PathStrokeType::rounded));
            }
            g.setColour (c.arc.colour);
            g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }
    }
    else if (c.arc.on && live > 0.01f)
    {
        const float a0 = juce::degreesToRadians (c.arc.a0);
        const float a1 = juce::degreesToRadians (c.arc.a0 + (c.arc.a1 - c.arc.a0) * live);
        juce::Path p; p.addCentredArc (cx, cy, c.arc.r * k, c.arc.r * k, 0.0f, a0, a1, true);
        g.setColour (c.arc.colour);
        g.strokePath (p, juce::PathStrokeType (c.arc.width * k, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }
    if (c.mod.on)
    {
        // WHERE THE KNOB IS SET, as a short radial tick across the rim. The arc
        // and the pointer have gone with the live value, so without this the one
        // number the player actually chose has nothing left showing it - and a
        // drag starts from there, not from wherever the LFO happens to be.
        const float aSet = rimA0 + (rimA1 - rimA0) * c.value;
        const float reach = juce::jmax (1.5f, (onArc ? c.arc.width : 2.0f) * 0.9f) * k;
        const auto p1 = polar (cx, cy, rimR - reach, aSet), p2 = polar (cx, cy, rimR + reach, aSet);
        juce::Path tick; tick.startNewSubPath (p1); tick.lineTo (p2);
        g.setColour (rimInk.withAlpha (0.9f));
        g.strokePath (tick, juce::PathStrokeType (juce::jmax (1.0f, 1.2f * k)));
    }
    // A SWITCH LANDS ON A POSITION. Quantising here and not only in the
    // parameter is what makes it LOOK stepped: an AudioParameterInt already
    // snaps the value, but a pointer drawn from a 0..1 that was rounded on the
    // way out still sits between two printed marks whenever the rounding and
    // the drawing disagree about the step size.
    const float liveQ = c.pointer.steps > 1
                      ? std::round (live * (float) (c.pointer.steps - 1)) / (float) (c.pointer.steps - 1)
                      : live;
    const float a = c.pointer.a0 + (c.pointer.a1 - c.pointer.a0) * liveQ;
    g.setColour (c.pointer.colour);
    if (c.pointer.style == "line")
    {
        const auto p1 = polar (cx, cy, c.pointer.r1 * k, a), p2 = polar (cx, cy, c.pointer.r2 * k, a);
        juce::Path p; p.startNewSubPath (p1); p.lineTo (p2);
        g.strokePath (p, juce::PathStrokeType (c.pointer.width * k, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }
    else
    {
        const auto p = polar (cx, cy, c.pointer.dist * k, a);
        juce::Path bar;
        bar.addRoundedRectangle (-c.pointer.w * k / 2, -c.pointer.h * k / 2,
                                 c.pointer.w * k, c.pointer.h * k, c.pointer.w * k / 2);
        g.fillPath (bar, juce::AffineTransform::rotation (juce::degreesToRadians (a)).translated (p.x, p.y));
    }
    // THE NUMBER, between the two printed ends of the scale. Nothing is drawn
    // until a unit pushes one, so a panel that exports the box and an editor
    // that says nothing leaves the gap the kit printed and looks unchanged.
    if (c.value_text.isNotEmpty() && ! c.textRect.isEmpty())
    {
        const auto r = c.textRect.transformedBy (juce::AffineTransform::scale (k));
        const float fs = juce::jmin (c.textSize * k, r.getHeight());
        g.setColour (c.text);
        g.setFont (squeezedToFit (juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                                                 fs, juce::Font::bold)),
                                  c.value_text, r.getWidth()));
        g.drawText (c.value_text, r, juce::Justification::centred, false);
    }
}

void NativePanel::drawFader (juce::Graphics& g, Ctl& c, float k)
{
    if (c.track != nullptr)
    {
        const auto& im = scaledSprite (*c.track, k);
        if (im.isValid()) drawRaster (g, im, { (c.x - c.track->ax) * k, (c.y - c.track->ay) * k,
                                                c.track->w * k, c.track->h * k });
    }
    if (c.handle != nullptr)
    {
        // HORIZONTAL is the same slider a quarter turn round, and its value RISES to the
        // right - which is why it is `value` here and `1 - value` for the upright one. The
        // exporter already writes `travel` as an along-axis pair, so nothing else differs.
        const float t = c.travel0 + (c.horizontal ? c.value : 1.0f - c.value) * (c.travel1 - c.travel0);
        const auto& im = scaledSprite (*c.handle, k);
        if (im.isValid())
            drawRaster (g, im, { ((c.horizontal ? t : c.x) - c.handle->ax) * k,
                                 ((c.horizontal ? c.y : t) - c.handle->ay) * k,
                                 c.handle->w * k, c.handle->h * k });
    }
}

/** The live number in a readout window. The WINDOW itself is printed on the background by
    the exporter; only this text changes, so a value can never be baked in and frozen. */
/** A readout drawn ON AN LCD: printed text in the screen's own ink, with a hair
    border and no housing. The moulded window drawReadout gives it is right on
    the metal and wrong on glass, where it reads as a second display sunk into
    the first. */
void NativePanel::drawGlassText (juce::Graphics& g, const Ctl& c, float k) const
{
    const juce::Rectangle<float> b { c.rect.getX() * k, c.rect.getY() * k,
                                     c.rect.getWidth() * k, c.rect.getHeight() * k };
    const auto ink = lcdInk();
    const auto txt = c.value_text.isNotEmpty() ? c.value_text : c.label;
    if (txt.isEmpty()) return;                 // an empty cell prints nothing
    g.setColour (ink.withAlpha (0.26f));
    g.drawRect (b, juce::jmax (1.0f, k));
    g.setColour (ink.withAlpha (0.88f));
    g.setFont (squeezedToFit (juce::Font (juce::FontOptions (
                                  juce::jmax (5.0f, b.getHeight() * 0.64f), juce::Font::bold)),
                              txt, b.getWidth() - 3.0f));
    g.drawText (txt, b, juce::Justification::centred, false);
}

void NativePanel::drawReadout (juce::Graphics& g, Ctl& c, float k)
{
    auto shown = c.prefix + c.value_text;
    if (shown.isEmpty()) return;
    if (c.textCase == "title")
    {
        auto t = shown.toLowerCase();
        bool head = true;
        juce::String cased;
        for (int i = 0; i < t.length(); ++i)
        {
            const auto ch = t[i];
            cased << (head ? juce::CharacterFunctions::toUpperCase (ch) : ch);
            head = ! juce::CharacterFunctions::isLetterOrDigit (ch);
        }
        shown = cased;
    }
    const auto box = c.textRect.isEmpty() ? c.rect.reduced (3.0f, 0.0f) : c.textRect;
    const auto r = box.transformedBy (juce::AffineTransform::scale (k));
    if (c.lcd) { drawLcd (g, c, r, shown, juce::Justification::centred, k); return; }
    const float fs = juce::jmin (c.textSize * k, r.getHeight() * 0.8f);
    const auto font = squeezedToFit (juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                                                    fs, juce::Font::bold)),
                                     shown, r.getWidth());
    g.setColour (c.text);
    g.setFont (font);
    g.drawText (shown, r, juce::Justification::centred, false);
}

/** THE TRACE, AND NOTHING ELSE.

    The window is printed into the background by the renderer, so this draws one
    polyline and stops. A display that the host has no samples for draws NOTHING
    rather than a flat line at zero: a flat line is a claim ("this lane is
    silent") and an empty window is the truth ("nothing has been handed to me"),
    and the two are worth telling apart while wiring a machine up.

    NORMALISED BY THE TRACE'S OWN PEAK, not by full scale. A lane at a quarter
    of full level is still an oscillator doing something, and a display that
    shows it as a flat line near the centre is a display nobody looks at twice.
    The floor stops a silent lane being amplified into noise. */
/** A FIELD'S VALUE. The plate and the NAME are printed; this writes the number
    against the right-hand edge, which is the half that changes. */
void NativePanel::drawField (juce::Graphics& g, Ctl& c, float k)
{
    const auto r = c.rect.transformedBy (juce::AffineTransform::scale (k));
    if (c.lcd)
    {
        // IN THE GLASS THE KIT LEFT FREE, centred - the design centres its digit
        // in the window, measured to within a pixel on OCT and on DET's two
        // digits. Without a `textRect` the box is the whole strip, name and all,
        // so the number keeps to the right-hand edge where the older export put
        // it, or it would print over the printed name.
        if (c.textRect.isEmpty())
            drawLcd (g, c, r.reduced (6.0f * k, 0.0f), c.prefix + c.value_text,
                     juce::Justification::centredRight, k);
        else
            drawLcd (g, c, c.textRect.transformedBy (juce::AffineTransform::scale (k)),
                     c.prefix + c.value_text, juce::Justification::centred, k);
        return;
    }
    g.setColour (c.text);
    g.setFont (juce::Font (juce::FontOptions (c.textSize * k, juce::Font::bold)));
    g.drawText (c.prefix + c.value_text,
                r.toNearestInt().reduced ((int) (6 * k), 0),
                juce::Justification::centredRight, false);
}

juce::Typeface::Ptr NativePanel::lcdTypefaceFor (const Ctl& c) const
{
    // The layout names the face per window - "DSEG7 Classic" for a number,
    // "DSEG14 Classic" for a word - and the 7 is the tell. Matching on the name
    // rather than on the kind is what lets a field that shows a word (a wave
    // name in a field-shaped window, say) ask for the alphanumeric face.
    return c.lcdFont.contains ("7") ? lcdDigits : lcdAlnum;
}

/** THE SEVEN-SEGMENT WINDOW.

    The author, of the design: "The LCD old style digital clock for value displays."
    Three passes make it read as one: first the GHOST - every segment of every
    position, unlit, in the dim ink - then a soft glow of the value, then the
    value itself. The ghost is not decoration. A real display has its dead
    segments faintly visible in the glass, and without them DSEG reads as an
    odd typeface rather than as a display.

    THE GHOST MUST LAND UNDER THE VALUE glyph for glyph, so it is built one
    character at a time to keep the same advances: DSEG is monospaced at 816
    per em for a digit, a letter, "-" and "~", but "." is a zero-advance point
    hung off the previous digit, ":" is 200 wide and a SPACE is 200 as well.
    A space becomes DSEG's "!" - the blank digit, full width and nothing drawn -
    in both strings, since a real display's empty position still has its dead
    segments; "." and ":" are kept as themselves in both, so they stay put.

    The size is the window's height times kLcdFill, and it is reduced (never
    squeezed - a squeezed seven-segment digit is a different shape) when the
    string would not fit the width. */
void NativePanel::drawLcd (juce::Graphics& g, const Ctl& c, juce::Rectangle<float> box,
                           const juce::String& text, juce::Justification just, float k)
{
    if (text.isEmpty()) return;
    const auto face = lcdTypefaceFor (c);
    if (face == nullptr)
    {
        // No face from the host: the sans, in the LCD ink, at the size the
        // window would have given the sans anyway. Legible, and not the design.
        const float fs = juce::jmin (c.textSize * k, box.getHeight() * 0.8f);
        g.setColour (c.lcdInk);
        g.setFont (squeezedToFit (juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                                                 fs, juce::Font::bold)),
                                  text, box.getWidth()));
        g.drawText (text, box, just, false);
        return;
    }
    const bool seven = c.lcdFont.contains ("7");
    juce::String lit, ghost;
    for (auto t = text.getCharPointer(); ! t.isEmpty();)
    {
        const auto ch = t.getAndAdvance();
        if (ch == ' ')                   { lit << "!"; ghost << "!"; }
        else if (ch == '.' || ch == ':') { lit << juce::String::charToString (ch);
                                           ghost << juce::String::charToString (ch); }
        else                             { lit << juce::String::charToString (ch);
                                           ghost << (seven ? "8" : "~"); }
    }

    // Fit by HEIGHT first, then by width: the advance of the ghost at a reference
    // height scales linearly, so one measurement gives the largest height at
    // which the whole string still sits inside the glass with a hair to spare.
    const float refH = 100.0f;
    const float refW = juce::GlyphArrangement::getStringWidth (
                           juce::Font (juce::FontOptions (face).withHeight (refH)), ghost);
    float fs = box.getHeight() * kLcdFill;
    if (refW > 0.0f) fs = juce::jmin (fs, refH * (box.getWidth() * 0.96f) / refW);
    if (fs < 1.0f) return;
    const juce::Font font (juce::FontOptions (face).withHeight (fs));

    // Both strings are laid out by the SAME justification over the same box, so
    // with equal advances they overlay exactly; the glow is the lit path stroked
    // wide and faint, then narrower and brighter, before it is filled.
    const auto pathOf = [&] (const juce::String& str)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, str, 0.0f, 0.0f);
        ga.justifyGlyphs (0, ga.getNumGlyphs(), box.getX(), box.getY(),
                          box.getWidth(), box.getHeight(), just);
        juce::Path path; ga.createPath (path);
        return path;
    };
    auto litPath = pathOf (lit), ghostPath = pathOf (ghost);
    // CENTRED ON THE INK, NOT ON THE FONT'S METRICS. `justifyGlyphs` centres the
    // glyph arrangement's box, which carries the face's full ascent and descent;
    // a seven-segment face has no descenders and a tall nominal descent, so the
    // digits sat high in the glass by a third of the gap. The author: "the digital font
    // text in many items is too far to the top and not centered." The GHOST is
    // the reference because every segment of it is lit, so its ink box is the
    // face's true block and the lit text cannot hop about as the value changes.
    if (const auto ink = ghostPath.getBounds(); ! ink.isEmpty())
    {
        const float dy = box.getCentreY() - ink.getCentreY();
        // AND ACROSS, FOR THE SAME REASON, which took a second report to see.
        // `justifyGlyphs` centres each string by its OWN advance, and the lit
        // string and the ghost are different characters - "1/16" against four
        // tildes - so a value made of narrow glyphs was centred on its own
        // narrow box and sat left of the block behind it. The author, with a
        // screenshot: "the values are still not centered." Anchoring BOTH on
        // the ghost's ink puts the value where the block is, and a changing
        // value cannot hop about as its glyphs change width.
        //
        // ONLY WHEN THE CALLER ASKED FOR CENTRED. A field with no textRect is
        // drawn centredRight on purpose, so that it keeps off the printed name
        // beside it, and a correction here would undo that.
        const float dx = just.testFlags (juce::Justification::horizontallyCentred)
                           ? box.getCentreX() - ink.getCentreX() : 0.0f;
        litPath.applyTransform (juce::AffineTransform::translation (dx, dy));
        ghostPath.applyTransform (juce::AffineTransform::translation (dx, dy));
    }
    g.setColour (c.lcdInkDim);
    g.fillPath (ghostPath);
    g.setColour (c.lcdInk.withAlpha (0.16f));
    g.strokePath (litPath, juce::PathStrokeType (kLcdGlowWide * k, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    g.setColour (c.lcdInk.withAlpha (0.30f));
    g.strokePath (litPath, juce::PathStrokeType (kLcdGlowNarrow * k, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    g.setColour (c.lcdInk);
    g.fillPath (litPath);
}

/** WHICH TAB IS LIT, and nothing else. Every name is already on the background
    in the dim ink; this paints the selected cell and re-draws that one name in
    the accent over the top, so the strip costs one rect and one word a frame. */
void NativePanel::drawTabStrip (juce::Graphics& g, Ctl& c, float k)
{
    const int n = c.pageNames.size();
    if (n <= 0) return;
    const int sel = pageOf (c.selects.isNotEmpty() ? c.selects : c.pageSection) - c.selectBase;
    // A STRIP THAT DOES NOT HOLD THE SELECTION LIGHTS NOTHING. The clamp that
    // used to be here is what made a shared selection impossible: it pulled any
    // out-of-range value back into this strip's own cells, so every strip
    // always had exactly one lit cell whatever the others were showing.
    if (sel < 0 || sel >= n) return;
    const int page = sel;
    // THE CELL THE KIT ACTUALLY DREW, taken from the layout rather than worked
    // out again here. The kit puts a gap between cells and dividing the strip by
    // the tab count lands half a gap off, so the lit name came out beside the
    // printed one instead of over it - twice now, once as "OSOSC" and once as a
    // doubled "LFO 1". There is nothing left for the two halves to disagree on.
    auto cell = page < c.cells.size() ? c.cells.getReference (page)
                                      : juce::Rectangle<float>();
    if (cell.isEmpty())
    {
        const float cw = c.rect.getWidth() / (float) n;
        cell = { c.rect.getX() + cw * (float) page, c.rect.getY(), cw, c.rect.getHeight() };
    }
    const auto r = cell.transformedBy (juce::AffineTransform::scale (k));
    // OPAQUE, because the background raster has already printed EVERY name in
    // the dim ink - this one included. A translucent highlight leaves that copy
    // showing through under the bright one, which is the same doubling by
    // another route.
    // THE INKS: the steel pill lights its name and its ring glyph in their own
    // colours; every older strip uses the one `color` for both, as it always has.
    const auto nameInk = c.pill ? c.textLit : c.text;
    const auto ringInk = c.pill ? c.ringLit : c.text;
    if (c.pill)
    {
        // THE PILL: fully rounded, and a HALO outside it - the design's lit tab
        // blooms into the plate for about 4 px. Layered fills of the same pill
        // grown by a step each, at one low alpha, stack into a fade that is
        // densest against the edge; the pill is then filled opaque over them, so
        // the printed unlit pill and its printed dim name are covered whole.
        // THE RADIUS THE KIT EXPORTED, not half the height. A pill's corner is
        // height/2 only when the cell is wider than it is tall; the top bar's
        // view tabs are 54 px tall and about 50 wide, so height/2 drew the lit
        // one as a CIRCLE - the author: "the selected one is all of a sudden a circle???"
        // The kit already caps the radius (10 for that strip) and exports it.
        const float rad = c.pillRadius > 0.0f ? c.pillRadius * k : r.getHeight() * 0.5f;
        if (c.glowWidth > 0.0f)
        {
            constexpr int steps = 4;
            for (int i = steps; i >= 1; --i)
            {
                const float d = c.glowWidth * k * (float) i / (float) steps;
                g.setColour (c.glowLit.withAlpha (0.18f));
                g.fillRoundedRectangle (r.expanded (d), rad + d);
            }
        }
        g.setColour (c.fillLit);
        g.fillRoundedRectangle (r, rad);
        g.setColour (c.glowLit.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
    }
    else
    {
        g.setColour (c.fillLit);
        g.fillRoundedRectangle (r, 3.0f * k);
        g.setColour (c.text.withAlpha (0.75f));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f * k, 1.0f);
        // THE NOTCH, pointing down into the pane this tab opens.
        juce::Path notch;
        notch.addTriangle (r.getCentreX() - 4.0f * k, r.getBottom(),
                           r.getCentreX() + 4.0f * k, r.getBottom(),
                           r.getCentreX(),            r.getBottom() + 4.0f * k);
        g.setColour (c.fillLit);
        g.fillPath (notch);
    }
    g.setColour (nameInk);
    g.setFont (juce::Font (juce::FontOptions (c.textSize * k, juce::Font::bold)));
    const auto box = r.toNearestInt();
    if (c.hasDot)
    {
        // A TALL TAB CENTRES ITS NAME UNDER A LAMP; the kit puts the baseline at
        // 0.72 of the cell, so match that exactly rather than approximately.
        g.fillEllipse (r.getCentreX() - juce::jmax (2.0f, r.getHeight() * 0.055f),
                       r.getY() + r.getHeight() * 0.3f - juce::jmax (2.0f, r.getHeight() * 0.055f),
                       juce::jmax (2.0f, r.getHeight() * 0.055f) * 2.0f,
                       juce::jmax (2.0f, r.getHeight() * 0.055f) * 2.0f);
        const float baseline = r.getY() + r.getHeight() * 0.72f;
        const float centre = baseline - c.textSize * k * 0.36f;
        g.drawText (c.pageNames[page],
                    juce::Rectangle<float> (r.getX(), centre - r.getHeight() * 0.5f,
                                            r.getWidth(), r.getHeight()).toNearestInt(),
                    juce::Justification::centred, false);
    }
    else
    {
        // A SHORT TAB LEFT-ALIGNS ITS NAME beside the ring glyph, and centres it
        // when the cell cannot afford the glyph. The kit's own test, copied.
        const float est = (float) c.pageNames[page].length() * c.textSize * 0.56f;
        const bool room = cell.getWidth() - est - 12.0f > 10.0f;
        if (room)
        {
            // AT THE KIT'S OWN POSITION for this strip's style (tabRing), so
            // the lit ring sits exactly over the printed one and the count
            // digit drawn below lands inside both.
            float gr = 0.0f; bool hasRoom = false;
            const auto rc = tabRing (c, cell, page, gr, hasRoom);
            juce::Path ring;
            ring.addCentredArc (rc.x * k, rc.y * k, gr * k, gr * k, 0.0f,
                                juce::degreesToRadians (-140.0f), juce::degreesToRadians (140.0f), true);
            g.setColour (ringInk);
            g.strokePath (ring, juce::PathStrokeType (1.6f * k, juce::PathStrokeType::curved,
                                                              juce::PathStrokeType::rounded));
            g.setColour (nameInk);
        }
        g.drawText (c.pageNames[page],
                    room ? box.withTrimmedLeft ((int) (6 * k)) : box,
                    room ? juce::Justification::centredLeft : juce::Justification::centred, false);
    }

    // ---- THE LIVE LAYER, ON EVERY CELL: the connection count in the ring
    // glyph and the X / Y grips of a two-handle tab. Drawn over the printed
    // raster for unlit cells and over the pill for the lit one, in each cell's
    // own inks, so a tab that is connected says so whether or not it is open.
    for (int ci = 0; ci < n; ++ci)
    {
        const auto cr = tabCellRect (c, ci);
        const bool lit = ci == page;
        const auto ink = lit ? (c.pill ? c.textLit : c.text) : c.text;
        const auto ringCol = lit ? ringInk : (c.pill ? c.ringDim : c.text.withAlpha (0.6f));
        float gr2 = 0.0f; bool room2 = false;
        const auto rc = tabRing (c, cr, ci, gr2, room2);
        const int count = cellValue (c.tabCounts, ci, 0);
        if (room2 && count > 0)
        {
            // THE DIGIT, centred in the glyph. Serum prints the count inside
            // its ring (lfos.png: "3 (1)"); ours is a broken ring, so the
            // number sits in its mouth. Small, because the ring is small - and
            // bold, because a 3 px stroke is the only way a 7 px digit reads.
            const float size = juce::jmax (6.0f, gr2 * 1.9f) * k;
            g.setColour (lit ? ringCol : ink);
            g.setFont (juce::Font (juce::FontOptions (size, juce::Font::bold)));
            const auto box = juce::Rectangle<float> (rc.x * k - size, rc.y * k - size, size * 2.0f, size * 2.0f);
            g.drawText (juce::String (juce::jmin (count, 99)), box.toNearestInt(),
                        juce::Justification::centred, false);
        }
        if (cellValue (c.tabHandles, ci, 1) >= 2)
        {
            for (int hnd = 0; hnd < 2; ++hnd)
            {
                const auto gp = tabGripRect (c, cr, hnd).transformedBy (juce::AffineTransform::scale (k));
                const float rad = gp.getHeight() * 0.3f;
                const bool held = tabPressIdx >= 0 && &ctls[(size_t) tabPressIdx] == &c
                               && tabPressCell == ci && tabPressHandle == hnd && tabDragging;
                // A filled tag on the lit cell, an outlined one on the rest, so
                // the grips read as part of the tab they belong to rather than
                // as two extra buttons floating over the strip.
                g.setColour (lit ? (c.pill ? c.glowLit : c.text).withAlpha (held ? 0.95f : 0.7f)
                                 : c.text.withAlpha (held ? 0.6f : 0.28f));
                g.fillRoundedRectangle (gp, rad);
                g.setColour ((lit ? ink : c.text).withAlpha (0.9f));
                g.drawRoundedRectangle (gp.reduced (0.5f), rad, 1.0f);
                g.setColour (lit ? (c.pill ? c.fillLit : c.fillLit) : ink);
                g.setFont (juce::Font (juce::FontOptions (juce::jmax (6.0f, gp.getHeight() * 0.72f), juce::Font::bold)));
                g.drawText (hnd == 0 ? "X" : "Y", gp.toNearestInt(), juce::Justification::centred, false);
            }
        }
    }
}

void NativePanel::drawScope (juce::Graphics& g, const Ctl& c, float k)
{
    if (! scopeSource) return;
    const int n = scopeSource (c.style, traceBuf.data(), (int) traceBuf.size());
    if (n < 2) return;

    const bool curve = c.value_text == "curve";
    float peak = 0.0f;
    for (int i = 0; i < n; ++i) peak = juce::jmax (peak, std::abs (traceBuf[(size_t) i]));
    if (! curve && peak < 1.0e-4f) return;            // silent: the window stays empty
    // A TRACE IS DRAWN AT ITS LEVEL. It was normalised by its own peak, which
    // made every signal fill the window and a level knob show nothing until the
    // last few percent. The author: "Level knobs should always affect the displays."
    // Full scale is 1.0; a signal over it is fitted rather than clipped, so the
    // window never shows a flat-topped picture of a signal that is not.
    const float norm = curve ? 1.0f : 1.0f / juce::jmax (peak, 1.0f);

    const auto r = c.rect.isEmpty() ? juce::Rectangle<float> (c.x, c.y, c.w, c.h) : c.rect;
    const float x0 = r.getX() * k, y0 = r.getY() * k, w = r.getWidth() * k, h = r.getHeight() * k;
    const float mid = y0 + h * 0.5f, amp = h * 0.42f;
    const float floorY = y0 + h - 2.0f * k;

    const auto pathFor = [&] (const float* buf, int count)
    {
        juce::Path path;
        for (int i = 0; i < count; ++i)
        {
            const float px = x0 + w * (float) i / (float) (count - 1);
            const float v = buf[i];
            const float py = curve ? floorY - juce::jlimit (0.0f, 1.0f, v) * (h - 4.0f * k)
                                   : mid - juce::jlimit (-1.0f, 1.0f, v * norm) * amp;
            if (i == 0) path.startNewSubPath (px, py); else path.lineTo (px, py);
        }
        return path;
    };
    // THE GHOST FIRST, faint and thin, on the same axes: the unit's second
    // curve for this window (ENV 1 behind another envelope, on this machine).
    if (ghostSource)
    {
        std::array<float, 2048> ghostBuf {};
        const int gn = ghostSource (c.style, ghostBuf.data(), (int) ghostBuf.size());
        if (gn >= 2)
        {
            g.setColour (c.trace.withAlpha (0.22f));
            g.strokePath (pathFor (ghostBuf.data(), gn),
                          juce::PathStrokeType (juce::jmax (1.0f, c.traceWidth * k * 0.6f)));
        }
    }
    juce::Path p = pathFor (traceBuf.data(), n);
    // THE GLOW FIRST, when the layout asks for one: a wide translucent stroke
    // of the same path under the crisp trace. On the design "that glow is most
    // of what makes the display read as lit" (the theme's own note); a panel
    // whose export names no glow gets the single stroke it always had.
    if (c.traceGlowWidth > 0.0f && ! c.traceGlow.isTransparent())
    {
        g.setColour (c.traceGlow.withAlpha (0.45f));
        g.strokePath (p, juce::PathStrokeType (juce::jmax (1.0f, c.traceGlowWidth * k),
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }
    g.setColour (c.trace.withAlpha (0.95f));
    g.strokePath (p, juce::PathStrokeType (juce::jmax (1.0f, c.traceWidth * k)));
    // A CURVE IS FILLED UNDER, a trace is not. Serum's filter response is a
    // translucent wash below the line and it is what makes the shape readable
    // at a glance rather than a squiggle.
    if (curve)
    {
        juce::Path fill (p);
        fill.lineTo (x0 + w, floorY);
        fill.lineTo (x0, floorY);
        fill.closeSubPath();
        g.setColour (c.trace.withAlpha (0.18f));
        g.fillPath (fill);
    }
    drawHandles (g, c, k);
}

/** THE DRAGGABLE POINTS ON A DISPLAY.

    The author: *"the screens should be interactive where available. Filters/LFO/Env
    should have points on the actual screen so you can shape it and it alters the
    actual knob state"*, and again: *"why are there still no drag points on the
    scopes? on the env scope there needs to be drag points for atc/decay/sustain/
    release."*

    The panel owns the GESTURE and the host owns the MEANING. This file knows
    only that a display has some number of points at normalised positions and
    that dragging one reports a new normalised position; which parameters that
    moves - and in which direction, and on what curve - is the host's, because
    only the host knows that the filter window's x is a logarithmic cutoff and
    the envelope's second point is a time AND a level at once. */
/** WHERE A POINT IS DRAWN, in design px.

    INSET BY ITS OWN RADIUS, so a point at either end of its travel is still a
    whole circle inside the window and still has something to grab. The envelope's
    release point sits at x = 1 by construction - it IS the end of the tail - and
    without this it was half a ring hanging off the right edge of the glass. The
    hit test uses this same function, because a point you can see in one place and
    catch in another is worse than no point. */
juce::Point<float> NativePanel::handlePoint (juce::Rectangle<float> r, const Handle& h) const
{
    const auto in = r.reduced (kHandleRadius + 1.0f);
    return { in.getX() + in.getWidth()  * juce::jlimit (0.0f, 1.0f, h.x),
             in.getY() + in.getHeight() * (1.0f - juce::jlimit (0.0f, 1.0f, h.y)) };
}

void NativePanel::drawHandles (juce::Graphics& g, const Ctl& c, float k)
{
    if (! handleSource) return;
    const int n = handleSource (c.id, handleBuf.data(), (int) handleBuf.size());
    const auto r = c.rect.isEmpty() ? juce::Rectangle<float> (c.x, c.y, c.w, c.h) : c.rect;
    for (int i = 0; i < n; ++i)
    {
        const auto p = handlePoint (r, handleBuf[(size_t) i]);
        const auto& hnd = handleBuf[(size_t) i];
        const float px = p.x * k, py = p.y * k;
        const float rad = kHandleRadius * k * juce::jlimit (0.5f, 6.0f, hnd.scale);
        const bool held = dragScope == c.id && dragHandle == hnd.id;
        // A RING, NOT A DISC: the curve passes through the point and a filled
        // dot hides the very thing the point is there to shape.
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (px - rad, py - rad, rad * 2.0f, rad * 2.0f);
        g.setColour (c.text.withAlpha (held ? 1.0f : 0.9f));
        g.drawEllipse (px - rad, py - rad, rad * 2.0f, rad * 2.0f,
                       juce::jmax (1.0f, (held ? 2.2f : 1.6f) * k));
        // A MACRO POINT SAYS SO: a second, fainter ring inside it, so a point
        // that moves seven controls does not look like a point that moves one.
        if (hnd.scale > 1.5f)
        {
            const float inner = rad * 0.45f;
            g.setColour (c.text.withAlpha (held ? 0.8f : 0.45f));
            g.drawEllipse (px - inner, py - inner, inner * 2.0f, inner * 2.0f,
                           juce::jmax (1.0f, 1.2f * k));
        }
    }
}

// ---------------------------------------------------------------------------
// THE DRAWN PATH.
//
// Everything below is the GESTURE half of the LFO editor. It knows that a path
// is a list of points, that a segment can be bent, and that a window has a grid;
// it does not know what an LFO is, how fast it runs, or where the path is kept.
// Every edit goes out through `onPathEdit` as a complete list and comes back in
// through `PathSource` on the next repaint, so what is on the glass is always
// what the host actually kept - never what it was asked for.

/** THE BEND LAW. It MUST stay identical to megasynth's `pathCurve`
    (src/engine/LfoPath.h) and it is copied here rather than included because
    this library is shared by every machine and cannot depend on one machine's
    engine. If the two ever differ, the engine reads a shape the editor never
    drew - a fault that is inaudible until it is a click in somebody's mix and
    that looking at the picture will never find, because the picture is right.

    The bend warps TIME, not level, which is what keeps both ends exactly on
    their points however hard it is bent. The exponent is bounded at 5: an
    unbounded power turns a segment into a step, and a step drawn by accident is
    a click. */
float NativePanel::pathBend (float t, float curve) noexcept
{
    if (curve > -1.0e-4f && curve < 1.0e-4f) return t;
    const float k = 1.0f + std::abs (curve) * 4.0f;      // 1 .. 5
    return curve > 0.0f ? std::pow (t, k)
                        : 1.0f - std::pow (1.0f - t, k);
}

/** `pathBend(0.5, curve) = f`, solved for curve, so a bend handle lands under
    the pointer instead of approaching it. At f = 0.5 the answer is 0 and the
    segment is straight; beyond the law's own limit (f = 0.5^5) it saturates,
    which is the same ceiling the law has. */
float NativePanel::pathBendForMid (float f) noexcept
{
    const float v = juce::jlimit (1.0e-3f, 1.0f - 1.0e-3f, f);
    const float k = std::log (v <= 0.5f ? v : 1.0f - v) / std::log (0.5f);
    const float c = juce::jlimit (0.0f, 1.0f, (k - 1.0f) * 0.25f);
    return v <= 0.5f ? c : -c;
}

float NativePanel::pathValueAt (const std::vector<PathPoint>& p, float x) noexcept
{
    if (p.size() < 2) return p.empty() ? 0.5f : p.front().y;
    const float ph = juce::jlimit (0.0f, 1.0f, x);
    size_t i = 0;
    while (i + 2 < p.size() && p[i + 1].x <= ph) ++i;
    const float w = p[i + 1].x - p[i].x;
    if (w < 1.0e-5f) return p[i + 1].y;                  // a vertical jump reads as its top
    return p[i].y + (p[i + 1].y - p[i].y) * pathBend ((ph - p[i].x) / w, p[i].curve);
}

/** A path position as a point in DESIGN pixels, through the kit's own plot rect.
    The inverse goes through the same rect, so what is drawn and what is grabbed
    are one map and a point cannot be visible in one place and catchable in
    another - the rule `handlePoint` already follows for a scope's handles. */
juce::Point<float> NativePanel::pathToDesign (const Ctl& c, float x, float y) const noexcept
{
    const auto& r = c.plot;
    return { r.getX() + r.getWidth()  * juce::jlimit (0.0f, 1.0f, x),
             r.getY() + r.getHeight() * (1.0f - juce::jlimit (0.0f, 1.0f, y)) };
}

juce::Point<float> NativePanel::pathFromDesign (const Ctl& c, juce::Point<float> d) const noexcept
{
    const auto& r = c.plot;
    if (r.getWidth() <= 0.0f || r.getHeight() <= 0.0f) return { 0.0f, 0.5f };
    return { juce::jlimit (0.0f, 1.0f, (d.x - r.getX()) / r.getWidth()),
             juce::jlimit (0.0f, 1.0f, 1.0f - (d.y - r.getY()) / r.getHeight()) };
}

/** THE BEND HANDLE'S POSITION: the segment's temporal middle, ON the curve.
    Serum draws it there and it is the only place it can go - a handle that sits
    on the straight chord between two points is a handle that leaves the line it
    is supposed to be bending as soon as you bend it. */
juce::Point<float> NativePanel::pathMid (const Ctl& c, const std::vector<PathPoint>& p, int seg) const noexcept
{
    if (seg < 0 || seg + 1 >= (int) p.size()) return {};
    const auto& a = p[(size_t) seg]; const auto& b = p[(size_t) seg + 1];
    if (c.pathMode == kPathFigure) return figurePoint (a, b, 0.5f);
    return { (a.x + b.x) * 0.5f, a.y + (b.y - a.y) * pathBend (0.5f, a.curve) };
}

int NativePanel::pathFetch (const Ctl& c, std::vector<PathPoint>& out) const
{
    out.clear();
    if (! pathSource) return 0;
    // A STACK BUFFER, because this is called from paint and from every step of a
    // drag: the host fills a plain array and this turns it into the vector the
    // editing below works in. Thirty-two points is 384 bytes and never allocates.
    std::array<PathPoint, kMaxPathPoints> buf {};
    const int n = juce::jlimit (0, kMaxPathPoints, pathSource (c.id, buf.data(), kMaxPathPoints));
    if (n < 2) return 0;                                 // fewer than two points is not a path
    out.assign (buf.begin(), buf.begin() + n);
    return n;
}

/** HAND THE WHOLE LIST BACK AND DRAW NOTHING ON THE STRENGTH OF IT. The host
    validates, clamps and stores; the next repaint asks what it ended up with. A
    panel that also kept a copy would show the player an edit the patch refused. */
void NativePanel::pathEmit (const Ctl& c, std::vector<PathPoint>& p)
{
    if (p.size() < 2) return;
    // THE CYCLE IS CLOSED, and this is the one invariant worth enforcing on the
    // way out rather than leaving to the host: the first point is phase 0 and the
    // last is phase 1, or a reader divides by a segment of negative width and
    // hands back a value from somewhere else entirely.
    if ((int) p.size() > kMaxPathPoints) p.resize ((size_t) kMaxPathPoints);
    // A FIGURE'S ENDS ARE FREE: its first point is where the LFO starts and its
    // last where the cycle ends, and neither is a time. Pinning them would drag
    // every figure's ends to the window's edges on the first edit.
    if (c.pathMode != kPathFigure)
    {
        p.front().x = 0.0f;
        p.back().x = 1.0f;
    }
    for (auto& q : p)
    {
        q.x = juce::jlimit (0.0f, 1.0f, q.x);
        q.y = juce::jlimit (0.0f, 1.0f, q.y);
        q.curve = juce::jlimit (-1.0f, 1.0f, q.curve);
    }
    if (onPathEdit) onPathEdit (c.id, p.data(), (int) p.size());
    repaintCtl (c);
}

/** ONE GRID COLUMN, REPLACED. Flat, Ramp Up, Ramp Down and a shift-click step
    are all this function with different ends.

    THE JUMP BETWEEN COLUMNS IS THE POINT. A staircase is not a sequence of
    ramps: each step holds its level and then jumps, so the column's neighbours
    keep a point of their own at the boundary and the two land at the same x with
    different levels. The path reader treats a zero-width segment as a jump, which
    is what makes a step sequencer drawable at all.

    Re-filling a column that was already filled must not accumulate boundary
    points, so at most one is kept on each side: the FIRST at the left edge (the
    previous run's terminator) and the LAST at the right (the next run's start). */
void NativePanel::pathFillCell (std::vector<PathPoint>& p, int col, int gridV, float y0, float y1)
{
    if (gridV < 1 || p.size() < 2) return;
    col = juce::jlimit (0, gridV - 1, col);
    constexpr float eps = 1.0e-4f;
    const float x0 = (float) col / (float) gridV;
    const float x1 = (float) (col + 1) / (float) gridV;
    const float wasL = pathValueAt (p, x0), wasR = pathValueAt (p, x1);

    std::vector<PathPoint> left, right;
    for (const auto& q : p)
    {
        if (q.x <= x0 + eps)      left.push_back (q);
        else if (q.x >= x1 - eps) right.push_back (q);
    }
    while (left.size() >= 2 && std::abs (left[left.size() - 1].x - x0) <= eps
                            && std::abs (left[left.size() - 2].x - x0) <= eps)
        left.pop_back();
    while (right.size() >= 2 && std::abs (right[0].x - x1) <= eps
                             && std::abs (right[1].x - x1) <= eps)
        right.erase (right.begin());
    // The first and last columns ARE the ends of the cycle: there is no previous
    // run to terminate and no next one to start.
    if (col == 0) left.clear();
    if (col == gridV - 1) right.clear();

    std::vector<PathPoint> out;
    out.reserve (p.size() + 4);
    for (const auto& q : left) out.push_back (q);
    if (! left.empty() && std::abs (left.back().x - x0) > eps)
        out.push_back ({ x0, wasL, 0.0f });              // close the untouched part at its own level
    out.push_back ({ x0, y0, 0.0f });
    out.push_back ({ x1, y1, 0.0f });
    if (! right.empty() && std::abs (right.front().x - x1) > eps)
        out.push_back ({ x1, wasR, 0.0f });
    for (const auto& q : right) out.push_back (q);

    // A PATH THAT WOULD NOT FIT IS NOT WRITTEN. Truncating it would silently
    // throw away the far end of the cycle, which is a worse answer than the
    // column declining to fill - and at sixteen columns a full staircase lands
    // on exactly the thirty-two points the format has.
    if ((int) out.size() > kMaxPathPoints) return;
    p.swap (out);
}

/** THE OUTPUT METER: a bar that fills from the bottom, green over most of its
    travel and the accent's own warning colour at the top, with a peak line that
    falls slowly. A printed meter reads the same at silence and at clipping. */
void NativePanel::drawMeter (juce::Graphics& g, Ctl& c, float k)
{
    const auto full = (c.rect.isEmpty() ? juce::Rectangle<float> (c.x, c.y, c.w, c.h) : c.rect)
                          .transformedBy (juce::AffineTransform::scale (k));
    // A BAR IS INSET INSIDE ITS PRINTED GLASS; A LADDER IS NOT. The ladder's
    // cells have to land on the discs the panel printed, so insetting it would
    // put every cell a pixel and a half off its own well.
    const auto r = c.segRows > 1 ? full : full.reduced (1.5f * k);
    if (r.isEmpty()) return;
    // THE PEAK HOLD, decayed here rather than by a timer: this is called on the
    // repaint the level itself asked for, so the fall is tied to the frames the
    // meter actually gets and a hidden meter does not drift.
    c.value2 = juce::jmax (c.value, c.value2 - 0.012f);
    const float lit = juce::jlimit (0.0f, 1.0f, c.value);
    // THE LADDER. Cells are counted from the BOTTOM, which is both how a meter
    // fills and how the colours were given, and the row rectangle is the pitch
    // so a cell lands exactly where the panel printed its unlit disc. The peak
    // hold is one extra cell rather than a hairline, because a line across a
    // gap between discs reads as a scratch on the glass.
    if (c.segRows > 1)
    {
        const float pitch = r.getHeight() / (float) c.segRows;
        const float cellH = pitch * c.segFill;
        const int on   = (int) std::floor (lit * (float) c.segRows + 0.0001f);
        const int hold = (int) std::floor (juce::jlimit (0.0f, 1.0f, c.value2) * (float) c.segRows + 0.0001f);
        for (int i = 0; i < c.segRows; ++i)
        {
            const bool isHold = (i == hold - 1 && hold > on);
            if (i >= on && ! isHold) continue;
            const auto cell = juce::Rectangle<float> (r.getX(),
                                                      r.getBottom() - (float) (i + 1) * pitch + (pitch - cellH) * 0.5f,
                                                      r.getWidth(), cellH);
            auto col = i < c.segColours.size() ? c.segColours[i]
                                               : (c.text.isTransparent() ? juce::Colour (0xff55c8fa) : c.text);
            g.setColour (isHold ? col.withAlpha (0.45f) : col);
            g.fillRoundedRectangle (cell, cell.getWidth() * 0.5f);
        }
        return;
    }
    if (lit > 0.001f)
    {
        const auto bar = r.withTop (r.getBottom() - r.getHeight() * lit);
        const auto hot = c.text.isTransparent() ? juce::Colour (0xff55c8fa) : c.text;
        g.setGradientFill (juce::ColourGradient (hot.withAlpha (0.95f), bar.getX(), r.getBottom(),
                                                 juce::Colours::orangered, bar.getX(), r.getY(), false));
        g.fillRect (bar);
    }
    if (c.value2 > 0.001f)
    {
        const float y = r.getBottom() - r.getHeight() * juce::jlimit (0.0f, 1.0f, c.value2);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.fillRect (r.getX(), y - 0.5f * k, r.getWidth(), juce::jmax (1.0f, k));
    }
}

// --- the step grid ---------------------------------------------------------
//
// A SEQUENCER LANE AND A PIANO ROLL ARE ONE WINDOW DIVIDED TWO WAYS. Everything
// below is the path editor's contract applied to a grid: the host owns the
// pattern, the panel owns the gesture, and an edit hands the WHOLE new list back
// rather than a description of the change - so the window and the state it came
// from cannot drift apart, and what is drawn is always what the host actually
// kept rather than what the pointer asked for.
void NativePanel::setGridSource (GridSource s)
{
    gridSource = std::move (s);
    repaintDisplays();
}

NativePanel::Ctl* NativePanel::gridCtl (const juce::String& id)
{
    const auto it = index.find (id);
    if (it == index.end()) return nullptr;
    auto& c = ctls[(size_t) it->second];
    return c.kind == "stepGrid" ? &c : nullptr;
}

void NativePanel::setGridSize (const juce::String& id, int rows, int cols)
{
    auto* c = gridCtl (id); if (c == nullptr) return;
    // ZERO MEANS "LEAVE THAT AXIS ALONE", so a unit that changes only the length
    // does not have to know what the row count was set to in the panel spec.
    const int r = rows > 0 ? juce::jlimit (1, 128, rows) : c->rows;
    const int n = cols > 0 ? juce::jlimit (1, kMaxGridCells, cols) : c->cols;
    if (r == c->rows && n == c->cols) return;
    c->rows = r; c->cols = n;
    // A GESTURE INDEXED BY COLUMN MEANS NOTHING ONCE THE COLUMNS HAVE MOVED,
    // which is the same reason setPathMode drops a path gesture and a selection.
    if (dragGrid == id) gridEndGesture();
    repaintCtl (*c);
}

juce::Point<int> NativePanel::gridSize (const juce::String& id) const
{
    const auto it = index.find (id);
    if (it == index.end()) return {};
    const auto& c = ctls[(size_t) it->second];
    return c.kind == "stepGrid" ? juce::Point<int> { c.rows, c.cols } : juce::Point<int> {};
}

void NativePanel::setGridBase (const juce::String& id, int firstRow)
{
    auto* c = gridCtl (id); if (c == nullptr || c->gridBase == firstRow) return;
    c->gridBase = firstRow;
    repaintCtl (*c);
}

void NativePanel::setGridRowLabels (const juce::String& id, const juce::StringArray& labels)
{
    auto* c = gridCtl (id); if (c == nullptr) return;
    if (c->rowLabels == labels) return;
    c->rowLabels = labels;
    repaintCtl (*c);
}

void NativePanel::setGridPlayhead (const juce::String& id, int col)
{
    auto* c = gridCtl (id); if (c == nullptr) return;
    const int n = col < 0 ? -1 : juce::jmin (col, c->cols - 1);
    // Called at display rate, so it repaints only when the head has actually
    // moved a column - the reason setPathCursor checks the same thing.
    if (n == c->playCol) return;
    c->playCol = n;
    repaintCtl (*c);
}

int NativePanel::gridPlayhead (const juce::String& id) const
{
    const auto it = index.find (id);
    if (it == index.end()) return -1;
    const auto& c = ctls[(size_t) it->second];
    return c.kind == "stepGrid" ? c.playCol : -1;
}

/** ROW 0 IS AT THE BOTTOM. That is what puts a piano roll's low notes low on the
    window and an arpeggiator's first degree under its second, and it has to be
    said in exactly one place or the half that draws and the half that hit-tests
    will each pick a direction. */
juce::Point<int> NativePanel::gridCellAt (const Ctl& c, juce::Point<float> d) const noexcept
{
    if (! c.plot.contains (d)) return { -1, -1 };
    const int cols = juce::jmax (1, c.cols), rows = juce::jmax (1, c.rows);
    const int col = juce::jlimit (0, cols - 1,
        (int) ((d.x - c.plot.getX()) / juce::jmax (1.0f, c.plot.getWidth()) * (float) cols));
    const int row = juce::jlimit (0, rows - 1, rows - 1
        - (int) ((d.y - c.plot.getY()) / juce::jmax (1.0f, c.plot.getHeight()) * (float) rows));
    return { col, row };
}

juce::Rectangle<float> NativePanel::gridCellRect (const Ctl& c, const GridCell& cell) const noexcept
{
    const int cols = juce::jmax (1, c.cols), rows = juce::jmax (1, c.rows);
    const float cw = c.plot.getWidth() / (float) cols;
    const float gap = juce::jlimit (0.0f, cw * 0.4f, c.cellGap);
    const int col = juce::jlimit (0, cols - 1, cell.col);
    const int len = c.span ? juce::jlimit (1, cols - col, cell.len) : 1;
    const float x = c.plot.getX() + cw * (float) col + gap * 0.5f;
    const float w = juce::jmax (1.0f, cw * (float) len - gap);
    if (c.lane)
    {
        // A BAR STANDING ON THE FLOOR: the value IS the height, so a step at
        // zero reads as zero instead of as a full cell in another colour.
        const float h = juce::jmax (1.0f, c.plot.getHeight() * juce::jlimit (0.0f, 1.0f, cell.value));
        return { x, c.plot.getBottom() - h, w, h };
    }
    const float chh = c.plot.getHeight() / (float) rows;
    const float y = c.plot.getY() + chh * (float) (rows - 1 - juce::jlimit (0, rows - 1, cell.row))
                    + gap * 0.5f;
    return { x, y, w, juce::jmax (1.0f, chh - gap) };
}

int NativePanel::gridFetch (const Ctl& c, std::vector<GridCell>& out) const
{
    out.clear();
    if (! gridSource) return 0;
    // A STACK BUFFER, as pathFetch uses: the host fills a plain array and this
    // turns it into the vector the editing below works in, so a repaint never
    // allocates on the way in.
    std::array<GridCell, (size_t) kMaxGridCells> buf {};
    const int n = juce::jlimit (0, kMaxGridCells, gridSource (c.id, buf.data(), kMaxGridCells));
    out.assign (buf.begin(), buf.begin() + n);
    return n;
}

/** HAND THE WHOLE LIST BACK AND DRAW NOTHING ON THE STRENGTH OF IT, exactly as
    pathEmit does. An EMPTY list is legitimate here and a path's never is: a
    pattern with every note rubbed out is a pattern, and refusing to emit it
    would make the last note on a grid impossible to delete. */
void NativePanel::gridEmit (const Ctl& c, std::vector<GridCell>& cells)
{
    if ((int) cells.size() > kMaxGridCells) cells.resize ((size_t) kMaxGridCells);
    for (auto& q : cells)
    {
        q.col = juce::jlimit (0, juce::jmax (1, c.cols) - 1, q.col);
        q.row = juce::jlimit (0, juce::jmax (1, c.rows) - 1, q.row);
        // A WINDOW THAT DID NOT DECLARE `span` HAS NO LENGTHS. Letting one
        // through would draw a cell over its neighbours on a grid whose whole
        // model is one cell per step.
        q.len = c.span ? juce::jlimit (1, juce::jmax (1, c.cols) - q.col, q.len) : 1;
        q.value = juce::jlimit (0.0f, 1.0f, q.value);
    }
    if (onGridEdit) onGridEdit (c.id, cells.data(), (int) cells.size());
    repaintCtl (c);
}

namespace
{
    /** The cell whose BODY covers this column on this row, or -1. Its body and
        not its head, or a four-step note could only be erased by clicking the
        one column it starts on. */
    int gridCover (const std::vector<NativePanel::GridCell>& cells, int col, int row) noexcept
    {
        for (int i = 0; i < (int) cells.size(); ++i)
        {
            const auto& q = cells[(size_t) i];
            if (q.row == row && col >= q.col && col < q.col + juce::jmax (1, q.len)) return i;
        }
        return -1;
    }
    /** The cell on this column of a one-row lane, or -1. */
    int gridAtCol (const std::vector<NativePanel::GridCell>& cells, int col) noexcept
    {
        for (int i = 0; i < (int) cells.size(); ++i)
            if (cells[(size_t) i].col == col) return i;
        return -1;
    }
}

/** WHAT A NEW CELL ARRIVES AT. Not 1.0: a roll whose every note is drawn at full
    velocity is a roll with no dynamics until somebody edits every note, and 0.79
    is 100 of 127, which is what every sequencer's pencil writes. */
static constexpr float kGridNewValue = 100.0f / 127.0f;

void NativePanel::gridBeginGesture (Ctl& c, juce::Point<float> d, const juce::ModifierKeys& mods)
{
    const auto at = gridCellAt (c, d);
    if (at.x < 0) return;
    std::vector<GridCell> cells;
    gridFetch (c, cells);
    dragGrid = c.id;
    if (onGridGrab) onGridGrab (c.id, true);

    if (c.lane)
    {
        // A LANE IS PAINTED, NOT TOGGLED. The pointer's HEIGHT is the value and
        // a drag writes it into every column it crosses, which is the only
        // gesture that can set sixteen steps in one movement - and it is what a
        // velocity lane does on every sequencer anybody has used.
        gridGesture = kGridLevel;
        const float v = juce::jlimit (0.0f, 1.0f,
            1.0f - (d.y - c.plot.getY()) / juce::jmax (1.0f, c.plot.getHeight()));
        const int i = gridAtCol (cells, at.x);
        if (i >= 0) cells[(size_t) i].value = v;
        else        cells.push_back ({ at.x, 0, 1, v });
        gridEmit (c, cells);
        return;
    }

    // A CLICK ON A FILLED CELL ERASES IT AND A CLICK ON AN EMPTY ONE FILLS IT,
    // and which of the two this gesture is, is decided HERE and then held for
    // the whole drag. A drag that decided per cell would rub out everything it
    // crossed on the way to what it was drawing.
    const int found = gridCover (cells, at.x, at.y);
    if (found >= 0 || mods.isPopupMenu())
    {
        gridGesture = kGridErase;
        if (found >= 0)
        {
            cells.erase (cells.begin() + found);
            gridEmit (c, cells);
        }
        return;
    }
    gridGesture = kGridPaint;
    gridAnchorCol = at.x; gridAnchorRow = at.y;
    cells.push_back ({ at.x, at.y, 1, kGridNewValue });
    gridEmit (c, cells);
}

void NativePanel::gridDragTo (Ctl& c, juce::Point<float> d)
{
    const auto at = gridCellAt (c, d);
    if (at.x < 0) return;
    std::vector<GridCell> cells;
    gridFetch (c, cells);
    bool changed = false;

    if (gridGesture == kGridLevel)
    {
        const float v = juce::jlimit (0.0f, 1.0f,
            1.0f - (d.y - c.plot.getY()) / juce::jmax (1.0f, c.plot.getHeight()));
        const int i = gridAtCol (cells, at.x);
        if (i >= 0) { changed = std::abs (cells[(size_t) i].value - v) > 1.0e-4f;
                      cells[(size_t) i].value = v; }
        else        { cells.push_back ({ at.x, 0, 1, v }); changed = true; }
    }
    else if (gridGesture == kGridErase)
    {
        if (const int i = gridCover (cells, at.x, at.y); i >= 0)
        { cells.erase (cells.begin() + i); changed = true; }
    }
    else if (gridGesture == kGridPaint)
    {
        // DRAGGING RIGHT ALONG THE ROW YOU STARTED ON LENGTHENS THE NOTE; moving
        // off that row starts a new one. That is how a note gets a length in
        // every piano roll there is, and it is why `span` exists: on a grid
        // whose model is one cell per step the same drag paints a run of cells
        // instead.
        const int anchor = gridCover (cells, gridAnchorCol, gridAnchorRow);
        if (c.span && at.y == gridAnchorRow && at.x >= gridAnchorCol && anchor >= 0)
        {
            const int len = at.x - cells[(size_t) anchor].col + 1;
            if (cells[(size_t) anchor].len != len) { cells[(size_t) anchor].len = len; changed = true; }
        }
        else if (gridCover (cells, at.x, at.y) < 0)
        {
            cells.push_back ({ at.x, at.y, 1, kGridNewValue });
            gridAnchorCol = at.x; gridAnchorRow = at.y;
            changed = true;
        }
    }
    if (changed) gridEmit (c, cells);
}

void NativePanel::gridEndGesture()
{
    if (dragGrid.isEmpty()) return;
    const auto id = dragGrid;
    dragGrid = {}; gridGesture = kGridNone; gridAnchorCol = -1; gridAnchorRow = -1;
    if (onGridGrab) onGridGrab (id, false);
}

void NativePanel::drawStepGrid (juce::Graphics& g, const Ctl& c, float k)
{
    const auto plot = c.plot.transformedBy (juce::AffineTransform::scale (k));
    const int cols = juce::jmax (1, c.cols), rows = juce::jmax (1, c.rows);
    const float cw = plot.getWidth() / (float) cols;
    const float ch = plot.getHeight() / (float) rows;
    const float hair = juce::jmax (1.0f, k);

    // THE BLACK-KEY ROWS, TINTED, so a roll can be read as a keyboard rather
    // than as squared paper. The painter is told the bottom row's MIDI note and
    // works out the rest; it is never told what a piano is.
    if (c.gridBase >= 0 && rows > 1)
    {
        static constexpr bool kBlack[12] = { false, true, false, true, false, false,
                                             true, false, true, false, true, false };
        g.setColour (c.rowTint.withAlpha (0.45f));
        for (int r = 0; r < rows; ++r)
            if (kBlack[(size_t) (((c.gridBase + r) % 12 + 12) % 12)])
                g.fillRect (plot.getX(), plot.getY() + ch * (float) (rows - 1 - r), plot.getWidth(), ch);
    }

    // THE PLAYHEAD'S COLUMN, UNDER the cells rather than over them: a wash over
    // a note would change the note's colour, and the one thing a playhead must
    // never do is make the pattern harder to read.
    if (c.playCol >= 0 && c.playCol < cols)
    {
        g.setColour (c.playInk.withAlpha (0.16f));
        g.fillRect (plot.getX() + cw * (float) c.playCol, plot.getY(), cw, plot.getHeight());
    }

    // THE DIVISIONS, DRAWN AND NOT PRINTED, because the column count follows the
    // pattern's LENGTH and its step value and the row count follows its range -
    // all three of which move while the plugin runs. Same split, same reason, as
    // the LFO graph's grid.
    for (int i = 0; i <= cols; ++i)
    {
        const bool bar = c.beat > 0 && i % c.beat == 0;
        g.setColour ((bar ? c.gridMajor : c.edge)
                         .withAlpha (bar ? juce::jmin (1.0f, c.gridOpacity * 2.2f) : c.gridOpacity));
        g.fillRect (plot.getX() + cw * (float) i, plot.getY(), hair, plot.getHeight());
    }
    if (rows > 1)
    {
        g.setColour (c.edge.withAlpha (c.gridOpacity));
        for (int j = 0; j <= rows; ++j)
            g.fillRect (plot.getX(), plot.getY() + ch * (float) j, plot.getWidth(), hair);
    }

    // THE ROW BANDS AND THEIR NAMES. A twelve-lane grid of identical rows is a
    // wall; alternating the tone and writing the lane's name at the left edge is
    // what a drum machine's own display does and what makes one lane tell itself
    // from its neighbour. Drawn UNDER the cells, so a hit always wins.
    if (! c.rowLabels.isEmpty() && rows > 1)
    {
        const float fs = juce::jlimit (7.0f * k, 13.0f * k, ch * 0.52f);
        g.setFont (juce::Font (juce::FontOptions (fs, juce::Font::bold)));
        for (int j = 0; j < rows; ++j)
        {
            // Row 0 is the BOTTOM row, as GridCell's own index is, so the label
            // for row j is drawn at rows-1-j from the top.
            const float y = plot.getY() + ch * (float) (rows - 1 - j);
            if ((j & 1) == 0)
            {
                g.setColour (c.edge.withAlpha (juce::jmin (0.5f, c.gridOpacity * 1.6f)));
                g.fillRect (plot.getX(), y, plot.getWidth(), ch);
            }
            if (j < c.rowLabels.size())
            {
                g.setColour (c.text.withAlpha (0.62f));
                g.drawText (c.rowLabels[j],
                            juce::Rectangle<float> (plot.getX() + 4.0f * k, y,
                                                    plot.getWidth() * 0.5f, ch),
                            juce::Justification::centredLeft, false);
            }
        }
    }

    std::vector<GridCell> cells;
    const int n = gridFetch (c, cells);
    for (int i = 0; i < n; ++i)
    {
        const auto r = gridCellRect (c, cells[(size_t) i])
                           .transformedBy (juce::AffineTransform::scale (k));
        // HOW HARD THE STEP IS, IN THE BRIGHTNESS. A velocity that only exists
        // in a lane under the grid is a velocity nobody sees while they are
        // writing notes; the floor is 0.45 so a quiet note is still legible.
        const float v = juce::jlimit (0.0f, 1.0f, cells[(size_t) i].value);
        const float rad = juce::jmin (3.0f * k, juce::jmin (r.getWidth(), r.getHeight()) * 0.4f);
        g.setColour (c.cellFill.withAlpha (c.lane ? 0.85f : 0.45f + 0.55f * v));
        g.fillRoundedRectangle (r, rad);
        // The lit core, inset, which is what makes a cell read as a lamp on
        // glass rather than as a painted square - the same pair a path point has.
        const auto core = r.reduced (juce::jmin (r.getWidth(), r.getHeight()) * 0.22f);
        if (core.getWidth() > 0.5f && core.getHeight() > 0.5f)
        {
            g.setColour (c.cellCore.withAlpha (c.lane ? 0.55f : 0.35f + 0.5f * v));
            g.fillRoundedRectangle (core, juce::jmin (rad, core.getHeight() * 0.4f));
        }
    }

    // AND THE PLAYHEAD'S OWN LINE, ON TOP, at the leading edge of its column. The
    // wash underneath disappears the moment the column is full of notes, and a
    // playhead you cannot find while the pattern is dense is a playhead that is
    // missing exactly when it is wanted.
    if (c.playCol >= 0 && c.playCol < cols)
    {
        g.setColour (c.playInk.withAlpha (0.85f));
        g.fillRect (plot.getX() + cw * (float) c.playCol, plot.getY(),
                    juce::jmax (1.5f, 1.5f * k), plot.getHeight());
    }
}

// --- the list -------------------------------------------------------------
//
// The step grid's contract applied to rows of text: the host owns the names and
// the selection, the panel owns the window, the scroll and the click. Nothing
// below learns what a program is.

void NativePanel::setListSource (ListSource src)
{
    listSource = std::move (src);
    repaintDisplays();
}

NativePanel::Ctl* NativePanel::listCtl (const juce::String& id)
{
    for (auto& c : ctls)
        if (c.id == id)
            return c.kind == "list" ? &c : nullptr;
    return nullptr;
}

void NativePanel::setListLength (const juce::String& id, int rows)
{
    auto* c = listCtl (id); if (c == nullptr) return;
    const int want = juce::jmax (0, rows);
    // AND THE SCROLL IS CLAMPED WITH IT. A filter that cuts five hundred
    // programs to nine leaves the window looking at row 400 otherwise, which is
    // an empty list that the player has no way to know is not empty.
    const int visible = juce::jmax (1, c->rows * juce::jmax (1, c->columns));
    const int top = juce::jlimit (0, juce::jmax (0, want - visible), c->listTop);
    if (c->listLen == want && c->listTop == top) return;
    c->listLen = want;
    c->listTop = top;
    repaintCtl (*c);
}

void NativePanel::setListScroll (const juce::String& id, int firstRow)
{
    auto* c = listCtl (id); if (c == nullptr) return;
    const int visible = juce::jmax (1, c->rows * juce::jmax (1, c->columns));
    const int top = juce::jlimit (0, juce::jmax (0, c->listLen - visible), firstRow);
    if (c->listTop == top) return;
    c->listTop = top;
    repaintCtl (*c);
}

int NativePanel::listScroll (const juce::String& id) const
{
    for (const auto& c : ctls)
        if (c.id == id) return c.kind == "list" ? c.listTop : 0;
    return 0;
}

int NativePanel::listVisibleRows (const juce::String& id) const
{
    for (const auto& c : ctls)
        if (c.id == id)
            return c.kind == "list" ? juce::jmax (1, c.rows * juce::jmax (1, c.columns)) : 0;
    return 0;
}

/** Where the Nth VISIBLE row is drawn, in design units. Rows flow down a column
    and then into the next, the way a newspaper sets type, which is the only way
    a list of five hundred names fits a plate this shape. */
juce::Rectangle<float> NativePanel::listRowRect (const Ctl& c, int visibleIndex) const noexcept
{
    const int perCol = juce::jmax (1, c.rows);
    const int col = visibleIndex / perCol, row = visibleIndex % perCol;
    const float cw = c.colW > 0.0f ? c.colW : c.plot.getWidth() / (float) juce::jmax (1, c.columns);
    return { c.plot.getX() + cw * (float) col,
             c.plot.getY() + c.rowH * (float) row,
             cw, c.rowH };
}

int NativePanel::listRowAt (const Ctl& c, juce::Point<float> d) const noexcept
{
    if (! c.plot.contains (d)) return -1;
    const int perCol = juce::jmax (1, c.rows);
    const float cw = c.colW > 0.0f ? c.colW : c.plot.getWidth() / (float) juce::jmax (1, c.columns);
    const int col = juce::jlimit (0, juce::jmax (0, c.columns - 1),
                                  (int) ((d.x - c.plot.getX()) / juce::jmax (1.0f, cw)));
    const int row = (int) ((d.y - c.plot.getY()) / juce::jmax (1.0f, c.rowH));
    if (row < 0 || row >= perCol) return -1;
    const int at = c.listTop + col * perCol + row;
    return at < c.listLen ? at : -1;
}

void NativePanel::drawList (juce::Graphics& g, const Ctl& c, float k)
{
    const auto plot = c.plot.transformedBy (juce::AffineTransform::scale (k));
    const int perCol = juce::jmax (1, c.rows);
    const int visible = perCol * juce::jmax (1, c.columns);
    if (! listSource) return;

    std::vector<ListRow> rows ((size_t) juce::jmin (visible, kMaxListRows));
    const int n = rows.empty() ? 0
        : juce::jlimit (0, (int) rows.size(),
                        listSource (c.id, c.listTop, rows.data(), (int) rows.size()));

    const float fs = juce::jmax (8.0f, c.rowH * 0.62f) * k;
    // SEVEN, NOT FOUR, AND IT IS A MULTI-COLUMN DECISION. A row prints its name
    // at the left and its second field at the right, so in a three-column list
    // the right field of one column lands beside the NAME of the next: at four
    // px of padding the two are eight px apart and "C28  Brass Pad Swell" reads
    // as one row. The gutter is what separates them, and it costs nothing a name
    // was using.
    const float pad = 7.0f * k;
    for (int i = 0; i < n; ++i)
    {
        const auto& r = rows[(size_t) i];
        const auto box = listRowRect (c, i).transformedBy (juce::AffineTransform::scale (k));
        if (r.header)
        {
            // A GROUP HEADING IS NOT A ROW ANYBODY CHOOSES, so it is drawn as a
            // rule with a name on it rather than as a name: the difference has
            // to be visible before the click, not after it.
            g.setColour (c.textDim.withAlpha (0.85f));
            g.setFont (juce::Font (juce::FontOptions (fs * 0.92f, juce::Font::bold)));
            g.drawText (r.text.toUpperCase(), box.reduced (pad, 0.0f),
                        juce::Justification::centredLeft, true);
            g.setColour (c.edge.withAlpha (0.55f));
            g.fillRect (box.getX() + pad, box.getBottom() - juce::jmax (1.0f, k),
                        box.getWidth() - pad * 2.0f, juce::jmax (1.0f, k));
            continue;
        }
        if (r.selected)
        {
            // THE SELECTED ROW IS A LAMP ON GLASS, the same body-and-core pair a
            // lit step-grid cell is drawn with, so one design language covers
            // every window on the plate.
            const auto lit = box.reduced (1.0f * k, 0.5f * k);
            g.setColour (c.cellFill.withAlpha (0.55f));
            g.fillRoundedRectangle (lit, juce::jmin (3.0f * k, lit.getHeight() * 0.4f));
            g.setColour (c.cellCore.withAlpha (0.35f));
            g.fillRoundedRectangle (lit.reduced (lit.getHeight() * 0.18f),
                                    juce::jmin (2.0f * k, lit.getHeight() * 0.3f));
        }
        const auto ink = r.selected ? c.cellCore : (r.dim ? c.textDim : c.text);
        g.setColour (ink.withAlpha (r.dim && ! r.selected ? 0.65f : 1.0f));
        g.setFont (juce::Font (juce::FontOptions (fs)));
        // THE RIGHT-HAND FIELD TAKES ITS SPACE FIRST, so a long name is
        // truncated rather than printed through the bank letter beside it.
        auto text = box.reduced (pad, 0.0f);
        if (r.right.isNotEmpty())
        {
            const float rw = juce::jmin (text.getWidth() * 0.4f, fs * (float) r.right.length() * 0.7f + pad);
            auto rr = text.removeFromRight (rw);
            g.setColour ((r.selected ? c.cellCore : c.textDim).withAlpha (0.8f));
            g.setFont (juce::Font (juce::FontOptions (fs * 0.86f)));
            g.drawText (r.right, rr, juce::Justification::centredRight, true);
            g.setColour (ink.withAlpha (r.dim && ! r.selected ? 0.65f : 1.0f));
            g.setFont (juce::Font (juce::FontOptions (fs)));
        }
        g.drawText (r.text, text, juce::Justification::centredLeft, true);
    }

    // THE SCROLLBAR, and it exists only when there is something off the window.
    // A bar that is always there and always full says a list scrolls when it
    // does not, which is the same lie a lit cap with no handler tells.
    if (c.listLen > visible)
    {
        const float w = juce::jmax (2.0f, 3.0f * k);
        const float track = plot.getHeight();
        const float thumb = juce::jmax (12.0f * k, track * (float) visible / (float) c.listLen);
        const float at = (track - thumb) * (float) c.listTop
                       / (float) juce::jmax (1, c.listLen - visible);
        g.setColour (c.edge.withAlpha (0.35f));
        g.fillRect (plot.getRight() - w, plot.getY(), w, track);
        g.setColour (c.cellFill.withAlpha (0.7f));
        g.fillRect (plot.getRight() - w, plot.getY() + at, w, thumb);
    }
}

void NativePanel::drawPathEditor (juce::Graphics& g, const Ctl& c, float k)
{
    const auto plot = c.plot.transformedBy (juce::AffineTransform::scale (k));

    // ---- THE AUDIO, BEHIND EVERYTHING, AT LOW OPACITY ----------------------
    //
    // Drawn FIRST so the grid, the curve and the points all sit on top of it.
    // The author: "you should be able to see the actual sound/waveform in the
    // background so its easy to shape" - and a duck placed against a kick you
    // can see is the entire reason the COMP window is a window and not a
    // dropdown. Mirrored about the centre line, as a waveform is.
    if (! c.backdrop.empty() && plot.getWidth() > 1.0f)
    {
        const int n = (int) c.backdrop.size();
        juce::Path wave;
        wave.startNewSubPath (plot.getX(), plot.getCentreY());
        for (int i = 0; i < n; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (n - 1);
            wave.lineTo (x, plot.getCentreY()
                            - juce::jlimit (0.0f, 1.0f, c.backdrop[(size_t) i])
                              * plot.getHeight() * 0.5f);
        }
        for (int i = n - 1; i >= 0; --i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (n - 1);
            wave.lineTo (x, plot.getCentreY()
                            + juce::jlimit (0.0f, 1.0f, c.backdrop[(size_t) i])
                              * plot.getHeight() * 0.5f);
        }
        wave.closeSubPath();
        g.setColour (c.text.withAlpha (0.16f));
        g.fillPath (wave);
    }
    // THE GRID, DRAWN AND NOT PRINTED, because it is a parameter: Serum 2 user
    // guide p. 192, "The visual grid background in the LFO graph changes as you
    // adjust this setting". The kit deliberately leaves it out of the background
    // raster, so this is the only copy of it - and it is drawn in the plot rect
    // the kit exported, because alt-drag snaps to these lines and the line you
    // see has to be the line a point lands on. The SAME grid in every mode: a
    // figure snaps to it on both axes.
    //
    // BOTH OUTER LINES ARE DRAWN: the leftmost is phase 0 and the rightmost is
    // where the cycle closes. The middle ROW is skipped when there is one, since
    // the centre line is already printed there and two lines at one y is a
    // brighter line rather than a grid - the same rule `scopeWindow` follows.
    g.setColour (c.edge.withAlpha (c.gridOpacity));
    const float hair = juce::jmax (1.0f, k);
    for (int i = 0; i <= c.gridV; ++i)
    {
        const float gx = plot.getX() + plot.getWidth() * (float) i / (float) c.gridV;
        g.fillRect (gx, plot.getY(), hair, plot.getHeight());
    }
    for (int j = 0; j <= c.gridH; ++j)
    {
        if (j * 2 == c.gridH) continue;
        const float gy = plot.getY() + plot.getHeight() * (float) j / (float) c.gridH;
        g.fillRect (plot.getX(), gy, plot.getWidth(), hair);
    }

    const auto strokeTrace = [&] (const juce::Path& line, float alphaScale)
    {
        if (c.traceGlowWidth > 0.0f && ! c.traceGlow.isTransparent())
        {
            g.setColour (c.traceGlow.withAlpha (0.45f * alphaScale));
            g.strokePath (line, juce::PathStrokeType (juce::jmax (1.0f, c.traceGlowWidth * k),
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }
        g.setColour (c.trace.withAlpha (alphaScale));
        g.strokePath (line, juce::PathStrokeType (juce::jmax (1.0f, c.traceWidth * k),
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    };
    const auto drawCursor = [&]
    {
        if (! c.cursorOn) return;
        // THE MOVING DOT: where the LFO is right now, the one thing on this
        // window that moves without anybody touching it. A filled disc of the
        // trace colour with a light core, a shade larger than a point so it is
        // never mistaken for one.
        const auto d = pathToDesign (c, c.cursorX, c.cursorY);
        const float cr = (c.pointR + 1.5f) * k;
        g.setColour (c.trace.withAlpha (0.35f));
        g.fillEllipse (d.x * k - cr * 1.8f, d.y * k - cr * 1.8f, cr * 3.6f, cr * 3.6f);
        g.setColour (c.trace);
        g.fillEllipse (d.x * k - cr, d.y * k - cr, cr * 2.0f, cr * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.fillEllipse (d.x * k - cr * 0.45f, d.y * k - cr * 0.45f, cr * 0.9f, cr * 0.9f);
    };

    if (c.pathMode == kPathTrace)
    {
        // THE TRAIL. Nothing here is editable: an attractor has no points to
        // drag, so the window shows where the LFO has BEEN, oldest first and
        // fading, and where it is now. Drawn in a few strokes of rising alpha
        // rather than one, so the tail dies away instead of stopping.
        const int pairs = (int) c.trail.size() / 2;
        if (pairs >= 2)
        {
            constexpr int bands = 4;
            for (int b = 0; b < bands; ++b)
            {
                const int from = pairs * b / bands, to = juce::jmin (pairs - 1, pairs * (b + 1) / bands);
                if (to <= from) continue;
                juce::Path seg;
                for (int i = from; i <= to; ++i)
                {
                    const auto d = pathToDesign (c, c.trail[(size_t) i * 2], c.trail[(size_t) i * 2 + 1]);
                    if (i == from) seg.startNewSubPath (d.x * k, d.y * k); else seg.lineTo (d.x * k, d.y * k);
                }
                strokeTrace (seg, 0.25f + 0.75f * (float) (b + 1) / (float) bands);
            }
        }
        drawCursor();
        return;
    }

    std::vector<PathPoint> pts;
    if (pathFetch (c, pts) < 2)
    {
        // No source, or a source that has nothing to say. The window keeps its
        // grid and its centre line rather than drawing a straight line that
        // would read as a real, flat path.
        drawCursor();
        return;
    }
    const bool figure = c.pathMode == kPathFigure;

    // THE CURVE, sampled along each segment with the SAME law the engine reads
    // it with - pathBend for a graph, figurePoint for a figure. Roughly one
    // sample every two screen pixels: a bend at the law's limit is nearly a
    // step, and a polyline of eight points across a 200 px segment shows that
    // step as a staircase of its own.
    juce::Path line;
    bool started = false;
    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        const auto a = pts[i], b = pts[i + 1];
        const auto p0 = pathToDesign (c, a.x, a.y), p1 = pathToDesign (c, b.x, b.y);
        if (! started) { line.startNewSubPath (p0.x * k, p0.y * k); started = true; }
        else           { line.lineTo (p0.x * k, p0.y * k); }
        const float span = figure ? p0.getDistanceFrom (p1) : std::abs (p1.x - p0.x);
        const int steps = juce::jlimit (1, 128, juce::roundToInt (span * k * 0.5f));
        for (int s = 1; s < steps; ++s)
        {
            const float t = (float) s / (float) steps;
            const auto q = figure ? [&] { const auto f = figurePoint (a, b, t); return pathToDesign (c, f.x, f.y); }()
                                  : pathToDesign (c, a.x + (b.x - a.x) * t,
                                                     a.y + (b.y - a.y) * pathBend (t, a.curve));
            line.lineTo (q.x * k, q.y * k);
        }
        line.lineTo (p1.x * k, p1.y * k);
    }
    // The curve takes the same glow-under-trace treatment a scope does; the
    // points and handles below stay in the export's `color`, as they were.
    strokeTrace (line, 1.0f);

    // THE BEND HANDLES, hollow, at each segment's middle. Hollow is not
    // decoration: a main point and a bend handle do different things, and the
    // one reliable way to tell two small circles apart at 6 px is that one is
    // filled and the other is not. A segment too short to hold a handle without
    // sitting on top of its own endpoints does not get one.
    const float hr = c.handleR * k;
    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        const auto d0 = pathToDesign (c, pts[i].x, pts[i].y), d1 = pathToDesign (c, pts[i + 1].x, pts[i + 1].y);
        const float span = (figure ? d0.getDistanceFrom (d1) : std::abs (d1.x - d0.x)) * k;
        if (span < hr * 4.0f) continue;
        const auto m = pathMid (c, pts, (int) i);
        const auto d = pathToDesign (c, m.x, m.y);
        const bool held = dragPath == c.id && dragBend == (int) i;
        g.setColour (c.text.withAlpha (held ? 1.0f : 0.75f));
        g.drawEllipse (d.x * k - hr, d.y * k - hr, hr * 2.0f, hr * 2.0f,
                       juce::jmax (1.0f, (held ? 2.0f : 1.4f) * k));
    }

    // THE POINTS. Filled, which is the opposite of what a scope's handles do -
    // and deliberately, because a scope handle RIDES on a continuous trace and a
    // disc would hide the very thing it shapes, while a path point is where two
    // segments END. Nothing passes through it to hide, and Serum draws it filled
    // for the same reason.
    const float pr = c.pointR * k;
    for (size_t i = 0; i < pts.size(); ++i)
    {
        const auto d = pathToDesign (c, pts[i].x, pts[i].y);
        const bool on = (pathSelOn == c.id
                         && std::find (pathSel.begin(), pathSel.end(), (int) i) != pathSel.end())
                        || (dragPath == c.id && dragPoint == (int) i);
        g.setColour (c.text);
        g.fillEllipse (d.x * k - pr - k, d.y * k - pr - k, (pr + k) * 2.0f, (pr + k) * 2.0f);
        g.setColour (c.pointFill);
        g.fillEllipse (d.x * k - pr, d.y * k - pr, pr * 2.0f, pr * 2.0f);
        // A SELECTED POINT WEARS A HALO RATHER THAN A DIFFERENT FILL. The core is
        // already the accent's light core, which is within a shade of white, so
        // recolouring it says almost nothing at 4 px - a ring outside the point
        // is the only change of state this size of mark can actually carry.
        if (on)
        {
            const float hl = pr + 2.5f * k;
            g.setColour (juce::Colours::white);
            g.drawEllipse (d.x * k - hl, d.y * k - hl, hl * 2.0f, hl * 2.0f,
                           juce::jmax (1.0f, 1.6f * k));
        }
    }
    // A FIGURE SAYS WHERE IT STARTS. Its first point is where every cycle
    // begins and its order is otherwise invisible, so the start wears a small
    // outer ring - the one mark a figure needs that a graph does not.
    if (figure)
    {
        const auto d = pathToDesign (c, pts[0].x, pts[0].y);
        const float sr = pr + 3.5f * k;
        g.setColour (c.trace.withAlpha (0.8f));
        g.drawEllipse (d.x * k - sr, d.y * k - sr, sr * 2.0f, sr * 2.0f, juce::jmax (1.0f, 1.2f * k));
    }

    // THE RUBBER BAND, while it is being dragged. `[MAN]` p. 190, "Click-drag on
    // the background: Select multiple points."
    if (pathBand && dragPath == c.id)
    {
        const auto band = juce::Rectangle<float> (bandFrom, bandTo)
                              .transformedBy (juce::AffineTransform::scale (k));
        g.setColour (c.text.withAlpha (0.14f));
        g.fillRect (band);
        g.setColour (c.text.withAlpha (0.6f));
        g.drawRect (band, juce::jmax (1.0f, k));
    }
    drawCursor();
}

// --- the gestures ----------------------------------------------------------
// The Serum 2 user guide's "Modifying LFOs" table (p. 190) is the specification
// for this section, and each branch names the row it implements.

void NativePanel::pathApplyTool (Ctl& c, juce::Point<float> d, int tool)
{
    std::vector<PathPoint> pts;
    if (pathFetch (c, pts) < 2) return;
    const auto v = pathFromDesign (c, d);
    const int col = juce::jlimit (0, c.gridV - 1, (int) std::floor (v.x * (float) c.gridV));
    if (col == pathCell) return;                         // this column is already drawn
    pathCell = col;
    // `[MAN]` p. 189: every tool "relies on the current grid setting". FLAT holds
    // the level the pointer is at; the two RAMPS run the full height of the
    // column, which is what their own icons show and what makes a row of them a
    // saw rather than a row of shallow slopes.
    switch (tool)
    {
        case kPathRampUp:   pathFillCell (pts, col, c.gridV, 0.0f, 1.0f); break;
        case kPathRampDown: pathFillCell (pts, col, c.gridV, 1.0f, 0.0f); break;
        default:            pathFillCell (pts, col, c.gridV, v.y, v.y);   break;
    }
    pathEmit (c, pts);
}

void NativePanel::pathBeginGesture (Ctl& c, juce::Point<float> d, const juce::ModifierKeys& mods)
{
    // A TRACE WINDOW IS NOT EDITED. There are no points behind an attractor.
    if (c.pathMode == kPathTrace) return;
    std::vector<PathPoint> pts;
    if (pathFetch (c, pts) < 2) return;
    dragPath = c.id; dragPoint = -1; dragBend = -1; pathBand = false; pathCell = -1;
    pathGrab = pts;
    if (onPathGrab) onPathGrab (c.id, true);

    // A TOOL OTHER THAN POINT FILLS COLUMNS, and keeps filling them while the
    // pointer is dragged across the window - which is how a staircase gets drawn
    // in one gesture rather than in eight clicks. ON A GRAPH ONLY: a figure has
    // no column to fill - a flat or a ramp is a shape in time - so on a figure
    // the four tools all move points, which is the one thing they can mean.
    const bool graph = c.pathMode == kPathTime;
    if (graph && pathToolNow != kPathPoint) { pathApplyTool (c, d, pathToolNow); return; }

    // `[MAN]` p. 190, Shift-click: "Draw steps at the grid size (step
    // sequencer)." The Point tool borrows the Flat tool for one click.
    if (graph && mods.isShiftDown() && ! mods.isAltDown()) { pathApplyTool (c, d, kPathFlat); return; }

    // A POINT IS GRABBED BEFORE ITS BEND HANDLE, and both before the background.
    // The grab radius is generous because these are small targets on a window
    // that is otherwise inert, so a near miss has nothing to hit by mistake.
    float best = c.pointR * 2.4f; int pick = -1;
    for (size_t i = 0; i < pts.size(); ++i)
    {
        const float dist = pathToDesign (c, pts[i].x, pts[i].y).getDistanceFrom (d);
        if (dist < best) { best = dist; pick = (int) i; }
    }
    if (pick >= 0)
    {
        dragPoint = pick;
        // A POINT OUTSIDE THE SELECTION REPLACES IT. Dragging one point while
        // three others are highlighted and watching all four move is the single
        // most surprising thing a multi-selection can do.
        if (pathSelOn != c.id
            || std::find (pathSel.begin(), pathSel.end(), pick) == pathSel.end())
        { pathSel.clear(); pathSelOn = c.id; }
        repaintCtl (c);
        return;
    }
    best = juce::jmax (c.handleR * 2.6f, 5.0f);
    for (size_t i = 0; i + 1 < pts.size(); ++i)
    {
        const auto m = pathMid (c, pts, (int) i);
        const float dist = pathToDesign (c, m.x, m.y).getDistanceFrom (d);
        if (dist < best) { best = dist; dragBend = (int) i; }
    }
    if (dragBend >= 0) { bendStart = pts[(size_t) dragBend].curve; repaintCtl (c); return; }

    // `[MAN]` p. 190, Click-drag on the background: "Select multiple points."
    pathBand = true; bandFrom = d; bandTo = d; pathSel.clear(); pathSelOn = c.id; repaintCtl (c);
}

void NativePanel::pathDragGesture (Ctl& c, juce::Point<float> d, const juce::ModifierKeys& mods)
{
    const bool figure = c.pathMode == kPathFigure;
    if (! figure && pathToolNow != kPathPoint) { pathApplyTool (c, d, pathToolNow); return; }
    if (pathBand) { bandTo = d; repaintCtl (c); return; }
    // SHIFT KEEPS DRAWING STEPS while the pointer moves. `[MAN]` p. 190 calls it
    // shift-CLICK, but a step sequencer is eight steps and eight separate clicks
    // is not what anybody means by drawing one - and every column the pointer has
    // already filled is remembered, so the gesture never fights itself.
    if (! figure && dragPoint < 0 && dragBend < 0 && mods.isShiftDown() && ! mods.isAltDown())
    { pathApplyTool (c, d, kPathFlat); return; }

    if (dragBend >= 0 && dragBend + 1 < (int) pathGrab.size())
    {
        auto pts = pathGrab;
        const auto& a = pts[(size_t) dragBend]; const auto& b = pts[(size_t) dragBend + 1];
        const auto v = pathFromDesign (c, d);
        float bend;
        if (figure)
        {
            // A FIGURE'S BEND IS A BULGE: the handle sits on the curve at its
            // middle, which is HALF the control point's offset from the chord
            // (figurePoint at t = 0.5), so the curve that puts the handle under
            // the pointer is twice the pointer's offset along the chord's left
            // normal, over the bulge law's scale. A segment of no length has no
            // normal and nothing to bend.
            const float cx = b.x - a.x, cy = b.y - a.y;
            const float len2 = cx * cx + cy * cy;
            if (len2 < 1.0e-8f) return;
            const float mx = (a.x + b.x) * 0.5f, my = (a.y + b.y) * 0.5f;
            const float off = (-(v.x - mx) * cy + (v.y - my) * cx) / len2;   // along the left normal, in chords
            bend = juce::jlimit (-1.0f, 1.0f, 2.0f * off / kFigureBulge);
        }
        else
        {
            // A BEND WARPS TIME, so a segment whose ends are at the same level
            // has the same value at every time and there is nothing a bend could
            // do to it. Storing a number that draws no picture would be worse
            // than a handle that does not move.
            if (std::abs (b.y - a.y) < 1.0e-4f) return;
            bend = pathBendForMid ((v.y - a.y) / (b.y - a.y));
        }
        // `[MAN]` p. 190, Option/Alt-click-drag any curve point: "Move all curve
        // points at once."
        if (mods.isAltDown()) for (auto& q : pts) q.curve = bend;
        else                  pts[(size_t) dragBend].curve = bend;
        pathEmit (c, pts);
        return;
    }

    if (dragPoint < 0 || dragPoint >= (int) pathGrab.size()) return;
    auto pts = pathGrab;
    const int n = (int) pts.size();
    const auto from = pathToDesign (c, pathGrab[(size_t) dragPoint].x, pathGrab[(size_t) dragPoint].y);
    const auto v0 = pathFromDesign (c, from), v1 = pathFromDesign (c, d);
    float dx = v1.x - v0.x, dy = v1.y - v0.y;

    // `[MAN]` p. 190, Option/Alt-click-drag a point: "Snap the point to the grid
    // size." The GRABBED point is snapped and the rest of a selection follows by
    // the same delta, so the one under the pointer lands on a line and the shape
    // of everything else is preserved.
    if (mods.isAltDown())
    {
        const auto& g0 = pathGrab[(size_t) dragPoint];
        const float sx = std::round ((g0.x + dx) * (float) c.gridV) / (float) c.gridV;
        const float sy = std::round ((g0.y + dy) * (float) c.gridH) / (float) c.gridH;
        dx = sx - g0.x; dy = sy - g0.y;
    }

    std::vector<int> move = pathSelOn == c.id ? pathSel : std::vector<int>();
    if (std::find (move.begin(), move.end(), dragPoint) == move.end()) move.assign (1, dragPoint);

    // CLAMP THE WHOLE MOVE, not each point. On a GRAPH a point may not be
    // dragged past a neighbour that is standing still - crossing one would
    // renumber the list under the pointer and the drag would jump to a
    // different point - and the ends of the cycle are pinned in time by
    // construction, because the first point IS phase 0 and the last IS phase 1.
    // On a FIGURE none of that holds: x is a place, not a time, the order is
    // the order of the list and a point may go anywhere in the window.
    float lo = -2.0f, hi = 2.0f, loY = -2.0f, hiY = 2.0f;
    for (const int i : move)
    {
        if (figure) {}
        else if (i <= 0 || i >= n - 1) { lo = hi = 0.0f; }
        else
        {
            if (std::find (move.begin(), move.end(), i - 1) == move.end())
                lo = juce::jmax (lo, pathGrab[(size_t) i - 1].x - pathGrab[(size_t) i].x);
            if (std::find (move.begin(), move.end(), i + 1) == move.end())
                hi = juce::jmin (hi, pathGrab[(size_t) i + 1].x - pathGrab[(size_t) i].x);
        }
        lo = juce::jmax (lo, -pathGrab[(size_t) i].x);
        hi = juce::jmin (hi, 1.0f - pathGrab[(size_t) i].x);
        loY = juce::jmax (loY, -pathGrab[(size_t) i].y);
        hiY = juce::jmin (hiY, 1.0f - pathGrab[(size_t) i].y);
    }
    dx = juce::jlimit (juce::jmin (lo, 0.0f), juce::jmax (hi, 0.0f), dx);
    dy = juce::jlimit (juce::jmin (loY, 0.0f), juce::jmax (hiY, 0.0f), dy);
    for (const int i : move)
    {
        pts[(size_t) i].x = juce::jlimit (0.0f, 1.0f, pathGrab[(size_t) i].x + dx);
        pts[(size_t) i].y = juce::jlimit (0.0f, 1.0f, pathGrab[(size_t) i].y + dy);
    }
    pathEmit (c, pts);
}

void NativePanel::pathEndGesture()
{
    if (dragPath.isEmpty()) return;
    if (auto* c = pathCtl (dragPath))
    {
        if (pathBand)
        {
            // `[MAN]` p. 190. The band selects on RELEASE: a selection that
            // changed on every mouse-move would flicker points in and out while
            // the band was still being drawn.
            std::vector<PathPoint> pts;
            pathSel.clear(); pathSelOn = c->id;
            const auto band = juce::Rectangle<float> (bandFrom, bandTo);
            if (pathFetch (*c, pts) >= 2)
                for (size_t i = 0; i < pts.size(); ++i)
                    if (band.contains (pathToDesign (*c, pts[i].x, pts[i].y)))
                        pathSel.push_back ((int) i);
        }
        repaintCtl (*c);
    }
    if (onPathGrab) onPathGrab (dragPath, false);
    dragPath = {}; dragPoint = -1; dragBend = -1; pathBand = false; pathCell = -1;
    pathGrab.clear();
}

void NativePanel::drawScreen (juce::Graphics& g, Ctl& c, float k)
{
    const auto r = juce::Rectangle<float> (c.rect.getX() + 3, c.rect.getY() + 3,
                                           c.rect.getWidth() - 6, c.rect.getHeight() - 6)
                     .transformedBy (juce::AffineTransform::scale (k));
    g.setGradientFill (juce::ColourGradient (c.bg0, r.getX(), r.getY(), c.bg1, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.0f * k);
    g.setColour (c.edge.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 4.0f * k, 1.0f * k);
    const int n = c.lines.size(); if (n == 0) return;
    const float ih = r.getHeight(), iw = r.getWidth();
    // A SEGMENT SCREEN draws through the same painter a readout uses, so one
    // number fills its window in the face the theme names, unlit segments
    // ghosted. It is the PSP 85's BPM/VAL window, and it has to look like one.
    if (c.lcd && n == 1)
    {
        drawLcd (g, c, r.reduced (5.0f * k, 3.0f * k), c.lines[0], juce::Justification::centred, k);
        return;
    }

    // A seven-segment block shows ONE number and should FILL its window; the glow and lcd
    // styles are the multi-line character displays the other machines have.
    if (c.style == "led" && n == 1)
    {
        const int len = juce::jmax (2, c.lines[0].length());
        const float fs = juce::jmin (ih * 0.80f, iw / (float) len * 1.55f);
        g.setColour (c.text);
        g.setFont (juce::Font (juce::FontOptions ("Menlo", fs, juce::Font::bold)));
        g.drawText (c.lines[0], r, juce::Justification::centred, false);
        return;
    }

    int maxLen = 8; for (const auto& l : c.lines) maxLen = juce::jmax (maxLen, l.length());
    const bool lcd = c.style == "lcd";
    // AN EXPLICIT SIZE WINS, and a small screen needs one.
    //
    // The derived size assumes a screen is a multi-line character display, so it
    // takes 0.42 of the height for one line and 0.36 for several - which is
    // right for a two-line patch display and wrong for a one-word window
    // standing next to readouts that use 0.66. Measured: the filter's response
    // window came out at 10.9 px beside 19.8 px readouts on the same plate.
    // The author: *"we should use the same screen size for the screen in filters as we
    // do everywhere else. The font is way to small."*
    //
    // The WIDTH still caps it, so a long word squeezes rather than spilling: an
    // explicit size is the author saying how big the type should be, not a
    // promise that any string will fit.
    // THREE OR MORE LINES HAVE TO SHARE THE HEIGHT, and until now they did not:
    // a four-line display was sized at 0.40 of the glass per line and laid out
    // at 0.36 + 0.42 i, so lines three and four were drawn at 1.20 and 1.62 of
    // the height - off the glass, and invisible because it clips. The kit's SVG
    // fixed this on the Reverb 4000's three-line screen and the host never did,
    // so the picture and the plugin disagreed about how many lines a display
    // shows. 1.61, not 1.72, for the same reason the kit uses it: a monospace
    // glyph advances 0.6 em, so 1/0.6 = 1.667 is the break-even and anything
    // above it runs a full-width line off the glass.
    const float lead = c.lineHeight;
    const float derived = lcd ? juce::jmin (lead > 0.0f ? ih / ((float) n * lead)
                                                        : (n <= 2 ? ih * 0.40f : ih * 0.80f / (float) n),
                                            iw / (float) maxLen * 1.61f)
                              : juce::jmin (ih * (n > 1 ? 0.36f : 0.42f),
                                            iw / (float) juce::jmax (6, c.lines[0].length()) * 1.7f);
    const float fs = c.textSize > 0.0f && n == 1
                   ? juce::jmin (c.textSize, iw / (float) juce::jmax (4, c.lines[0].length()) * 1.7f)
                   : derived;
    g.setColour (c.text);
    g.setFont (juce::Font (juce::FontOptions (lcd ? "Menlo" : juce::Font::getDefaultSansSerifFontName(),
                                              fs, juce::Font::bold)));
    for (int i = 0; i < n; ++i)
    {
        const float cy = lcd && lead > 0.0f
                       ? r.getCentreY() + ((float) i - (float) (n - 1) * 0.5f) * fs * lead
                       : (lcd && n > 2 ? r.getY() + ih * ((float) i + 0.5f) / (float) n
                                       : r.getY() + ih * (n > 1 ? 0.36f + 0.42f * (float) i : 0.5f));
        g.drawText (c.lines[i], r.withY (cy - fs * 0.6f).withHeight (fs * 1.2f),
                    juce::Justification::centred, false);
        if (! lcd && i == 0)
            g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultSansSerifFontName(),
                                                      juce::jmin (ih * 0.16f, 18.0f * k), juce::Font::plain)));
    }
}

/** WHAT THE WINDOW SHOWS UNTIL resources/native/ EXISTS: the machine's own controls at the
    spec's own coordinates, live and clickable, and an honest statement across the top of what
    is missing. Deliberately plain - anything prettier invites somebody to keep it, which is
    exactly how three units shipped a placeholder with fully green builds. */
void NativePanel::paintPlaceholder (juce::Graphics& g, float k)
{
    g.fillAll (ground);
    // The band runs the width of the PANEL, not of the window: on a panel that
    // scrolls those are different, and a band cut to the window slides off it.
    // At offset zero they are the same number to the pixel.
    const int cw = contentSize().x;
    const int bandH = juce::jmax (24, juce::roundToInt (34.0f * k));
    g.setColour (ground.brighter (0.18f));
    g.fillRect (0, 0, cw, bandH);
    g.setColour (ink);
    g.setFont (juce::Font (juce::FontOptions (juce::jmax (12.0f, bandH * 0.55f), juce::Font::bold)));
    g.drawText (panelTitle, 12, 0, cw - 24, bandH, juce::Justification::centredLeft);
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (juce::Font (juce::FontOptions (juce::jmax (10.0f, bandH * 0.38f), juce::Font::plain)));
    g.drawText ("panel artwork pending - npm run panel:native, then rebuild",
                12, 0, cw - 24, bandH, juce::Justification::centredRight);

    for (const auto& c : ctls)
    {
        const auto b = c.drawn.transformedBy (juce::AffineTransform::scale (k));
        if (c.kind == "screen")
        {
            g.setColour (juce::Colour (0xff0a0d12));
            g.fillRoundedRectangle (b, 4.0f);
            g.setColour (ink);
            auto inner = b.reduced (juce::jmax (3.0f, 8.0f * k));
            const auto lines = showingEdit ? editLines : restLines;
            const int n = juce::jmax (1, lines.size());
            for (int i = 0; i < lines.size(); ++i)
            {
                auto row = inner.withHeight (inner.getHeight() / (float) n)
                                .withY (inner.getY() + inner.getHeight() * (float) i / (float) n);
                g.setFont (squeezedToFit (juce::Font (juce::FontOptions (row.getHeight() * 0.78f,
                                                                         juce::Font::bold)),
                                          lines[i], row.getWidth()));
                g.drawText (lines[i], row, juce::Justification::centredLeft, false);
            }
            continue;
        }
        if (c.kind == "cap" || c.kind == "selCap" || c.kind == "toggle" || c.kind == "button")
        {
            g.setColour (c.lit ? ink.withAlpha (0.85f) : ground.brighter (0.22f));
            g.fillRoundedRectangle (b, 3.0f);
            g.setColour (c.lit ? juce::Colours::black : juce::Colours::white.withAlpha (0.8f));
            const auto label = c.label.replace ("\n", " ");
            g.setFont (squeezedToFit (juce::Font (juce::FontOptions (b.getHeight() * 0.5f, juce::Font::plain)),
                                      label, b.getWidth() - 4.0f));
            g.drawText (label, b, juce::Justification::centred, false);
            continue;
        }
        if (c.kind == "fader")
        {
            g.setColour (juce::Colour (0xff11141c));
            g.fillRoundedRectangle (b, 4.0f);
            const float t01 = juce::jlimit (0.0f, 1.0f, c.value);
            const float hs = juce::jmax (6.0f, 18.0f * k);
            const auto h = c.horizontal
                ? juce::Rectangle<float> (b.getX() + t01 * (b.getWidth() - hs), b.getY() - 2.0f,
                                          hs, b.getHeight() + 4.0f)
                : juce::Rectangle<float> (b.getX() - 2.0f, b.getY() + (1.0f - t01) * (b.getHeight() - hs),
                                          b.getWidth() + 4.0f, hs);
            g.setColour (juce::Colour (0xff8f97a6));
            g.fillRoundedRectangle (h, 2.0f);
            g.setColour (ink);
            g.fillRect (c.horizontal
                          ? juce::Rectangle<float> (h.getCentreX() - 1.0f, h.getY() + 2.0f, 2.0f, h.getHeight() - 4.0f)
                          : juce::Rectangle<float> (h.getX() + 2.0f, h.getCentreY() - 1.0f, h.getWidth() - 4.0f, 2.0f));
            continue;
        }
        if (c.kind == "led")
        {
            g.setColour (c.lit ? ink : ground.brighter (0.3f));
            g.fillEllipse (b);
            continue;
        }
        if (c.kind == "static") continue;
        // a knob: face, pointer, and the label the panel prints BENEATH it
        const auto centre = b.getCentre();
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        g.setColour (ground.brighter (0.22f));
        g.fillEllipse (b);
        g.setColour (ink.withAlpha (0.7f));
        g.drawEllipse (b.reduced (1.0f), juce::jmax (1.0f, 1.5f * k));
        const float a = juce::MathConstants<float>::pi * (0.75f + 1.5f * c.value);
        g.setColour (ink);
        g.drawLine (centre.x, centre.y, centre.x - std::sin (a) * r * 0.85f,
                    centre.y + std::cos (a) * r * 0.85f, juce::jmax (1.0f, 2.0f * k));
        if (c.label.isNotEmpty())
        {
            const auto label = c.label.replace ("\n", " ");
            const auto row = juce::Rectangle<float> (b.getX() - r, b.getBottom() + 1.0f,
                                                     b.getWidth() + r * 2.0f, juce::jmax (8.0f, r * 0.8f));
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.setFont (squeezedToFit (juce::Font (juce::FontOptions (juce::jmax (7.0f, r * 0.5f), juce::Font::plain)),
                                      label, row.getWidth()));
            g.drawText (label, row, juce::Justification::centredTop, false);
        }
    }
}

// --- the mouse -------------------------------------------------------------
int NativePanel::hitCtl (juce::Point<float> p) const
{
    for (int i = (int) ctls.size() - 1; i >= 0; --i)
    {
        const auto& c = ctls[(size_t) i];
        // A CONTROL ON A PAGE THAT IS NOT SHOWING IS NOT THERE. It has to be
        // tested here rather than only in paint, or the pages would stack in
        // hit order: three sets of knobs at one rect, and a click landing on
        // whichever page happened to be last in the layout.
        if (! visible (c)) continue;
        // WHILE A LAYER IS OPEN ONLY ITS CONTROLS TAKE THE MOUSE: the plate
        // under the scrim is out of reach, and a click off the layer closes it
        // (mouseDown) rather than landing on whatever was under the scrim.
        if (layerView >= 0 && ! onLayer (c)) continue;
        // A BAND CONTROL SCROLLED OUT OF ITS AREA IS NOT THERE: the area clips
        // it in paint, and a hit on it would be a hit on nothing visible.
        if (c.band.isNotEmpty())
            if (const auto* gp = groupOfBand (c.band); gp != nullptr && ! gp->rect.contains (p)) continue;
        if (! c.hit.isEmpty() && c.hit.contains (p)) return i;
    }
    return -1;
}

void NativePanel::mouseMove (const juce::MouseEvent& e)
{
    if (overBar (e.position) != 0)
    {
        hoverIdx = -1;
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        return;
    }
    const int i = hitCtl (toDesign (e.position));
    if (i != hoverIdx)
    {
        hoverIdx = i;
        // A PATH EDITOR IS DRAWN ON, so it gets a crosshair: the pointing hand
        // says "this is a thing you press once" and every gesture on this window
        // is a drag with a position that matters to the pixel.
        if (i >= 0 && (ctls[(size_t) i].kind == "pathEditor" || ctls[(size_t) i].kind == "stepGrid"))
            setMouseCursor (juce::MouseCursor::CrosshairCursor);
        else if (i >= 0) setMouseCursor (isValueCtl (ctls[(size_t) i]) ? juce::MouseCursor::UpDownResizeCursor
                                                                      : juce::MouseCursor::PointingHandCursor);
        // Bare panel on a machine bigger than its window is a HANDLE: it drags the
        // panel under the window, and the cursor is what says so.
        else setMouseCursor ((scrollsHorizontally() || scrollsVertically())
                               ? juce::MouseCursor::DraggingHandCursor
                               : juce::MouseCursor::NormalCursor);
    }
}

/** 0 for neither bar, 1 for the horizontal one, 2 for the vertical. Generous by a
    few pixels, because a 9 px bar is otherwise a test of aim. */
int NativePanel::overBar (juce::Point<float> p) const
{
    const auto h = barTrack (true);
    if (! h.isEmpty() && h.expanded (4.0f).contains (p)) return 1;
    const auto v = barTrack (false);
    if (! v.isEmpty() && v.expanded (4.0f).contains (p)) return 2;
    return 0;
}

/** Put the thumb's CENTRE where the pointer is - a click anywhere on the track goes
    straight there rather than paging towards it. */
void NativePanel::thumbTo (bool horizontal, float coord)
{
    const auto track = barTrack (horizontal), thumb = barThumb (horizontal);
    if (track.isEmpty()) return;
    const float span  = horizontal ? track.getWidth() : track.getHeight();
    const float len   = horizontal ? thumb.getWidth() : thumb.getHeight();
    const float start = horizontal ? track.getX() : track.getY();
    const float t01   = span > len ? juce::jlimit (0.0f, 1.0f, (coord - len * 0.5f - start) / (span - len))
                                   : 0.0f;
    if (horizontal) setViewOffset (juce::roundToInt (t01 * (float) maxViewX()), viewY);
    else            setViewOffset (viewX, juce::roundToInt (t01 * (float) maxViewY()));
}

void NativePanel::emitValue (Ctl& c)
{
    repaintCtl (c);
    if (! artwork) repaint();
    maybeReflowGate (c.id);
    if (onChange) { Event ev; ev.id = c.id; ev.hasValue = true; ev.value = c.value; ev.group = c.group; onChange (ev); }
}

void NativePanel::mouseDown (const juce::MouseEvent& e)
{
    dragIdx = -1; dragMoved = false; barDrag = 0; panning = false;

    // The bars are drawn ON TOP of the panel, so a click on one is theirs and never
    // reaches the control underneath.
    if (const int axis = overBar (e.position))
    {
        const bool horiz = axis == 1;
        barDrag = axis;
        if (! barThumb (horiz).contains (e.position)) thumbTo (horiz, horiz ? e.position.x : e.position.y);
        viewDragFrom = e.position; viewDragView = { viewX, viewY };
        repaint();
        return;
    }

    if (layerView >= 0 && ! layerRect.contains (toDesign (e.position))) { closeLayer(); return; }

    const int i = hitCtl (toDesign (e.position));
    // A PRESS ON A BAND'S GRIP starts a reorder drag - IN mouseDown, which is
    // where a press is. Both of these lived in mouseMove, so the drag was armed
    // by the pointer PASSING OVER a grip and a press on one armed nothing: the
    // strip drag worked only when a move event happened to land on the grip
    // first ("sometimes it works. sometimes it doesnt") and the CHAIN list's
    // handles never worked at all. The author, 2026-09-20: "I still cant Drag/reorder
    // the FX units in the left list."
    // A PRESS ON A BAND'S GRIP starts a reorder drag. The grip is the band's
    // name tile - its left kBandGripPx - and only where no control claimed the
    // point, so the tile's own lamp still clicks. Bands of another view are
    // not on screen and cannot be gripped.
    // A PRESS ON A HANDLE starts the same reorder drag from the LIST: the band
    // it names moves through the order as the pointer crosses the other rows.
    if (i >= 0 && ! e.mods.isPopupMenu() && ctls[(size_t) i].kind == "handle")
    {
        const auto& want = ctls[(size_t) i].listOf;
        for (size_t gi = 0; gi < bandGroups.size(); ++gi)
            for (size_t bi = 0; bi < bandGroups[gi].bands.size(); ++bi)
                if (bandGroups[gi].bands[bi].id == want)
                {
                    bandDragGroup = (int) gi; bandDragBand = (int) bi; bandDragging = false;
                    listDrag = true; bandPressAt = e.position;
                    repaint();
                    return;
                }
    }
    if (i < 0 && bandGripEnabled && ! e.mods.isPopupMenu())
    {
        const auto d = toDesign (e.position);
        for (size_t gi = 0; gi < bandGroups.size(); ++gi)
        {
            const auto& gp = bandGroups[gi];
            if (gp.view >= 0 && pageOf (gp.viewSection) != gp.view) continue;
            for (const int bi : gp.order)
            {
                const auto& b = gp.bands[(size_t) bi];
                if (! b.shown) continue;
                const auto r = b.rect.translated (0.0f, b.dy);
                // ONLY THE GRIP STRIP, not the band's left third: a knob near
                // the left edge used to start a reorder whenever the press
                // landed in the gap between two controls.
                if (r.contains (d) && d.x < r.getX() + kBandGripPx)
                {
                    bandDragGroup = (int) gi; bandDragBand = bi; bandDragging = false; bandPressAt = e.position;
                    return;
                }
            }
        }
    }
    // A PAGE SELECT DROPS DOWN, because it is drawn as a dropdown.
    //
    // It advanced the page on a click at first, on the argument that two or
    // three pages named in the window do not need a menu. The author, using it: *"none
    // of the dropdowns actually 'dropdown' they just change page."* He is right
    // and the argument was beside the point: the control has a caret on it, and
    // a caret is a promise about what a click does. A control that looks like
    // one thing and behaves like another is worse than either, and the list is
    // also the only place a player can SEE what the other pages are called
    // without visiting them one at a time.
    //
    // The menu opens under the window, with the current page ticked, and the
    // scroll wheel still steps through the pages for anyone who preferred that.
    // A TAB STRIP IS CLICKED WHERE THE TAB IS. No menu: the strip is already the
    // list, so opening one over it would be asking twice.
    if (i >= 0 && ctls[(size_t) i].kind == "tabStrip")
    {
        const auto& c = ctls[(size_t) i];
        const int n = c.pageNames.size();
        if (n > 0 && c.rect.getWidth() > 0.0f)
        {
            const float u = (toDesign (e.position).x - c.rect.getX()) / c.rect.getWidth();
            const int cell = juce::jlimit (0, n - 1, (int) (u * (float) n));
            // A RIGHT-CLICK IS A QUESTION ABOUT THE TAB, NOT A USE OF IT. It is
            // reported and nothing else happens: no tab switch, no drag. The author:
            // "should be able to right click a tab like in serum and remove
            // connections." The unit builds the menu, since it knows the matrix.
            if (e.mods.isPopupMenu())
            {
                if (onTabMenu) onTabMenu (c.id, cell, e.getScreenPosition());
                return;
            }
            // A STRIP WHOSE HOST TAKES DROPS WAITS FOR THE RELEASE. The author: *"keep
            // the mouse button down on one of the 6 lfo tabs and drag them to
            // any control I want that lfo to manipulate."* The press cannot know
            // yet whether it is a click or the start of that drag, so it is
            // remembered and decided in mouseUp: released where it landed, it
            // switches the tab exactly as the line below does; moved first, it
            // is a drag and the tab stays where it was. With no host wired the
            // press switches the tab here, as it always has.
            // AND A STRIP THAT IS NOT A SOURCE NEVER ARMS THE DRAG. A filter tab
            // selects a page and nothing else; arming the gesture on it would
            // let a player drag it onto a control and watch nothing happen,
            // which is the promise a ring makes and this one cannot keep.
            if (onModDrop && c.assignable)
            {
                tabPressIdx = i; tabPressCell = cell; tabPressAt = e.position;
                // WHICH HANDLE. On a two-handle cell the Y grip is its own
                // source; everywhere else on the cell, the X grip included, is
                // the first one - so a tab with one handle behaves as it always
                // did and a press on its name still drags its one source.
                tabPressHandle = tabHandleAt (c, cell, toDesign (e.position));
                tabDragging = false; tabDropIdx = -1;
                return;
            }
            setPage (c.selects.isNotEmpty() ? c.selects : c.pageSection, c.selectBase + cell);
            repaint();
        }
        return;
    }
    if (i >= 0 && ctls[(size_t) i].kind == "pageSelect")
    {
        const auto& c = ctls[(size_t) i];
        const auto section = c.pageSection;
        const int now = pageOf (section);
        juce::PopupMenu menu;
        for (int n = 0; n < c.pageNames.size(); ++n)
            menu.addItem (n + 1, c.pageNames[n], true, n == now);
        // Under the control, at the control's own width, so it reads as the
        // window opening rather than as a menu appearing near the pointer.
        const auto onScreen = localAreaToGlobal (c.rect.transformedBy (
            juce::AffineTransform::scale (scale())
                .translated ((float) (centringOffset().x - viewX), (float) (centringOffset().y - viewY))).toNearestInt());
        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withTargetScreenArea (onScreen)
                                .withMinimumWidth (onScreen.getWidth())
                                .withStandardItemHeight (24),
                            [this, section] (int chosen)
                            { if (chosen > 0) setPage (section, chosen - 1); });
        return;
    }
    if (i < 0)
    {
        // Nothing under the pointer: drag the panel itself. On a panel that fits,
        // maxView is zero on both axes and this does nothing at all.
        if (scrollsHorizontally() || scrollsVertically())
        { panning = true; viewDragFrom = e.position; viewDragView = { viewX, viewY }; }
        return;
    }
    auto& c = ctls[(size_t) i];
    // A POINT ON A DISPLAY IS GRABBED BEFORE ANYTHING ELSE about that display.
    // The grab radius is generous - twice what is drawn - because these are
    // small targets on a window that is otherwise inert, so there is nothing for
    // a near miss to hit by mistake.
    if (c.kind == "scope" && handleSource)
    {
        const auto d = toDesign (e.position);
        const auto r = c.rect.isEmpty() ? juce::Rectangle<float> (c.x, c.y, c.w, c.h) : c.rect;
        const int n = handleSource (c.id, handleBuf.data(), (int) handleBuf.size());
        // NEAREST POINT WINS, EACH WITHIN ITS OWN REACH. A macro point is drawn
        // bigger, so it is grabbed from further away; without the scale here a
        // small point sitting under it would steal every press near its edge.
        // The comparison is the distance as a FRACTION of the point's reach, so
        // "which one did they mean" stays a fair question between sizes.
        float best = 1.0f; int pick = -1;
        for (int m = 0; m < n; ++m)
        {
            const float reach = kHandleRadius * 2.6f
                              * juce::jlimit (0.5f, 6.0f, handleBuf[(size_t) m].scale);
            const float dist = handlePoint (r, handleBuf[(size_t) m]).getDistanceFrom (d) / reach;
            if (dist < best) { best = dist; pick = m; }
        }
        if (pick >= 0)
        {
            dragScope = c.id; dragHandle = handleBuf[(size_t) pick].id;
            if (onHandleGrab) onHandleGrab (dragScope, dragHandle, true);
            repaintDisplays();
            return;
        }
    }
    // A PATH EDITOR TAKES THE WHOLE CLICK. It is not a value control and it is
    // not a cap, so without this it would fall through to the press path at the
    // bottom of this function and every click on the glass would emit a lit
    // toggle for an id that has no lamp.
    if (c.kind == "pathEditor")
    {
        // The return is unconditional and the gesture is not: a machine whose
        // path source is not wired yet still must not have its glass emit a cap
        // press, which is a dead control reporting as a live one.
        if (pathSource) pathBeginGesture (c, toDesign (e.position), e.mods);
        return;
    }
    // A STEP GRID TAKES THE WHOLE CLICK, for the same reason and with the same
    // unconditional return: a machine whose grid source is not wired yet must
    // not have its glass emit a cap press for an id that has no lamp.
    if (c.kind == "stepGrid")
    {
        if (gridSource) gridBeginGesture (c, toDesign (e.position), e.mods);
        return;
    }
    // A LIST TAKES THE WHOLE CLICK, same rule and same unconditional return: a
    // machine whose list source is not wired yet must not have its glass emit a
    // cap press for an id that has no lamp. A click on a heading or on empty
    // glass reports nothing, because neither is a row anybody chose.
    if (c.kind == "list")
    {
        const int row = listRowAt (c, toDesign (e.position));
        if (row >= 0 && onListRow) onListRow (c.id, row);
        return;
    }
    // A DROPDOWN DROPS DOWN. Same rule the page select already follows: the
    // caret is a promise about what a click does, and the list is also the only
    // place a player can SEE the other positions without visiting them one at a
    // time. A readout with no choices is a window that is pressed, and falls
    // through to the press path below.
    if (c.kind == "readout" && c.choices.size() > 1)
    {
        const int n = c.choices.size();
        // THE TICK COMES FROM WHOEVER OWNS THE LIST. A time window's own value is
        // milliseconds, so ticking from it would put the check mark on a division
        // nobody selected - read the target's value when there is one.
        float from = c.value;
        if (c.menuTarget.isNotEmpty())
            for (const auto& q : ctls) if (q.id == c.menuTarget) { from = q.value; break; }
        const int now = juce::jlimit (0, n - 1, (int) std::lround (from * (float) (n - 1)));
        juce::PopupMenu menu;
        for (int m = 0; m < n; ++m) menu.addItem (m + 1, c.choices[m], true, m == now);
        // Under the window, at the window's own width, so it reads as the
        // control opening rather than as a menu appearing near the pointer.
        const auto onScreen = localAreaToGlobal (c.rect.transformedBy (
            juce::AffineTransform::scale (scale())
                .translated ((float) (centringOffset().x - viewX), (float) (centringOffset().y - viewY))).toNearestInt());
        const auto id = c.id;
        const auto target = c.menuTarget;
        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withTargetScreenArea (onScreen)
                                .withMinimumWidth (onScreen.getWidth())
                                .withStandardItemHeight (22),
                            [this, id, target, n] (int chosen)
                            {
                                if (chosen <= 0) return;
                                // The list writes whatever control OWNS it. For a
                                // time window that is the MODE parameter, whose own
                                // display carries the same positions, so picking a
                                // division on any of the four windows moves MODE and
                                // every window re-reads through it.
                                const auto& wanted = target.isNotEmpty() ? target : id;
                                const float v = n > 1 ? (float) (chosen - 1) / (float) (n - 1) : 0.0f;
                                for (auto& q : ctls)
                                    if (q.id == wanted)
                                    {
                                        q.value = v;
                                        emitValue (q);
                                        return;
                                    }
                                // A TARGET THE PANEL DOES NOT DRAW is still a
                                // parameter the host can write. The four delay
                                // time windows open their own division lists,
                                // and a division has no control of its own -
                                // the window IS its control. Without this the
                                // pick vanished silently, which is the same
                                // dead-dropdown symptom one layer down.
                                if (onChange)
                                {
                                    Event ev; ev.id = wanted; ev.hasValue = true; ev.value = v;
                                    onChange (ev);
                                }
                            });
        return;
    }
    // A KEY IS PLAYED, NOT PRESSED. Without this it would fall through to the
    // press path below and report a lit cap for an id no binding names - a
    // keyboard that emits sixty-one dead toggles instead of notes. The note is
    // held for the whole gesture, so mouseDrag continues it and mouseUp ends it.
    if (c.kind == "key" && c.midi >= 0)
    {
        keyGesture (toDesign (e.position));
        return;
    }
    if (isValueCtl (c)) { dragIdx = i; dragStartValue = c.value; dragStart = e.position; return; }
    if (onChange)
    {
        Event ev; ev.id = c.id; ev.group = c.group;
        if (c.kind == "selCap")
            ev.active = true;
        else if (c.kind == "toggle" && c.positions > 2)
        {
            // low -> centre -> high -> low, the way a lever goes when you keep flicking it
            // the same way.
            c.pos = (c.pos + 1) % c.positions;
            c.lit = c.pos == c.positions - 1;
            ev.hasLit = true; ev.lit = c.lit;
            ev.hasValue = true; ev.value = (float) c.pos / (float) (c.positions - 1);
            repaintCtl (c);
        }
        else { ev.hasLit = true; ev.lit = ! c.lit; }
        onChange (ev);
    }
}

void NativePanel::mouseDrag (const juce::MouseEvent& e)
{
    if (bandDragGroup >= 0)
    {
        if (! bandDragging && e.position.getDistanceFrom (bandPressAt) < 6.0f) return;
        bandDragging = true;
        auto& gp = bandGroups[(size_t) bandDragGroup];
        const float y = toDesign (e.position).y;
        // WHERE THE POINTER IS IN THE STACK: the position among the SHOWN bands
        // whose stacked centre is nearest. Hidden bands keep their place in the
        // order and are stepped over, so a drag never reveals one.
        int cur = -1, target = -1; float best = 1.0e9f;
        for (int k = 0; k < (int) gp.order.size(); ++k)
        {
            const auto& b = gp.bands[(size_t) gp.order[(size_t) k]];
            if (gp.order[(size_t) k] == bandDragBand) cur = k;
            float cy = 0.0f;
            if (listDrag)
            {
                // FROM THE LIST: the rows are the slots, every band has one,
                // shown or not - the list names them all.
                const auto y0 = listY0.find (b.id);
                if (y0 == listY0.end()) continue;
                cy = y0->second + (listDy.count (b.id) ? listDy[b.id] : 0.0f);
            }
            else
            {
                if (! b.shown) continue;
                cy = b.rect.translated (0.0f, b.dy).getCentreY();
            }
            if (std::abs (y - cy) < best) { best = std::abs (y - cy); target = k; }
        }
        if (cur >= 0 && target >= 0 && target != cur)
        {
            const int moving = gp.order[(size_t) cur];
            gp.order.erase (gp.order.begin() + cur);
            gp.order.insert (gp.order.begin() + target, moving);
            reflowBands();
            reflowLists();
            repaint();
        }
        return;
    }
    if (tabPressIdx >= 0)
    {
        // NOT A DRAG UNTIL THE HAND HAS MOVED. Under kTabDragPx the press is
        // still the click it will become on release; past it the tab switch is
        // off the table for this gesture, whatever happens next.
        if (! tabDragging && e.position.getDistanceFrom (tabPressAt) < kTabDragPx) return;
        const auto oldLabel = tabLabelBounds();
        const int oldDrop = tabDropIdx;
        tabDragging = true;
        tabDragPos = e.position;
        // WHAT IS UNDER THE POINTER, and whether it may take the drop: a value
        // control that is not the strip itself and that the host does not refuse.
        // [MAN] p. 194: "The + sign indicates that you are over a valid mod
        // destination." A control the host has no destination for shows no +.
        const int over = hitCtl (toDesign (e.position));
        tabDropIdx = over >= 0 && over != tabPressIdx && isValueCtl (ctls[(size_t) over])
                     && (! canDrop || canDrop (ctls[(size_t) over].id)) ? over : -1;
        // Repaint the label where it was and where it is, and the two controls
        // whose + changed - not the plate.
        repaint (oldLabel);
        repaint (tabLabelBounds());
        if (oldDrop != tabDropIdx)
        {
            if (oldDrop >= 0)    repaintCtl (ctls[(size_t) oldDrop]);
            if (tabDropIdx >= 0) repaintCtl (ctls[(size_t) tabDropIdx]);
        }
        setMouseCursor (tabDropIdx >= 0 ? juce::MouseCursor::CopyingCursor
                                        : juce::MouseCursor::DraggingHandCursor);
        return;
    }
    // A HELD POINT FOLLOWS THE POINTER, reported as a normalised position on its
    // own display with y UP. The panel moves nothing itself: the host writes the
    // parameters and the next repaint asks where the point is now, so a point
    // and the knob it shares a parameter with cannot drift apart.
    if (dragHandle.isNotEmpty())
    {
        for (const auto& c : ctls)
            if (c.id == dragScope)
            {
                const auto r = c.rect.isEmpty() ? juce::Rectangle<float> (c.x, c.y, c.w, c.h) : c.rect;
                const auto d = toDesign (e.position);
                const auto in = r.reduced (kHandleRadius + 1.0f);
                if (onHandleDrag && in.getWidth() > 0.0f && in.getHeight() > 0.0f)
                    onHandleDrag (dragScope, dragHandle,
                                  juce::jlimit (0.0f, 1.0f, (d.x - in.getX()) / in.getWidth()),
                                  juce::jlimit (0.0f, 1.0f, 1.0f - (d.y - in.getY()) / in.getHeight()));
                break;
            }
        repaintDisplays();
        return;
    }
    // A PATH IS BEING DRAWN. The panel computes the new list and hands the whole
    // of it to the host; it stores nothing, so the next repaint shows what the
    // patch actually kept rather than what the pointer asked for.
    if (dragPath.isNotEmpty())
    {
        if (auto* c = pathCtl (dragPath)) pathDragGesture (*c, toDesign (e.position), e.mods);
        return;
    }
    // A PATTERN IS BEING PAINTED, and the same contract: the panel works out the
    // new list and hands the whole of it over, keeping no copy of its own.
    if (dragGrid.isNotEmpty())
    {
        if (auto* c = gridCtl (dragGrid)) gridDragTo (*c, toDesign (e.position));
        return;
    }
    // A FINGER ALONG THE KEYBED. The hand is holding a note, so every move asks
    // one question - which key is under the pointer now - and keyGesture does
    // the rest. It runs before the bar and pan branches because a gesture that
    // began on a key stays a gesture on the keys until it is released, whatever
    // it is dragged over.
    if (noteHeld >= 0) { keyGesture (toDesign (e.position)); return; }
    if (barDrag != 0)
    {
        const bool horiz = barDrag == 1;
        const auto track = barTrack (horiz), thumb = barThumb (horiz);
        const float span = horiz ? track.getWidth() : track.getHeight();
        const float len  = horiz ? thumb.getWidth() : thumb.getHeight();
        const float d    = horiz ? e.position.x - viewDragFrom.x : e.position.y - viewDragFrom.y;
        const float perPx = span > len ? (float) (horiz ? maxViewX() : maxViewY()) / (span - len) : 0.0f;
        const int moved = juce::roundToInt (d * perPx);
        setViewOffset (horiz ? viewDragView.x + moved : viewDragView.x,
                       horiz ? viewDragView.y : viewDragView.y + moved);
        return;
    }
    if (panning)
    {
        // The panel follows the hand, so dragging LEFT reveals what is to the right.
        setViewOffset (viewDragView.x - juce::roundToInt (e.position.x - viewDragFrom.x),
                       viewDragView.y - juce::roundToInt (e.position.y - viewDragFrom.y));
        return;
    }
    if (dragIdx < 0) return;
    auto& c = ctls[(size_t) dragIdx];
    const float range = (c.kind == "fader" ? 160.0f : 200.0f) * scale();
    const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
    // A CONTROL DRAGS ALONG ITS OWN AXIS. Both axes were summed, so a
    // side-to-side fader also answered an up-down drag and a knob answered a
    // sideways one - which reads as the control fighting the pointer. The author,
    // 2026-09-22, on EchoFold's FEEDBACK and FB-PAN faders: "the SLIDERS that
    // go side to side. can we do the mouse cursor is SIDE-TO-SIDE instead of
    // UP-DOWN". A horizontal fader takes X, everything else takes Y, and
    // getMouseCursor below says which before the drag starts.
    const float travel = (c.kind == "fader" && c.horizontal)
                           ? (e.position.x - dragStart.x)
                           : (dragStart.y - e.position.y);
    const float v = juce::jlimit (0.0f, 1.0f, dragStartValue + travel / range * fine);
    if (std::abs (v - c.value) > 1e-6f) { dragMoved = true; c.value = v; emitValue (c); }
}

void NativePanel::mouseUp (const juce::MouseEvent&)
{
    // A SPRUNG CONTROL LETS GO, before anything clears the drag: the control being
    // dragged is the only one that can have been released.
    if (dragIdx >= 0 && sprungIds.contains (ctls[(size_t) dragIdx].id))
    {
        auto& sc = ctls[(size_t) dragIdx];
        if (std::abs (sc.value - sc.defaultValue) > 1.0e-6f)
        {
            sc.value = sc.defaultValue;
            if (onChange) { Event ev; ev.id = sc.id; ev.group = sc.group;
                            ev.hasValue = true; ev.value = sc.value; onChange (ev); }
            repaint();
        }
    }

    if (bandDragGroup >= 0)
    {
        const auto& gp = bandGroups[(size_t) bandDragGroup];
        const bool dragged = bandDragging;
        juce::StringArray ids;
        for (const int bi : gp.order) ids.add (gp.bands[(size_t) bi].id);
        const juce::String section = gp.section;
        bandDragGroup = -1; bandDragBand = -1; bandDragging = false; listDrag = false;
        repaint();
        if (dragged && onBandOrder) onBandOrder (section, ids);
        return;
    }
    if (tabPressIdx >= 0)
    {
        const int strip = tabPressIdx, cell = tabPressCell, drop = tabDropIdx, handle = tabPressHandle;
        const bool dragged = tabDragging;
        endTabDrag();
        const auto& c = ctls[(size_t) strip];
        if (! dragged)
        {
            // THE CLICK. Never moved past kTabDragPx, so this is the tab switch
            // mouseDown used to make, made on the release instead.
            setPage (c.selects.isNotEmpty() ? c.selects : c.pageSection, c.selectBase + cell);
            repaint();
            return;
        }
        // THE DROP. [MAN] p. 194: "When you release the mouse button, Serum
        // automatically makes the connection." Released over nothing that takes
        // it, nothing happens - and the tab has not switched either, because a
        // gesture that moved was never a click.
        if (drop >= 0 && onModDrop) onModDrop (c.id, cell, handle, ctls[(size_t) drop].id);
        return;
    }
    if (dragHandle.isNotEmpty())
    {
        if (onHandleGrab) onHandleGrab (dragScope, dragHandle, false);
        dragScope = {}; dragHandle = {}; repaintDisplays(); return;
    }
    if (dragGrid.isNotEmpty()) { gridEndGesture(); return; }
    if (dragPath.isNotEmpty()) { pathEndGesture(); return; }
    if (noteHeld >= 0) { keyRelease(); return; }
    if (barDrag != 0 || panning) { barDrag = 0; panning = false; repaint(); return; }
    // A READOUT THAT SHOWS A MODE is pressed, not turned. A click that never became a drag
    // emits a press (no value), which is exactly what a cap sends, so the editor's existing
    // step-on-press path handles it and a readout with no step behaviour ignores it.
    if (dragIdx >= 0 && ! dragMoved && ctls[(size_t) dragIdx].kind == "readout" && onChange)
    { Event ev; ev.id = ctls[(size_t) dragIdx].id; ev.group = ctls[(size_t) dragIdx].group; onChange (ev); }
    dragIdx = -1; dragMoved = false;
}

/** The label's box: to the lower right of the pointer, so the hand does not
    cover it, sized to the name in the strip's own type. Empty when no drag. */
juce::Rectangle<int> NativePanel::tabLabelBounds() const
{
    if (! tabDragging || tabPressIdx < 0) return {};
    const auto& c = ctls[(size_t) tabPressIdx];
    const auto name = tabDragName();
    const float size = juce::jmax (11.0f, c.textSize * scale() * 1.15f);
    const juce::Font f (juce::FontOptions (size, juce::Font::bold));
    const float w = juce::GlyphArrangement::getStringWidth (f, name) + size * 1.2f;
    const float h = size * 1.7f;
    return juce::Rectangle<float> (tabDragPos.x + 14.0f, tabDragPos.y + 8.0f, w, h)
             .getSmallestIntegerContainer().expanded (2);
}

/** THE DRAG, DRAWN: the source's name riding beside the pointer as a lit pill in
    the strip's own colours, and a + on the control that will take it. [MAN]
    Serum 2 user guide p. 194: "an LFO 2 label hovers next to the mouse pointer.
    The pointer adds a + sign as you hover over an assignable knob." */
void NativePanel::paintModDrag (juce::Graphics& g)
{
    if (! tabDragging || tabPressIdx < 0) return;
    const auto& c = ctls[(size_t) tabPressIdx];
    // "LFO 2 Y" when the Y grip is what is being dragged, so the label says
    // which of the two sources will be connected - [MAN] p. 194's hovering
    // label, with the half of the tab that was taken.
    const auto name = tabDragName();
    const auto box = tabLabelBounds().reduced (2).toFloat();
    const float rad = box.getHeight() * 0.5f;
    const auto fill = c.pill ? c.fillLit : c.fillLit.withAlpha (0.92f);
    const auto edge = c.pill ? c.glowLit : c.text;
    const auto nameInk = c.pill ? c.textLit : c.text;
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (box.translated (1.0f, 2.0f), rad);
    g.setColour (fill);
    g.fillRoundedRectangle (box, rad);
    g.setColour (edge.withAlpha (0.7f));
    g.drawRoundedRectangle (box.reduced (0.5f), rad, 1.0f);
    g.setColour (nameInk);
    g.setFont (juce::Font (juce::FontOptions (juce::jmax (11.0f, c.textSize * scale() * 1.15f),
                                              juce::Font::bold)));
    g.drawText (name, box.toNearestInt(), juce::Justification::centred, false);

    if (tabDropIdx < 0) return;
    const auto& t = ctls[(size_t) tabDropIdx];
    // The target's centre, design to component: a knob is centred on x, y and a
    // window on its rect.
    const auto centreD = t.rect.isEmpty() ? juce::Point<float> (t.x, t.y) : t.rect.getCentre();
    const auto ctr = centreD.transformedBy (juce::AffineTransform::scale (scale())
                                              .translated ((float) (centringOffset().x - viewX), (float) (centringOffset().y - viewY)));
    const float r = juce::jmax (7.0f, 6.0f * scale());
    g.setColour (edge.withAlpha (0.9f));
    g.fillEllipse (ctr.x - r, ctr.y - r, 2.0f * r, 2.0f * r);
    g.setColour (juce::Colours::white);
    const float arm = r * 0.55f, thick = juce::jmax (1.5f, r * 0.22f);
    g.fillRect (ctr.x - arm, ctr.y - thick * 0.5f, 2.0f * arm, thick);
    g.fillRect (ctr.x - thick * 0.5f, ctr.y - arm, thick, 2.0f * arm);
}

/** Forget the gesture and repaint what it drew. The cursor goes back to plain
    and hoverIdx is dropped, so the next mouseMove sets it for whatever is
    actually under the hand now rather than for the strip it left. */
void NativePanel::endTabDrag()
{
    const auto label = tabLabelBounds();
    const int drop = tabDropIdx;
    const bool wasDragging = tabDragging;
    tabPressIdx = -1; tabPressCell = -1; tabPressHandle = 0; tabDragging = false; tabDropIdx = -1;
    if (! wasDragging) return;
    repaint (label);
    if (drop >= 0) repaintCtl (ctls[(size_t) drop]);
    hoverIdx = -1;
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

/** DOUBLE-CLICK ON A PATH WINDOW: add or remove a point. One function because
    two surfaces call it - the glass on the plate and the pop-out canvas, which
    is the same path at a larger scale ([MAN] Serum 2 guide p. 194, "LFO Editor
    featuring a larger canvas"). `d` is in design units of the Ctl passed in. */
void NativePanel::pathDoubleClick (Ctl& c, juce::Point<float> d)
{
    std::vector<PathPoint> pts;
    if (pathFetch (c, pts) >= 2)
    {
        int hit = -1; float best = c.pointR * 2.4f;
        for (size_t q = 0; q < pts.size(); ++q)
        {
            const float dist = pathToDesign (c, pts[q].x, pts[q].y).getDistanceFrom (d);
            if (dist < best) { best = dist; hit = (int) q; }
        }
        const bool figure = c.pathMode == kPathFigure;
        // A GRAPH KEEPS ITS TWO ENDS - they are phase 0 and phase 1 - and a
        // FIGURE may lose any point but must keep two, or there is no line.
        const bool removable = figure ? (hit >= 0 && pts.size() > 2)
                                      : (hit > 0 && hit + 1 < (int) pts.size());
        if (c.pathMode == kPathTrace) { pathEndGesture(); return; }
        if (removable)
        {
            pts.erase (pts.begin() + hit);
            pathSel.clear();                 // every index after it has just moved
            pathEmit (c, pts);
        }
        else if (hit < 0 && (int) pts.size() < kMaxPathPoints)
        {
            const auto v = pathFromDesign (c, d);
            size_t seg = 0;
            if (figure)
            {
                // INTO THE NEAREST SEGMENT, by distance from the click to
                // the segment's middle: on a figure the click's x says
                // nothing about where in the list it belongs.
                float best = 1.0e9f;
                for (size_t q = 0; q + 1 < pts.size(); ++q)
                {
                    const auto m = figurePoint (pts[q], pts[q + 1], 0.5f);
                    const float dist = pathToDesign (c, m.x, m.y).getDistanceFrom (d);
                    if (dist < best) { best = dist; seg = q; }
                }
            }
            else
                while (seg + 2 < pts.size() && pts[seg + 1].x <= v.x) ++seg;
            // THE NEW POINT INHERITS THE BEND IT SPLITS. Dropping a point
            // into a curved segment and having both halves come out straight
            // is a redraw of the shape, not an addition to it.
            pts.insert (pts.begin() + (long) seg + 1, { v.x, v.y, pts[seg].curve });
            pathSel.clear();
            pathEmit (c, pts);
        }
    }
    pathEndGesture();
}

void NativePanel::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (overBar (e.position) != 0) return;
    const int i = hitCtl (toDesign (e.position)); if (i < 0) return;
    auto& c = ctls[(size_t) i];
    // `[MAN]` Serum 2 user guide p. 190, Double-click: "Add or remove points."
    //
    // JUCE delivers down-up-down-doubleClick-up, so the second press has already
    // started a gesture by the time this arrives: the edit is made here and the
    // gesture is ended here, and the mouseUp that follows finds nothing to do.
    if (c.kind == "pathEditor") { pathDoubleClick (c, toDesign (e.position)); return; }
    if (isValueCtl (c)) { c.value = c.defaultValue; emitValue (c); }
}

void NativePanel::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    // THE RACK UNDER THE POINTER SCROLLS FIRST. A band group taller than its
    // area takes the wheel; everything else falls through to the viewport.
    {
        const auto p = toDesign (e.position);
        for (auto& gp : bandGroups)
        {
            if (! bandGroupShowing (gp) || ! gp.rect.contains (p) || gp.stackH <= gp.rect.getHeight() + 0.5f) continue;
            const float step = (w.isSmooth ? 260.0f : 90.0f);
            gp.scroll = juce::jlimit (0.0f, gp.stackH - gp.rect.getHeight(), gp.scroll - w.deltaY * step);
            reflowBands();
            repaint();
            return;
        }
    }
    const int i = overBar (e.position) != 0 ? -1 : hitCtl (toDesign (e.position));
    // THE WHEEL STEPS A PAGE SELECT, which is what the control under a pointer
    // is expected to do and is faster than the menu when you know where you are
    // going. The menu is what the caret promises on a CLICK; this is the same
    // control answering the other gesture.
    if (i >= 0 && ctls[(size_t) i].kind == "pageSelect")
    {
        const auto& c = ctls[(size_t) i];
        setPage (c.pageSection, pageOf (c.pageSection) + (w.deltaY > 0 ? -1 : 1));
        return;
    }
    // AND IT SCROLLS A LIST, which is the whole reason a list is a control and
    // not a printed table. ONE COLUMN AT A TIME when the rows flow down columns:
    // three rows of a newspaper layout shuffles every column and reads as noise,
    // where a column is the unit a reader's eye is already using.
    if (i >= 0 && ctls[(size_t) i].kind == "list")
    {
        const auto& c = ctls[(size_t) i];
        const int step = c.columns > 1 ? juce::jmax (1, c.rows) : 3;
        setListScroll (c.id, c.listTop + (w.deltaY > 0 ? -step : step));
        return;
    }
    if (i >= 0 && isValueCtl (ctls[(size_t) i]))
    {
        // THE CONTROL WINS over the panel. A knob under the pointer is the gesture the
        // hardware has, and losing it to a scroll would be a worse trade than any
        // amount of convenience.
        auto& c = ctls[(size_t) i];
        const float step = e.mods.isShiftDown() ? 0.005f : 0.02f;
        c.value = juce::jlimit (0.0f, 1.0f, c.value + (w.deltaY > 0 ? step : -step));
        emitValue (c);
        return;
    }
    const bool sx = scrollsHorizontally(), sy = scrollsVertically();
    if (! sx && ! sy) return;             // a panel that fits: exactly as it always was

    constexpr float kWheelPx = 90.0f;
    float dx = -w.deltaX * kWheelPx, dy = -w.deltaY * kWheelPx;
    if (e.mods.isShiftDown()) std::swap (dx, dy);
    // A one-axis wheel on a one-axis panel should still move it, whichever way it turns.
    if (sx && ! sy && std::abs (dx) < std::abs (dy)) { dx = dy; dy = 0.0f; }
    if (sy && ! sx && std::abs (dy) < std::abs (dx)) { dy = dx; dx = 0.0f; }
    setViewOffset (viewX + juce::roundToInt (dx), viewY + juce::roundToInt (dy));
}

} // namespace synthkit
