// THE ONE PANEL CONSUMER. Every unit's front panel is drawn by this class and
// no unit carries a copy of it.
//
// The picture itself belongs to ~/projects/synthpanel-creator: `npm run
// panel:native` turns panel/panel.json into resources/native/{background.png
// (all the printed matter), sprites/*.png (the moving parts), layout.json
// (where everything is, what it looks like, how it moves)}. This class draws
// nothing of its own invention - it blits what the package rendered and turns
// the pointer on a knob. A fix to how a panel LOOKS goes in the package.
//
// IT ALSO DRAWS THE PANEL BEFORE THE ARTWORK EXISTS. A machine is decoded long
// before its panel is authored, so when there is no native export this falls
// back to panel/panel.json - the same spec - and lays plain shapes out at the
// spec's own coordinates, saying plainly on its own face that the artwork is
// pending. Same ids, same events, same hit areas, same probes: nothing in the
// plugin knows which of the two it got, and the day the export appears the
// panel becomes the real one with no code change at all.
//
// This exists because it used to be FORTY per-unit copies that had diverged,
// and because three units shipped the placeholder with fully green builds -
// a green build says nothing about a picture nobody looked at (playbook 16.0).
// The merge is recorded in ~/projects/synthkit/MIGRATION.md.
#pragma once
#include <array>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <map>
#include <vector>

namespace synthkit {

/** Where the panel's bytes come from.

    A unit's assets live in ITS OWN `BinaryData`, which is a per-target symbol
    this shared library cannot see, so the unit hands the panel its three
    accessors instead of the panel reaching into a namespace that exists in one
    build only. Fill it with SYNTHKIT_PANEL_ASSETS() after including BinaryData.h. */
struct PanelAssets
{
    const char* const* names = nullptr;   // BinaryData::namedResourceList
    int count = 0;                        // BinaryData::namedResourceListSize
    const char* (*get) (const char*, int&) = nullptr;   // BinaryData::getNamedResource
};

#define SYNTHKIT_PANEL_ASSETS() \
    ::synthkit::PanelAssets { BinaryData::namedResourceList, \
                              BinaryData::namedResourceListSize, \
                              &BinaryData::getNamedResource }

class NativePanel : public juce::Component, private juce::Timer
{
public:
    /** What a control did. A knob drag carries a value; a cap click does not;
        a lamp carries a lit state; a selector cap carries `active`. The first
        three fields are the placeholder's whole event, so editor code written
        against that keeps compiling unchanged. */
    struct Event
    {
        juce::String id;
        bool  hasValue = false; float value = 0.0f;    // knobs, faders, readouts
        bool  hasLit   = false; bool  lit   = false;   // caps, leds
        bool  active   = false;                        // selector caps
        juce::String group;
    };
    std::function<void (const Event&)> onChange;

    /** WHERE A DISPLAY'S SAMPLES COME FROM, and the seam that keeps this file
        ignorant of what a machine is.

        The panel knows a display exists, where it is and what colour its trace
        should be. It does not know what "osc2" means - the machine does. So a
        unit hands over one function: given the source name the spec printed on
        that window, fill the buffer and say how many samples went in. A unit
        that has no displays never calls this and nothing here changes.

        Called on the MESSAGE thread from paint, so it must not block. The taps
        behind it are lock-free by construction (see the machine's ScopeTap). */
    using ScopeSource = std::function<int (const juce::String& source, float* dest, int maxSamples)>;
    void setScopeSource (ScopeSource s);
    /** The dim second trace behind a display; see ghostSource. */
    void setGhostSource (ScopeSource src) { ghostSource = std::move (src); }

    /** The seven-segment face the LCD windows draw with.

        The host owns the font file (it is an asset of the plugin, embedded
        through its own BinaryData) and hands the typeface over once; the panel
        draws with it wherever the layout says a window is an LCD. Nullptr means
        no LCD face is available and those windows fall back to the sans, which
        is legible but not the design.

        Two faces because a seven-segment digit and a fourteen-segment letter are
        different fonts (DSEG7 and DSEG14): a field holds a number and a dropdown
        holds a word, and the layout names which one each window wants in
        `lcdFont`. The panel carries no font of its own for the same reason it
        carries no PNG: it is a package, and an asset baked into it would be the
        one copy every machine could not replace. */
    void setLcdTypefaces (juce::Typeface::Ptr digits, juce::Typeface::Ptr alphanumeric);

    /** A DRAGGABLE POINT ON A DISPLAY, and the same seam one level further.

        The author: *"the screens should be interactive where available. Filters/LFO/Env
        should have points on the actual screen so you can shape it and it alters
        the actual knob state."*

        The panel owns the GESTURE - where the points are drawn, which one the
        pointer grabbed, where it was dragged to - and the host owns the MEANING.
        `x` and `y` are normalised to the display's own rect with y UP, because
        that is how a curve reads; everything about what they mean stays in the
        machine, since only the machine knows that a filter window's x is a
        logarithmic cutoff and that an envelope's middle point is a time and a
        level at once. A unit with no interactive displays sets neither of these
        and nothing here changes. */
    /** A DRAGGABLE POINT. `scale` is its size against the standard one: 1 is a
        point on the curve, and a bigger number is a point that stands for more
        than one control. The author, 2026-09-17, of the filter window: "Can we add
        another 'bigger' point on the filter that when dragged changes all the
        knobs at the same time? I think you can discover some cool effects that
        way." The panel still owns only the GESTURE - where the point is drawn,
        which one was grabbed, where it went - and a macro point is a point
        whose meaning happens to be seven parameters instead of one. */
    struct Handle { juce::String id; float x = 0.0f, y = 0.0f; float scale = 1.0f; };
    using HandleSource = std::function<int (const juce::String& scopeId, Handle* dest, int maxHandles)>;
    void setHandleSource (HandleSource s);

    /** A POINT WAS DRAGGED, to this normalised position on that display. The
        host moves whatever parameters the point stands for; the panel does not
        move anything itself, so a point can never disagree with the knob it
        shares a parameter with. */
    std::function<void (const juce::String& scopeId, const juce::String& handleId,
                        float x, float y)> onHandleDrag;

    /** A POINT WAS GRABBED (`down`) OR LET GO. A display whose axis is derived
        from the very values the points move has to hold that axis still for the
        duration of a drag, or the window rescales under the pointer and the
        point chases itself - which is what an envelope's release point did: it
        sits at the end of the tail by construction, so shortening the release
        shortened the window by exactly as much and the point never moved. The
        panel cannot know which displays have that property, so it reports the
        gesture and the machine decides what to freeze. */
    std::function<void (const juce::String& scopeId, const juce::String& handleId,
                        bool down)> onHandleGrab;

    /** A KEY WAS PLAYED, AND THIS IS THE WHOLE OF WHAT A KEYBED REPORTS.

        The same split every other seam on this class makes: the panel owns the
        GESTURE - which key the pointer is over, how far down it landed, what a
        drag across the bed means - and the unit owns the MEANING. This class
        knows a rectangle is MIDI note 60; it does not know what channel that
        goes out on, what the patch transposes it by, or whether a scale snaps
        it, and a panel that decided any of those would be a second keyboard
        disagreeing with the machine's own.

        `velocity` is 0..1 and comes from WHERE DOWN THE KEY the click landed:
        near the tip is loud, near the base (up by the black keys, where a
        pianist's hand is nearer the pivot) is quiet. That is Serum's own
        behaviour and it is what makes an on-screen keyboard playable at all -
        a fixed velocity gives one dynamic and a patch that answers to velocity
        then has exactly one sound from the panel.

        A unit that draws no keys never sets this and nothing here changes. */
    std::function<void (int midiNote, float velocity, bool on)> onNote;

    /** LIGHT A KEY THAT IS SOUNDING, from outside.

        The panel lights the key it is playing itself; this is for every note
        that arrives some other way - a wire, the host, an arpeggiator, the
        shell's pattern player - because the panel has no idea those exist. It
        is `setLit` addressed by NOTE rather than by control id, which is the
        only address a unit has: the ids are the export's (`kbd_7`) and a
        machine should not have to know how a keyboard was numbered to say that
        middle C is down. Unknown notes are ignored, so a unit may push every
        note it plays without first asking what the keyboard spans. */
    void setKeyLit (int midiNote, bool lit);

    /** WHICH KEY IS UNDER A POINT, in design units: the MIDI note, or -1. The
        blacks are tested first because they overlap the whites either side of
        them, which is the same order the layout lists them in and the same
        order the hit test uses. Exposed for a unit's own probe. */
    [[nodiscard]] int keyAt (juce::Point<float> design) const noexcept;

    /** A DRAWN LFO SHAPE, and the third seam of exactly the same shape as the
        other two.

        The author, sending Serum's LFO editor: *"honestly why dont we concentrate on
        building the exact same LFO system as serum has? Its a way way better
        LFO that we have. You have complete control over everything."* A shape
        selector offers five curves and nothing between them; a PATH is the
        shape itself - a list of points with a bend on the segment between each
        pair - so an LFO can be a ramp, a staircase, a swung pulse, a pitch
        sequence, or a curve nobody has a name for.

        THE PANEL OWNS THE GESTURE AND THE HOST OWNS THE PATH. This file never
        learns what an LFO is, what a cycle is worth in milliseconds, or where a
        path is stored. It is handed points, it draws them, and when somebody
        edits one it hands back the WHOLE new list - it moves nothing itself, so
        a path and the patch it came from cannot drift apart. That is the same
        rule `onHandleDrag` follows and it is why neither writes a parameter. */

    /** ONE POINT, AND THE BEND OF THE SEGMENT THAT LEAVES IT.

        A DELIBERATE COPY of the host's own point struct, field for field. This
        library is shared by every machine and must not include one machine's
        engine header; the price is that the two declarations have to agree, so:
        `x` is position in the cycle 0..1 ascending, `y` is level 0..1 with 0.5
        the centre line, and `curve` is the bend of the segment LEAVING this
        point, -1..+1 with 0 straight. The curve belonging to the LEFT point of a
        segment is what makes a path one array with nothing to keep in step.

        A host with a different layout copies field by field into this struct in
        its PathSource; it must not reinterpret_cast a block of its own. */
    struct PathPoint { float x = 0.0f; float y = 0.5f; float curve = 0.0f; };

    /** The most points this panel will draw or emit. It is a CEILING on what
        comes back from the host, not a promise about what the host stores - a
        machine whose own limit is lower simply never fills the buffer. Thirty-two
        is megasynth's, and it is a sixteen-column staircase with a point at each
        end of every step, which is the densest thing anybody draws by hand. */
    static constexpr int kMaxPathPoints = 32;

    /** WHERE A PATH EDITOR'S POINTS COME FROM. Given the editor's own id - the
        id the spec printed on that window - fill the buffer and say how many
        points went in. Called on the MESSAGE thread from paint, so it must not
        block. A unit with no path editors never sets this. */
    using PathSource = std::function<int (const juce::String& id, PathPoint* dest, int maxPoints)>;
    void setPathSource (PathSource s);

    /** THE PATH WAS EDITED, and here is the whole new list.

        The WHOLE list rather than a description of the change, because half the
        gestures below are structural - a double-click adds or removes a point, a
        shift-click replaces a whole grid column with a step - and a diff of a
        structural edit is a second representation of the path that has to be
        kept in step with the first. The host validates and stores it (megasynth's
        `LfoPath::tidy`) and the next repaint asks what it ended up with, so what
        is drawn is always what was actually kept and never what was asked for. */
    std::function<void (const juce::String& id, const PathPoint* points, int count)> onPathEdit;

    /** A PATH GESTURE BEGAN (`down`) OR ENDED, for a host that needs to know an
        edit is in flight: one undo transaction per drag rather than one per
        mouse-move, and a parameter left un-notified until the pointer is
        released. Same reason `onHandleGrab` exists. */
    std::function<void (const juce::String& id, bool down)> onPathGrab;

    /** THE FOUR DRAWING TOOLS, from the Serum 2 user guide's "LFO Graph Tools"
        table (p. 189). POINT manipulates points - add, move, delete, and bend
        the curve between them. FLAT, RAMP UP and RAMP DOWN each fill the grid
        column under the pointer, which is why the guide says every tool "relies
        on the current grid setting".

        The tool is the PANEL's, not a path's: it is which pointer you are
        holding, so it is one setting for the whole panel exactly as it is one
        row of buttons on Serum's own. A unit wires its tool buttons to this. */
    enum PathTool { kPathPoint = 0, kPathFlat = 1, kPathRampUp = 2, kPathRampDown = 3 };
    void setPathTool (int tool);
    [[nodiscard]] int pathTool() const noexcept { return pathToolNow; }

    /** THE GRID, WHICH IS A PARAMETER AND NOT A PICTURE.

        `[MAN]` Serum 2 user guide p. 192, GRID: *"The visual grid background in
        the LFO graph changes as you adjust this setting."* So the divisions
        cannot be printed into the background - the kit deliberately leaves them
        out and this class draws them - and a unit that offers the two grid
        spinners pushes them here.

        `gridH` is the HORIZONTAL lines: how many ROWS the LEVEL axis is cut into,
        and what a point's level snaps to. `gridV` is the VERTICAL lines: how many
        COLUMNS the CYCLE is cut into, what a point's time snaps to, and the cell
        the Flat and Ramp tools fill. Serum names its own two spinners the same
        way round. Either at zero leaves that axis as the layout authored it. */
    void setPathGrid (const juce::String& id, int gridH, int gridV);
    /** What the grid is now: x is `gridH` (rows), y is `gridV` (columns). */
    [[nodiscard]] juce::Point<int> pathGrid (const juce::String& id) const;

    /** HOW A PATH WINDOW READS ITS POINTS, set by the unit from the LFO's TYPE.

        kPathTime is the graph: x is a time in the cycle, the list is sorted by
        it, the ends are pinned to 0 and 1, and the grid tools fill columns.
        kPathFigure is a FIGURE (Serum 2's Path type, serum2_screenshots/
        lfos.png): the points are free positions on the window, the ORDER of
        the list is the order the LFO visits them, every axis is free, and a
        segment's `curve` is a bulge of the line rather than a warp of its
        timing - see figurePoint. kPathTrace is not editable at all: the window
        shows the trail the unit pushes with setPathTrace, which is what a
        chaotic LFO's picture is. The unit knows what an LFO is; the panel
        knows how to draw and edit three kinds of window. */
    enum PathMode { kPathTime = 0, kPathFigure = 1, kPathTrace = 2 };
    void setPathMode (const juce::String& id, int mode);
    /** THE LARGER CANVAS: the same path window as an overlay over the whole
        panel, every tool and gesture the small glass has, closed by a click
        outside it or Escape. [MAN] Serum 2 guide p. 194, "LFO Editor". */
    void openPathPopout (const juce::String& id);
    void closePathPopout();
    [[nodiscard]] bool pathPopoutOpen() const noexcept;
    /** A VIEW AS A LAYER over the plate. The view's page image and every control
        on it are drawn over a scrim, in the place the export gave them, and
        they take the mouse while the layer is open; a click anywhere off the
        layer closes it, as does switching the view section. Built for
        MegaSynth's delay designer on 2026-09-19 and NOT used there in the end:
        The author wanted the lanes under the unit's own strip, in the rack, which is
        what the band scroll below is for. It stays for a page that genuinely
        belongs over the plate. `viewSection` is the page section whose pages
        are the views (the export's "view"); `view` is the page index. */
    void openLayer (const juce::String& viewSection, int view);
    void closeLayer();
    [[nodiscard]] bool layerOpen() const noexcept { return layerView >= 0; }
    [[nodiscard]] int pathMode (const juce::String& id) const;
    /** WHERE THE LFO IS ON THE WINDOW, as a moving dot: both 0..1, and on a
        graph x is the phase and y the value. Pushed by the unit from its own
        engine tap at display rate; `on` false hides the dot. */
    void setPathCursor (const juce::String& id, float x, float y, bool on);
    /** THE TRAIL a kPathTrace window draws: `pairs` x,y pairs, 0..1, oldest
        first. Copied, so the unit's buffer may go away. */
    void setPathTrace (const juce::String& id, const float* xy, int pairs);
    /** WHAT THE WINDOW DRAWS BEHIND THE SHAPE, at low opacity: `n` values in
        0..1, oldest first, one per column across the plot.

        The author, 2026-09-22, on EchoFold's sidechain duck: *"you should be able to
        see the actual sound/waveform in the background so its easy to shape"*,
        and again when a first attempt drew a synthetic curve instead: *"You
        should be able to see the actual playing sound waveform in the
        background 'lower opacity'."* A duck is placed against a kick you can
        SEE, and a window without the audio in it is a window you place a shape
        on by counting.

        Copied, so the unit's buffer may go away. Empty clears it. */
    void setPathBackdrop (const juce::String& id, const float* v, int n);
    /** THE FIGURE'S SEGMENT LAW, a copy of megasynth's LfoPath::figurePoint
        for the reason pathBend is a copy of pathCurve: the library cannot
        depend on one machine's engine, and the two MUST agree to the number or
        a figure sounds different from the curve that was drawn. A quadratic
        from `a` to `b` through a control point pushed off the chord's middle,
        along its left normal, by curve * kFigureBulge * |chord|. */
    static constexpr float kFigureBulge = 0.75f;
    [[nodiscard]] static juce::Point<float> figurePoint (const PathPoint& a, const PathPoint& b,
                                                         float t) noexcept;

    // ---- THE STEP GRID: a sequencer lane and a piano roll, one control ------
    /** A PATTERN WINDOW. The path editor's contract, applied to a grid.

        THE PANEL OWNS THE GESTURE AND THE HOST OWNS THE PATTERN. This file never
        learns what a step is worth in beats, what a row means, or where a
        pattern is stored. It is handed cells, it draws them, and when somebody
        edits one it hands back the WHOLE new list - it moves nothing itself, so
        a grid and the state it came from cannot drift apart.

        ONE CONTROL TYPE, AND `rows` IS THE DIFFERENCE. A grid many rows high is
        a piano roll, or an arpeggiator's note lane where a row is a degree of
        the scale; a grid ONE row high is a VALUE LANE, where a cell is a bar
        whose height is that step's velocity or gate. Two types would have been
        two bezels, two geometries and two sets of gestures for one rectangle
        divided two ways. */

    /** ONE CELL. `col` is the step, `row` counts UP FROM THE BOTTOM (so a piano
        roll's low notes are low on the window and an arp's row 0 is its lowest
        degree), `len` is how many columns it covers - always 1 on a window that
        did not declare `span` - and `value` is 0..1: a note's velocity, a lane's
        level. A host with a different layout copies field by field; it must not
        reinterpret_cast a block of its own. */
    struct GridCell { int col = 0; int row = 0; int len = 1; float value = 1.0f; };

    /** The most cells this panel will draw or emit. 256 is megasynth's pattern
        pool, which is itself the widest grid the shell can draw - one note on
        every step of the longest clip. A machine whose own limit is lower simply
        never fills the buffer. */
    static constexpr int kMaxGridCells = 256;

    /** WHERE A STEP GRID'S CELLS COME FROM. Given the window's own id, fill the
        buffer and say how many cells went in. Called on the MESSAGE thread from
        paint, so it must not block. A unit with no step grids never sets this. */
    using GridSource = std::function<int (const juce::String& id, GridCell* dest, int maxCells)>;
    void setGridSource (GridSource s);

    /** THE PATTERN WAS EDITED, and here is the whole new list - for the reason
        onPathEdit hands back a whole path: half the gestures are structural (a
        click adds or removes a cell, a drag lengthens one) and a diff of a
        structural edit is a second representation that has to be kept in step
        with the first. The host validates and stores it, and the next repaint
        asks what it ended up with, so what is drawn is always what was kept. */
    std::function<void (const juce::String& id, const GridCell* cells, int count)> onGridEdit;

    /** A GRID GESTURE BEGAN (`down`) OR ENDED: one undo transaction per drag
        rather than one per mouse-move. Same reason onPathGrab exists. */
    std::function<void (const juce::String& id, bool down)> onGridGrab;

    /** HOW MANY ROWS AND COLUMNS THE WINDOW IS DIVIDED INTO, and it is a SETTING
        rather than a picture: a pattern's column count follows its LENGTH and
        its step value and a roll's row count follows its range, all of which the
        player changes while the plugin runs. So the kit deliberately prints no
        divisions and this class draws them - the same split, for the same
        reason, as the LFO graph's grid ([MAN] Serum 2 guide p. 192). Either at
        zero leaves that axis as the layout authored it. */
    void setGridSize (const juce::String& id, int rows, int cols);
    /** What the grid is now: x is rows, y is columns. */
    [[nodiscard]] juce::Point<int> gridSize (const juce::String& id) const;
    /** THE MIDI NOTE OF THE BOTTOM ROW, so the painter can tint the black-key
        rows without being told what a piano is. -1 means the rows are not
        pitches and none is tinted. */
    void setGridBase (const juce::String& id, int firstRow);
    /** WHICH COLUMN IS SOUNDING, -1 when nothing is. Pushed by the unit at
        display rate from its own player; it is the one thing in this window that
        moves without anybody touching it. */
    void setGridPlayhead (const juce::String& id, int col);
    /** NAME THE ROWS OF A GRID, bottom row first (row 0 is the bottom, as
        GridCell's own row index is).

        A twelve-lane drum grid is unreadable without them: the author, 2026-09-17,
        "the lanes in the grid are extremely hard to see, their colors needs to
        stand out more" and "the grid needs lanes so you know what lane is a
        kick/snare/hihat etc". The labels are drawn INSIDE the plot at its left
        edge, over a band that also alternates in tone - which is what makes one
        lane tell itself from its neighbour at a glance and is the same thing a
        drum machine's own rows do.

        An empty array is the old behaviour: no labels, no banding. */
    void setGridRowLabels (const juce::String& id, const juce::StringArray& labels);
    /** Which column is lit now, -1 when none. */
    [[nodiscard]] int gridPlayhead (const juce::String& id) const;

    // ---- THE LIST: rows of text with a selection ---------------------------
    /** A SCROLLING LIST, on the same contract as the step grid: the panel owns
        the window and the gesture, the host owns the rows.

        WHY A PANEL NEEDS ONE. A patch browser is five hundred names and every
        other window here holds one value, one curve or one pattern. The
        alternative was a juce::ListBox laid over the faceplate - another
        toolkit's furniture inside a panel this kit otherwise draws whole, in a
        different type, with different corners, and it would be the one control
        on the machine that does not match the machine.

        THE HOST IS ASKED FOR A WINDOW, NOT FOR THE BANK. Five hundred rows built
        into strings on every repaint is five hundred allocations at display rate
        for forty visible names, so the source is given the first row and the
        count that fit and fills only those. The TOTAL is pushed separately with
        setListLength, because the panel needs it for the scrollbar and to stop
        scrolling past the end, and only the host knows it. */
    struct ListRow
    {
        juce::String text;        ///< the name, at the left
        juce::String right;       ///< a second, dimmer field at the right: a bank, a number
        bool selected = false;    ///< THE HOST SAYS SO, from its own state, never the panel
        bool header = false;      ///< a group heading rather than a row anybody can choose
        bool dim = false;         ///< present but not available - a factory row in a user list
    };

    /** The most rows this panel asks for at once: a list taller than this on a
        1400 px plate would be drawing type nobody can read. */
    static constexpr int kMaxListRows = 256;

    /** WHERE A LIST'S ROWS COME FROM. Given the window's own id and the first
        row it is showing, fill the buffer and say how many went in. Called on
        the MESSAGE thread from paint, so it must not block. */
    using ListSource = std::function<int (const juce::String& id, int firstRow,
                                          ListRow* dest, int maxRows)>;
    void setListSource (ListSource s);

    /** A ROW WAS CHOSEN. The host decides what that means - load the program,
        set the filter - and the next repaint asks what it ended up with, so what
        is drawn is always what was kept. A heading row never reports. */
    std::function<void (const juce::String& id, int row)> onListRow;

    /** HOW MANY ROWS THE LIST HAS IN TOTAL, which only the host knows. It is
        what the scrollbar is drawn from and what stops a scroll past the end. */
    void setListLength (const juce::String& id, int rows);
    /** The first row showing. Clamped to the length, so a shorter list after a
        filter change never leaves the window looking at nothing. */
    void setListScroll (const juce::String& id, int firstRow);
    [[nodiscard]] int listScroll (const juce::String& id) const;
    /** How many rows are on screen at once: rows per column times columns. A
        host pages by this. */
    [[nodiscard]] int listVisibleRows (const juce::String& id) const;

    /** Repaint every display, and nothing else. A trace is the only thing on a
        panel that changes without anybody touching a control, so it is the only
        thing that needs a clock - and repainting the whole plate at 30 Hz to
        move four polylines would be the most expensive mistake available. */
    void repaintDisplays();


    /** `assets` is the unit's binary data. `prefix` picks which machine's panel
        to paint in a repo that ships siblings: JUCE keys binary data by FILE
        NAME, so two panels both exporting `background.png` collide silently and
        one machine wears the other's knobs (see tools/prefix_native.py in the
        Filterbank). An empty prefix means "the only panel". */
    explicit NativePanel (PanelAssets assets, const juce::String& prefix = {});
    ~NativePanel() override;   // out of line: PathPopout is complete only in the .cpp

    /** Swap machines at runtime. The panel's size changes with it, so whatever
        laid this component out has to do so again. */
    void setMachine (const juce::String& prefix);

    /** The widest this panel can be and still FILL the component it is given.

        A panel will not upscale past the sharpness ceiling - assetScale divided
        by the device scale, so 1.0 for a 2x export on a Retina screen - because
        upscaling a picture reads as blurry. Past that width it draws at its
        design size, parked at the top left, and the rest of the component is
        bare. The author: "the actual panel ui doesnt take 100% width. there is a like
        200px empty dark space on the right", and then the diagnosis: "the window
        when opening should never be bigger than the actual UI. or the UI should
        grow with it."

        An editor opens its window at min(whatever the screen allows, THIS), and
        the panel fills it at every size it can be. */
    [[nodiscard]] int maxUsefulWidth() const noexcept;

    [[nodiscard]] int designWidth()  const noexcept { return designW; }
    [[nodiscard]] int designHeight() const noexcept { return designH; }
    /** From the SPEC's own name, never a string in this file: a panel that
        prints a literal shows the TEMPLATE's name on every unit made from it. */
    [[nodiscard]] juce::String title() const noexcept { return panelTitle; }
    /** True when the real artwork is being painted; false while the spec
        placeholder stands in for it. */
    [[nodiscard]] bool hasArtwork() const noexcept { return artwork; }

    // --- state pushed from the editor -------------------------------------
    void setValue (const juce::String& id, float v01);
    void setLit (const juce::String& id, bool lit);
    /** A PANEL HAS MORE THAN ONE SCREEN and they say different things. Address
        them by id. */
    void setScreen (const juce::String& id, const juce::String& l1, const juce::String& l2);
    /** The whole-panel form, which writes to the LARGEST screen only. Writing to
        every screen is how the Nord Lead 3 put its patch name in the oscillator-2
        pitch readout, and it is why this is not a loop. */
    void setScreen (const juce::String& l1, const juce::String& l2);
    /** One line into one screen, for a display that shows a number rather than a
        program name - the TC 2290 has five and they never agree. */
    void setScreenText (const juce::String& id, const juce::String& line);
    /** Exactly these lines on the main screen. A single four-field line would be
        squashed by the two-line form above. */
    void setScreenLines (const juce::StringArray&);
    void showEdit (const juce::String& label, const juce::String& value);  // echo for 1.5 s
    void showEditLines (const juce::StringArray&);
    /** The number in a readout window. The panel spec knows the window is there;
        only the editor knows the parameter's range and how it reads, so the TEXT
        comes from there. */
    void setReadout (const juce::String& id, const juce::String& text);
    /** What a readout is CURRENTLY showing, for probes. A panel that can be
        written and not read can only be checked by eye, and "the dropdown in
        a display doesnt change the display value" is exactly the fault a
        written-only panel hides. Empty if there is no such control. */
    [[nodiscard]] juce::String readoutOf (const juce::String& id) const;

    /** What a KNOB or FADER is currently showing, 0..1, or -1 if this panel has no
        such control. The readout's twin, and for the same reason: a panel that
        cannot be asked what it shows can only be checked by eye.

        This is what catches a knob the host never pushed. Solera's gate compares it
        against the parameter for every id in the tree, and a value that never got
        pushed shows up as a control still sitting at its default while the
        parameter has moved. */
    [[nodiscard]] float valueOf (const juce::String& id) const;

    /** Every control this panel would hit-test, in the order the hit test uses:
        id, kind, and the centre a click should land on. For a unit's own probe, so
        it can walk the plate and drive each control the way a player would rather
        than guessing at coordinates. */
    struct ProbeCtl { juce::String id, kind; juce::Point<float> centre; };
    [[nodiscard]] juce::Array<ProbeCtl> probeList() const;

    /** CONTROLS THAT SPRING BACK when the mouse is released.

        A PITCH WHEEL IS SPRUNG AND A MOD WHEEL IS NOT, and that is the whole
        difference between two controls the panel cannot tell apart: both are the
        same kind, and the rest value is centre for one and bottom for the other, so
        nothing in the geometry says which one lets go. The mechanism is here and the
        knowledge is the unit's - it names the sprung ids once, the same seam the
        shell uses for headerCells().

        Found the hard way in Solera on 2026-09-26: the pitch wheel had just been
        given an engine and the very next assertion, "letting go returns the pitch",
        went red with the note stuck a fifth sharp forever. A stuck bend is worse
        than a dead one. */
    void setSprung (const juce::StringArray& ids) { sprungIds = ids; }

    /** PATCH CABLES, for a mod matrix drawn as what it is.

        A slot's cable exists only while that slot has both a source and a
        destination, and it moves whenever either changes - so it is live art and a
        printed one is a lie the machine cannot correct. The export prints the
        SOCKETS, because a socket is there whether or not anything is plugged into
        it, and the host draws the cable between them.

        The endpoints are the jacks `<srcPrefix><slot>` and `<dstPrefix><slot>`,
        looked up in `anchors` - the positions the exporter now carries for
        silkscreen that code has to find. Before it did, this whole feature failed
        SILENTLY: the painter looked up two sockets that were in no layout, found
        neither, and gave up once per slot per repaint with every gate green. */
    void setCableJacks (const juce::String& srcPrefix, const juce::String& dstPrefix)
        { cableSrcPrefix = srcPrefix; cableDstPrefix = dstPrefix; }
    void setCable (int slot, bool on, juce::Colour colour);

    /** Is this slot's cable actually DRAWABLE - on, and with both its sockets
        found? The question nothing asked. It reports what the PAINTER can do
        rather than what the state says, because a painter that gives up quietly is
        invisible to every test that checks state. */
    [[nodiscard]] bool cableDrawable (int slot) const;

    /** A jack's position, or false if this panel has no such anchor. */
    [[nodiscard]] bool anchorOf (const juce::String& id, juce::Point<float>& out) const;

    // --- a control the matrix is moving --------------------------------------
    /** SHOW A MODULATION ON A CONTROL. The author: *"the entire point is that the LFO
        manipulates other controls directly. And you should be able to visibly
        see that manipulation. if the lfo is manipulating cutoff the actual
        cutoff knob in filters should be animating according to the lfo."*

        Three numbers, all in the control's own 0..1 and all RELATIVE to the
        value the knob is set to: `offset01` is where the live value is right
        now, `rangeLo01 <= 0 <= rangeHi01` is how far the modulation can reach
        either side. [MAN] Serum 2 user guide pp. 195-196: "a blue halo appears
        around the knob ... the halo shrinks or expands to show the range of
        modulation". The knob draws the halo over [value + lo, value + hi], its
        lit arc and pointer at value + offset, and a tick where the knob itself
        is set - so the set value stays readable while the live one moves.

        Two range numbers rather than one because a unidirectional route
        reaches from the knob UPWARD only (p. 211: "the destination control to
        be at the beginning") and a symmetric halo would claim half a range the
        modulation never visits. The panel repaints only when one of the three
        moved by more than a pixel's worth, which is what lets an editor call
        this thirty times a second for every knob on the plate. */
    /** THE OUTPUT LEVEL, 0..1, for a `meter` control. A meter that never reads
        is a control that lies at every setting, so the unit pushes its own peak
        here and the panel draws the bar. */
    void setMeter (const juce::String& id, float level01);
    void setModulation (const juce::String& id, float offset01, float rangeLo01, float rangeHi01);
    /** What a control was last told the matrix is doing to it, for a probe that
        has to prove the halo moves rather than trust that it does. */
    [[nodiscard]] std::array<float, 3> modulationOf (const juce::String& id) const;
    /** The control is no longer modulated: halo off, pointer back on the value. */
    void clearModulation (const juce::String& id);

    // --- dragging a source onto a control ------------------------------------
    /** ROUTE A MODULATOR BY DRAGGING ITS TAB ONTO A KNOB. The author: *"to be able to
        assign an LFO I want to be able to keep the mouse button down on one of
        the 6 lfo tabs and drag them to any control I want that lfo to manipulate.
        That should then automatically set it in the matrix instead of someone
        always having to go to the matrix page."*

        [MAN] Serum 2 user guide p. 194: "click and drag from the LFO 2 tab to the
        DETUNE knob ... an LFO 2 label hovers next to the mouse pointer. The
        pointer adds a + sign as you hover over an assignable knob ... When you
        release the mouse button, Serum automatically makes the connection."

        The panel owns the gesture and the host owns what it means, which is the
        split every other seam here makes. A press on a tab-strip cell that then
        MOVES more than a few pixels is a drag and does not switch the tab; a
        press that is released where it landed switches the tab exactly as it
        always did. The gesture is only armed when `onModDrop` is set, so a
        machine that has not wired it keeps switching tabs on the press.

        `canDrop` says whether a control can be a destination at all - only the
        host knows which parameters the matrix reaches - and a control it
        refuses shows no + and takes no drop. Unset, every value control is
        droppable. `onModDrop` fires once, on release over an accepted control,
        with the strip's id, the cell index that was dragged (0 for the first
        tab), WHICH HANDLE of the cell was dragged (0 for the whole cell or its
        X handle, 1 for its Y handle - see setTabHandles) and the target
        control's id. Releasing anywhere else does nothing. */
    std::function<bool (const juce::String& controlId)> canDrop;
    std::function<void (const juce::String& sourceStripId, int sourceIndex, int handle,
                        const juce::String& targetControlId)> onModDrop;

    /** HOW MANY DRAG HANDLES A TAB HAS: 1, and the whole cell is the handle as
        it always was, or 2 - an X and a Y grip at the cell's right, labelled,
        each the source of a drop of its own. The author, on Serum: *"notice that in
        serum. the tabs has X and Y connections when not in NORMAL and SH mode.
        So it supports two connections."* The unit says which cells have two,
        from what it knows about the modulator behind the tab; the panel draws
        the grips, tells them apart on the press and reports the handle in
        onModDrop. Survives a page change. */
    void setTabHandles (const juce::String& stripId, int cell, int handles);
    [[nodiscard]] int tabHandles (const juce::String& stripId, int cell) const;
    /** WHERE A CELL'S GRIP IS DRAWN, in this component's pixels - handle 0 the
        X grip, 1 the Y grip - so a probe presses where the eye sees it, through
        the same geometry the painter and the hit test share. Empty for a strip
        that is not there. */
    [[nodiscard]] juce::Rectangle<int> tabHandleBounds (const juce::String& stripId, int cell, int handle) const;
    /** THE NUMBER IN A TAB'S RING GLYPH: how many things it is connected to.
        The author: *"that tiny half circle to the right is really supposed to show a
        number for how many things its connected to."* In Serum's lfos.png each
        tab reads "3 (1)" - the ring carries the count of assignments. 0 leaves
        the ring empty, as printed. Drawn live over the printed glyph at the
        kit's own ring position, for every cell and not only the lit one, and
        kept across page changes; the unit calls it for every cell whenever its
        matrix changes. */
    void setTabCount (const juce::String& stripId, int cell, int count);
    [[nodiscard]] int tabCount (const juce::String& stripId, int cell) const;
    /** A RIGHT-CLICK (or ctrl-click on macOS) ON A TAB CELL. The author: *"should be
        able to right click a tab like in serum and remove connections."* The
        panel only reports it - the strip's id, the cell, and where on the
        screen, so a menu can open there; the unit builds the menu, because it
        is the unit that knows the matrix. The press does NOT select the tab
        and does NOT start a drag: it is a question about the tab, not a use of
        it. Generic - an envelope strip gets it for free. */
    std::function<void (const juce::String& stripId, int cell, juce::Point<int> screenPos)> onTabMenu;
    /** Selftest only: every press on a two-handle cell reports handle 0, so
        the probe that checks the Y grip must notice. */
    void devIgnoreTabHandles (bool b) { ignoreTabHandles = b; }

    [[nodiscard]] bool hasControl (const juce::String& id) const { return index.count (id) > 0; }
    [[nodiscard]] juce::String labelOf (const juce::String& id) const;
    /** The id of the largest screen - the one the whole-panel calls write to. */
    [[nodiscard]] juce::String mainScreenId() const;
    /** How many lines the panel's own screen was authored with. */
    [[nodiscard]] int screenLineCount() const noexcept { return restLines.size(); }
    /** WHAT THE SCREEN CURRENTLY SAYS, read back, so a probe can assert that a
        key changed the display rather than a person looking at it. The edit
        echo wins while it is showing, which is what the eye sees too. Empty for
        a line the screen does not have. */
    [[nodiscard]] juce::String screenLine (int i) const noexcept
    {
        // THE UNDERLYING SCREEN, not the 1.5-second edit echo. A probe presses
        // controls back to back, so the echo from the PREVIOUS press is almost
        // always still up, and reading it means every control appears to leave
        // the display saying the same thing. Ask for what the screen will say
        // once the echo clears.
        //
        // AND THE SCREEN THAT WAS WRITTEN, not whichever is biggest. A machine
        // whose display is built out of parts has a large screen for the GLASS
        // and a small one for the text line - the M-ONE does - and restLines
        // follows the largest, which on such a panel is the empty face. Reading
        // it said every key left the display blank while the text line was
        // being written correctly the whole time.
        if (lastWritten.isNotEmpty())
            for (const auto& c : ctls)
                if (c.id == lastWritten && c.kind == "screen")
                    return juce::isPositiveAndBelow (i, c.lines.size()) ? c.lines[i] : juce::String();
        return juce::isPositiveAndBelow (i, restLines.size()) ? restLines[i] : juce::String();
    }
    /** Where a control sits in THIS COMPONENT's coordinates, for anchoring a
        popup to the control the hardware puts it on rather than somewhere near
        the panel. Empty for an unknown id. */
    [[nodiscard]] juce::Rectangle<int> controlBounds (const juce::String& id) const;
    /** Is this control drawn in its LIT state? The lamp half of what a unit
        pushed with setLit or setKeyLit, read back - so a unit's own probe can
        assert that a key lights while it sounds rather than looking at it. False
        for an unknown id, the same answer an unlit control gives. */
    [[nodiscard]] bool controlLit (const juce::String& id) const noexcept;
    /** What this control is DRAWN AT, 0..1: the value half of what a unit
        pushed with setValue, read back - so a probe can assert that a preset
        which rewrote forty parameters also moved the forty controls that show
        them, which is the half a preset that "works" can leave undone. -1 for
        an unknown id. */
    [[nodiscard]] float controlValue (const juce::String& id) const noexcept;

    // --- dynamic wells (bands) ----------------------------------------------
    // A well exported with `showWhen: "<controlId>"` is a BAND: it exists on the
    // plate only while its GATE control reads on, and the visible bands STACK
    // from their section's band area top with the export's gap between them.
    // The author's design decision, verbatim: "FXs rack modules should only be visible
    // when they are actually active." The layout carries the map (`bands`), each
    // band its own image; the base background holds none of them.
    //
    // THE GATE IS A CONTROL, not a parameter: the unit pushes the parameter into
    // that control exactly as it pushes any other (setValue / setLit), and the
    // panel reflows when it lands. On is "lit, or value above one half", which
    // covers a lamp cap and a value control alike; a gate that needs inverting
    // is the unit's to invert before it pushes, as invertLit already does.
    //
    // Reflow moves each band's member controls WHOLE - draw, hit, travel, every
    // value push - so the rest of this class never learns bands exist.

    /** Is this band stacked on the plate right now? False while its gate is off
        AND while it did not fit (see the overflow rule in reflowBands). */
    [[nodiscard]] bool bandShown (const juce::String& wellId) const noexcept;
    /** Where a band is stacked right now, in design units; empty while hidden. */
    [[nodiscard]] juce::Rectangle<float> bandBounds (const juce::String& wellId) const noexcept;
    /** THE STACKING ORDER IS AN INPUT, NOT THE EXPORT'S CONSTANT. The author: "its
        also vital the the FXs units can be dragged around so the user can
        change the order." The unit hands over the section's well ids in the
        order it wants them stacked; ids it leaves out keep their relative
        export order after the named ones. Unset, the export's order stands.
        The drag gesture and wherever the order is stored are the unit's. */
    void setBandOrder (const juce::String& sectionId, const juce::StringArray& wellIds);
    /** THE DRAG GESTURE THAT SETS THAT ORDER. Press on a shown band's GRIP - the
        narrow dotted strip at its left edge, kBandGripPx wide - and drag
        up or down: the band moves through the stack as the pointer crosses its
        neighbours' centres, reflowing live, and on release the unit is handed
        the section's new order (well ids, top to bottom) to store wherever it
        keeps it. The author: "I cant move order units around." The panel never
        stores the order itself; the unit pushes it back with setBandOrder on
        every program load, the way it pushes every other stored thing. */
    std::function<void (const juce::String& sectionId, const juce::StringArray& wellIds)> onBandOrder;
    /** THE STRIP'S OWN GRIP CAN BE SWITCHED OFF. A unit with a chain list of
        HANDLES (controls of kind `handle`, each naming the band it moves with
        `listOf`) reorders from the list and wants nothing on the strips to
        start a drag: the author, 2026-09-19, "disable drag and drop completely on the
        actual FX panel", after a press near a knob kept starting a reorder. */
    void setBandGripEnabled (bool on) noexcept { bandGripEnabled = on; }
    /** THE GRIP: a narrow strip at the band's left edge, drawn with three dots,
        and the only place a reorder drag starts. It was the left 110 design px
        (the name tile) until 2026-09-17, when the author reported the obvious
        consequence: "many times when I try to turn a knob the module moves. A
        module should only be draggable by a dragg handle. Make that on the left
        side." A wide invisible target that yields to controls still catches
        every press that lands between two of them. */
    static constexpr float kBandGripPx = 18.0f;     // design units, and it is DRAWN
    /** Selftest hooks, so a probe can prove its own checks catch a painter that
        draws a gated-off band, and one that forgets the hit rects on a reflow. */
    void devForceBandGatesOn (bool b);
    void devIgnoreBandReflowForHits (bool b) { bandHitsStale = b; }

    // --- the viewport ------------------------------------------------------
    // A PANEL TOO BIG FOR ITS WINDOW IS SCROLLED, NOT SHRUNK. On-screen size is
    // `design x window / designWidth`, so a panel that grows - a modular machine
    // adding lanes, or a plate you look down at rather than a rack front - drives
    // its own legends to nothing and there is no arrangement of a fixed faceplate
    // that survives it. Two machines hit this from opposite directions: the
    // megasynth's lanes (supersynth BUILD-SPEC 1b/1c) and the HR-16's 1200x1644
    // top view (hr16 todo.md D-13, a 15 px legend landing at 9.2).
    //
    // THE SCALE POLICY, in three sentences.
    //   1. Width still drives the scale exactly as it always did: k = width / designWidth.
    //   2. A panel MAY declare a legibility floor - `minLegendPx` in the theme, or
    //      setLegibilityFloor() - and then k never falls below the scale at which the
    //      panel's SMALLEST legend (`minSize`, design units, not `labelSize`, which is
    //      only the default one) reaches that many screen pixels. No declaration, no
    //      floor, so k is byte-for-byte what it was and no existing panel moves.
    //   3. Height NEVER reduces k. Whatever does not fit, in either axis, is
    //      scrolled. That is the whole feature: scale is a legibility decision,
    //      not a fitting decision.
    //   4. k is then CAPPED at sharpCeiling(), the export scale over the display's,
    //      because past that the resampler is inventing pixels. On a 2x export and a
    //      2x screen the cap is 1.0, so a window wider than the design does NOT get a
    //      wider plate; the plate draws at 1.0 and centres. Rule 1 therefore holds
    //      only up to the cap, and anything computing an expected scale has to apply
    //      the cap too.
    //
    // It is deliberately NOT a free zoom. A zoom is a control the user has to find
    // and then keeps having to re-find; a floor plus scrolling has one right answer
    // and reaches it by itself.

    /** True when there is panel to the left or right of what is on screen. */
    [[nodiscard]] bool scrollsHorizontally() const noexcept;
    /** True when there is panel above or below what is on screen. */
    [[nodiscard]] bool scrollsVertically() const noexcept;
    /** How far the window has been moved over the panel, in COMPONENT pixels. */
    [[nodiscard]] juce::Point<int> viewOffset() const noexcept { return { viewX, viewY }; }
    /** The whole panel's size at the current scale, in component pixels. Bigger
        than this component's bounds in whichever axis scrolls. */
    [[nodiscard]] juce::Point<int> contentSize() const noexcept;
    /** Move the window over the panel. Clamped; a no-op on an axis that fits. */
    void setViewOffset (int x, int y);
    /** Bring a control fully into view, doing nothing if it already is. For an
        editor that has just selected a module the user cannot see. */
    void scrollToControl (const juce::String& id);
    /** The scale actually in force. Equals width/designWidth unless the legibility
        floor raises it or the sharp ceiling caps it. */
    [[nodiscard]] float drawScale() const noexcept { return scale(); }
    /** THE CAP: the largest scale at which every raster is a downscale of its own
        art, the export scale over the display's. A 2x export on a 2x screen gives
        1.0, so on a Retina display a panel in a window WIDER than its design does
        not fill it - it draws at 1.0 and centres. That is deliberate and it is why
        `drawScale()` is not `width/designWidth` in general. Public because a probe
        or an editor that reasons about scale has to account for it; before it was
        exposed, the viewport probe asserted the uncapped ratio and failed on every
        Retina machine while passing on a 1x one. */
    [[nodiscard]] float sharpCeiling() const noexcept;
    /** Declare the legibility floor by hand: the theme's smallest legend is
        `legendDesignUnits` design units, and it may not reach the screen at fewer
        than `screenPx` pixels. 13 is the recommended `screenPx`. Either argument at
        zero clears the floor. An export normally supplies both, as `theme.minSize`
        and `theme.minLegendPx`; this is for a panel whose export predates them, and
        for the probe. Re-apply it after setMachine(), which reloads the theme. */
    void setLegibilityFloor (float legendDesignUnits, float screenPx);
    [[nodiscard]] float minLegendPx() const noexcept { return minLegend; }
    /** The theme's smallest printed legend, in DESIGN units - the number the floor
        is computed against. 0 when the export does not say. */
    [[nodiscard]] float legendDesignSize() const noexcept { return labelDesign; }
    /** The height this panel wants at width `w`: its own design aspect. A parent
        that gives it less gets a vertical scroll instead of a squashed panel, which
        is how a tall machine keeps its window on the screen. */
    [[nodiscard]] int heightForWidth (int w) const noexcept;
    /** True when this point is on the scroll chrome rather than on the panel. The
        bars are drawn OVER the panel and take a click before any control does, which
        costs the few controls under the last ten pixels of a scrolling edge - scroll
        a little and they come out from under it. Anything laying something else over
        this component needs to know where the chrome is. */
    [[nodiscard]] bool isOnScrollBar (juce::Point<float> componentPt) const;
    /** A point in THIS COMPONENT's coordinates, in design units. The one conversion
        every mouse handler goes through, exposed so a probe can prove that clicking
        where a control is painted reaches that control - under scroll as well as not. */
    [[nodiscard]] juce::Point<float> toDesign (juce::Point<float> componentPt) const noexcept;
    /** Selftest hook: make hit testing ignore the viewport - the exact defect that
        would put every knob under the cursor one control off once the panel scrolls. */
    void devIgnoreViewportForHits (bool);

    /** WHERE A CONTROL IS DRAWN AND WHERE IT IS CLICKED, side by side.
        The controls probe drives everything BY ID, which proves the editor moves the audio
        when it is handed an id and proves nothing at all about whether clicking where a
        control is painted produces that id. Those are two different claims and a dead button
        lives in the gap: move a control in panel.json without re-exporting, or ship a stale
        layout.json, and every click lands nowhere while every existing check stays green. */
    /** `page` and `section` are what a paged panel needs a checker to know: a
        control on a page that is not showing is not clickable and must not be
        reported as misplaced for it. A probe walks these, switches the section
        to the control's own page, and then asks what a click at its centre
        reaches - which is the only question worth asking of a paged panel. */
    /** WHERE ONE CONTROL IS DRAWN AND WHAT HAS TO BE SHOWING FOR IT TO BE THERE.
        Both axes travel, because a control can be on a view AND on a page of a
        section that lives there: a checker that set only one of them is asking
        about a control that is still hidden behind the other. */
    struct Placement
    {
        juce::String id, kind, section, viewSection;
        juce::Rectangle<float> drawn, hit;
        int page = -1, view = -1;
    };
    [[nodiscard]] std::vector<Placement> placements() const;
    /** The id a click at this point reaches, in DESIGN units. Empty when it reaches nothing. */
    [[nodiscard]] juce::String idAt (juce::Point<float> designPoint) const;
    /** Selftest hook: slide one control's hit area off the thing it draws. */
    void devMoveHit (const juce::String& id, float dx, float dy);
    /** Whether a reorder drag is armed, for a probe that performs the gesture
        and has to say WHICH step of it failed. */
    [[nodiscard]] bool devDragArmed() const noexcept { return bandDragGroup >= 0; }
    /** What a handle names, and every band id there is, for the same probe. */
    [[nodiscard]] juce::String devListOf (const juce::String& id) const
    { const auto it = index.find (id); return it == index.end() ? juce::String() : ctls[(size_t) it->second].listOf; }
    [[nodiscard]] juce::String devBandIds() const
    {
        juce::String out;
        for (const auto& gp : bandGroups)
        { out << "[" << gp.section << ":"; for (const auto& b : gp.bands) out << " " << b.id; out << "] "; }
        return out;
    }
    /** Every control this panel parsed, on stderr. For when a control is not
        where the spec says and nothing else will tell you why. */
    void debugDump() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    struct Sprite { juce::Image img; float w = 0, h = 0, ax = 0, ay = 0; juce::Image scaled; int scaledFor = -1; };
    struct Pointer { juce::String style; float r1 = 0, r2 = 0, width = 2, dist = 0, w = 0, h = 0;
                     juce::Colour colour; float a0 = -135, a1 = 135;
                     /** Positions, when the control is a SWITCH rather than a knob.
                         0 is continuous. A four-position OCTAVE switch drawn with a
                         continuous pointer reads as a continuous control, which is
                         exactly what it is not. */
                     int steps = 0; };
    /** A knob's LED ring. `r` is the LIT lamp's radius and every other circle is a
        fixed fraction of it, which is how the navy kit draws one (bezel 4.6, lit 3.4,
        unlit 3.2, core 1.8) - so a DENSE ring, whose lamps are shrunk by the exporter
        to fit the 270-degree arc, shrinks as one lamp instead of a full-size bezel
        with a tiny light inside it.

        `single` is not decoration. A BAR lights every lamp up to the value and says
        HOW MUCH; SINGLE lights only the lamp at the value and says WHERE. Painting a
        positional control as a bar reports a different number to the player at every
        setting, and this painter drew every ring as a bar while the spec said which
        it was - so balance, detune and shape read as levels on the panel that
        renders and as positions in the PNG that is checked. */
    struct Ring { int n = 0; float radius = 0, r = 0; bool single = false;
                  juce::Colour bezel, off, lit, core; };
    /** The LIT part of a knob's value arc. It follows the value, so it is drawn here and is
        deliberately NOT in the body sprite: knobs of one radius share a sprite, so a baked arc
        showed one reading on all of them and never moved. */
    struct Arc { bool on = false; float r = 0, width = 0, origin = 0, a0 = -135, a1 = 135;
                 juce::String style;          // "serum" sweeps from `origin` to the value, round-capped;
                                              // "steel" is the same sweep over a soft glow
                 juce::Colour colour;
                 /** THE GLOW UNDER THE ARC, "steel" only. A wider, translucent copy of
                     the same sweep in the theme's halo colour, drawn first, so the crisp
                     arc reads as a lit filament on the rim rather than a painted line.
                     A width of 0 draws none, which is every arc exported before it. */
                 float glowWidth = 0, glowOpacity = 0; juce::Colour glowColour; };
    /** JACK POSITIONS from the export's `anchors`. A jack is silkscreen - the
        raster draws the socket - so it is not a control and it is not in `ctls`. It
        still has a place, because a cable has to be drawn into it. */
    std::map<juce::String, juce::Point<float>> anchors;
    juce::StringArray sprungIds;
    juce::String cableSrcPrefix { "matSrcJ" }, cableDstPrefix { "matDstJ" };
    struct Cable { bool on = false; juce::Colour colour; };
    std::array<Cable, 16> cables {};
    void paintCables (juce::Graphics&, float k) const;

    struct Ctl
    {
        juce::String id, kind, label, group;
        float x = 0, y = 0, w = 0, h = 0, r = 0;
        float value = 0.5f, defaultValue = 0.5f; bool lit = false, bipolar = false;
        bool invertLit = false;      // a lamp that lights when its parameter is 0
        bool hasDot = false;         // a tall tab: lamp above, name centred below
        /** THE CELLS THE KIT DREW, with its own gaps between them, so the lit
            one is highlighted and re-lettered exactly where the printed name
            already is. Empty falls back to dividing the strip evenly. */
        juce::Array<juce::Rectangle<float>> cells;
        juce::Colour fillLit;
        float pillRadius = 0.0f;   // the kit's corner radius for a pill strip; 0 = height/2
        /** PER CELL: how many drag handles (1 or 2) and the connection count
            in the ring glyph. Both set by the unit, both outlive a page change
            because they live on the control. Short arrays grow on demand. */
        juce::Array<int> tabHandles, tabCounts;
        juce::Colour ringDim;      // the printed ring's ink on an unlit pill cell
        /** A DROPDOWN'S POSITIONS, in order. Non-empty means a click opens the
            list rather than stepping the parameter - which is what the caret
            printed on the window has been promising. */
        juce::StringArray choices;
        /** THE CONTROL THE LIST ACTUALLY WRITES, when that is not this one.

            A time window shows MILLISECONDS but the list a player wants on it is
            the note divisions, and those live on the MODE parameter - so picking
            "1/8" has to write MODE, not the millisecond field it was clicked on.
            Without this the menu wrote 1/8 of the time range into the time.
            Empty (the normal case) means the list writes the control it opened
            from. The author asked for this three times: "I still cant get a dropdown on
            any displays?????" */
        juce::String menuTarget;
        Sprite* sprite = nullptr; Sprite* off = nullptr; Sprite* on = nullptr;
        Sprite* track = nullptr; Sprite* handle = nullptr;
        // A centre-off toggle has three frames, not two. `positions` is 2 for an
        // ordinary switch and `frames` is then just {off, on}.
        std::vector<Sprite*> frames; int positions = 2, pos = 0;
        Pointer pointer; Ring ring; Arc arc; float travel0 = 0, travel1 = 0;
        juce::Rectangle<float> rect;                    // a screen's or readout's glass
        juce::String style; juce::Colour bg0, bg1, text, edge; juce::StringArray lines;
        bool horizontal = false; float cross = 30;      // fader: the slot's width across itself
        juce::String value_text, prefix; float textSize = 12;   // readout
        float lineHeight = 0;       // a screen's line pitch, as a multiple of the font size
        juce::String textCase;      // "title" cases a window's value the design's way
        /** THE BOX THE LIVE VALUE IS CENTRED IN, when the printed half of the
            control owns part of the window - an inline legend at the left, a
            caret at the right. Empty means the whole rect, less a small inset. */
        juce::Rectangle<float> textRect;
        /** A PATH EDITOR'S GEOMETRY, every number of it exported by the kit
            rather than worked out again here. `plot` above all: it is the
            drawing area inset from the glass by a point's own radius, so an
            endpoint is a whole circle inside the window - and the GRID is drawn
            in that same rect, because alt-drag snaps a point to the grid and the
            line you see has to be the line it lands on. Two halves each
            insetting by their own idea of a radius is how they stop being one
            line, which is the fault `cells` and `textRect` were added for. */
        juce::Rectangle<float> plot;
        int gridH = 8, gridV = 8;
        /** A STEP GRID'S OWN GEOMETRY, in the same `plot` rect above. `rows` and
            `cols` are the divisions, `beat` is how many columns make a heavier
            line, `cellGap` the air around a cell. `lane` is one row of BARS
            rather than a grid of cells, `span` allows a cell longer than one
            column, `gridBase` is the MIDI note of the bottom row (-1 = the rows
            are not pitches) and `playCol` the column sounding now (-1 = none).
            Every one of them pushed by the unit or exported by the kit; none of
            them worked out here, for the reason `plot` is not. */
        int rows = 12, cols = 16, beat = 4, gridBase = -1, playCol = -1;
        /** setGridRowLabels: names the rows, bottom first, and bands them. */
        juce::StringArray rowLabels;
        float cellGap = 1.0f;
        bool lane = false, span = false;
        juce::Colour cellFill, cellCore, gridMajor, playInk, rowTint;
        /** A LIST'S OWN GEOMETRY, in the same `plot` rect. `rows` above is how
            many fit in ONE column; `columns` is how many columns the rows flow
            down, newspaper fashion. `rowH` is one row's height and `headerH` the
            printed heading band's, both exported by the kit so the row a click
            lands on is the row that was drawn. `listTop` is the first row
            showing and `listLen` the total the host declared. */
        int columns = 1, listTop = 0, listLen = 0;
        float rowH = 16.0f, headerH = 0.0f, colW = 0.0f;
        /** The dimmer of a list's two inks: the bank letter, the number, a group
            heading. Two inks rather than one alpha, because a heading and a
            dimmed name are different things and a reader has to tell them apart. */
        juce::Colour textDim;
        float pointR = 4.0f, handleR = 3.0f, lineWidth = 2.0f, gridOpacity = 0.16f;
        juce::Colour pointFill;
        /** THE WINDOW'S MODE (PathMode), the moving dot, and the trail a
            kPathTrace window shows. All pushed by the unit. */
        int pathMode = 0;
        bool cursorOn = false; float cursorX = 0.5f, cursorY = 0.5f;
        std::vector<float> trail;
        std::vector<float> backdrop;   // the audio drawn behind the shape
        // A METER THAT IS A LADDER, not a bar: `segRows` cells stacked bottom to
        // top, each lit in its own colour from `segColours` (bottom row first)
        // and filling `segFill` of its row's pitch, which is what leaves the gap
        // between the discs the panel prints. Zero means the continuous bar.
        int segRows = 0; float segFill = 0.43f; juce::Array<juce::Colour> segColours;
        /** THE STEEL THEME'S LIVE HALF, every field of it optional and every one
            read from the layout rather than decided here, so the printed and the
            live halves of a window cannot disagree about what a lit thing looks
            like. A layout that carries none of them draws exactly as it did.

            `lcd`: the window is a seven-segment display. The value is drawn in
            `lcdFont` (DSEG7 for a number, DSEG14 for a word) in `lcdInk` over a
            ghost of every unlit segment in `lcdInkDim` - the ghost is what makes
            it read as a real LCD rather than as text in an odd typeface.
            `pill`: a tab strip's lit cell is a fully rounded pill with a halo of
            `glowLit` fading over `glowWidth` outside it, the name in `textLit`
            and the ring glyph in `ringLit`, and no notch.
            `trace` / `traceGlow`: a display's line is drawn twice, a wide soft
            glow under a crisp trace; empty means the plain single stroke. */
        bool lcd = false; juce::Colour lcdInk, lcdInkDim; juce::String lcdFont;
        bool pill = false; juce::Colour textLit, ringLit, glowLit; float glowWidth = 0.0f;
        /** WHETHER THIS TAB STRIP CAN BE DRAGGED ONTO A CONTROL. An LFO tab is a
            MODULATION SOURCE and dragging it is how a route is made; a filter
            tab is only a page selector, and offering the gesture on one promises
            something that cannot happen. The author, 2026-09-18: "I shouldnt be able to
            drag the filters tabs to anything. they are just tabs." True unless
            the layout says otherwise, so every strip that was draggable stays. */
        bool assignable = true;
        juce::Colour trace, traceGlow; float traceWidth = 0.0f, traceGlowWidth = 0.0f;
        /** A METER'S PEAK HOLD, the line that falls slowly behind the bar. */
        float value2 = 0.0f;
        juce::Rectangle<float> drawn, hit;              // design units
        /** WHAT THE MATRIX IS DOING TO THIS CONTROL, see setModulation. All in
            the control's own 0..1, relative to `value`; `on` false draws the
            control exactly as an unmodulated one. */
        struct Mod { bool on = false; float offset = 0.0f, lo = 0.0f, hi = 0.0f; };
        Mod mod;
        /** WHICH PAGE OF ITS SECTION THIS CONTROL IS ON, and -1 for a control
            that is always there. A paged section stacks its wells in z rather
            than down the column, so only one page's controls are drawn and only
            that page's are hit-tested - which is the whole mechanism. */
        int page = -1;
        /** WHAT A TAB STRIP SWITCHES, when that is not where it LIVES.

            `pageSection` + `page` say where a control is drawn. A tab strip
            also SELECTS something, and for most strips those are the same
            thing - a filter tab lives in the filter section and pages it. But
            EchoFold's LFO and ENV selectors live ON the LFO/ENV page and
            select which of four modulators that page is showing, so paging
            their own section would switch the page out from under them (which
            is exactly what "clicking LFO 2 goes to COMP" was).

            Empty means the strip selects what it lives in, as it always did. */
        juce::String selects;
        /** This strip's offset into a selection shared with other strips: its
            first cell IS selection `selectBase`. 0 for a strip that owns its
            selection outright, which is every strip that is not sharing. */
        int selectBase = 0;
        juce::String pageSection;
        /** WHICH VIEW OF THE WHOLE PLATE this control is on - a SECOND and
            independent axis. A view swaps the sections; a page swaps the wells
            inside one section. A control is visible only when both agree, and
            -1 means "every view", which is what furniture like a top bar is. */
        int view = -1;
        juce::String viewSection;
        /** WHICH BAND (dynamic well) THIS CONTROL BELONGS TO - the third axis.
            Empty for every control outside one. A band's controls are drawn and
            hit only while the band is stacked, at coordinates the reflow has
            already moved wholesale, so nothing else here branches on it. */
        juce::String band;
        /** A LIST ROW'S BAND: the dynamic well this control follows in the
            ORDER (a chain list's handle and lamp), not the well it is in. */
        juce::String listOf;
        juce::StringArray pageNames;                    // pageSelect only
        /** A KEY'S OWN TWO FACTS, straight out of the export and never derived
            here. `midi` is what the key plays (-1 on anything that is not a
            key) and `black` is what it is cut from - which the painter needs as
            well as the picture does, because velocity is measured DOWN the key
            and a black key is shorter than the white it sits over. */
        int midi = -1;
        bool black = false;
    };

    /** ONE PAGED SECTION: where its page area is and what covers it.

        The author's design: *"By default it shows what people use. Cutoff/Reso/Blend.
        Then we have a dropdown, change it and then the Filters section changed
        to that page."* A section's height stops being the sum of its wells and
        becomes the tallest one of them, which is what pays for a machine having
        more controls than its plate has room for.

        PAGE 0 IS IN THE BASE BACKGROUND and has no image. That is what makes a
        panel that never touches a page select load and draw exactly what it drew
        before pages existed: one PNG, no compositing, no branch on the hot path
        beyond an empty vector. */
    struct Page
    {
        juce::String id, select;
        juce::Rectangle<float> rect;                    // design units
        juce::StringArray names;
        std::vector<juce::Image> images;                // [0] is empty: it is the base
        int current = 0;
        /** The view this section is on (-1: every view) and the page record that
            switches views, so a hidden view's page images stay unpainted. */
        int view = -1;
        juce::String viewSection;
        /** The current page already rescaled to the size it is drawn at, cached
            per scale exactly as the background is. Resampling it on every
            repaint instead made its type look different from the background's,
            because the two went through different resamplers. */
        juce::Image scaled;
        int scaledFor = -1, scaledPage = -1;
    };

    /** Which page a section is showing, and move it. `sectionId` is the id the
        export wrote; an unknown one is ignored rather than asserted, because a
        machine may share a binding table with a panel that has no such page. */
public:
    [[nodiscard]] int pageOf (const juce::String& sectionId) const noexcept;
    /** Whether a control is showing right now: its view, its page and its band
        all have to agree. A probe pressing controls needs this or it presses
        things that are not there and reports them dead. */
    [[nodiscard]] bool isVisibleCtl (const juce::String& id) const noexcept;
    /** How many positions a control's menu offers, 0 when it has none. A probe
        uses it to tell a DROPDOWN from a dead window: pressing a dropdown opens
        a menu and changes nothing until a row is chosen, which reads exactly
        like a control that does nothing. */
    [[nodiscard]] int choiceCountOf (const juce::String& id) const noexcept;
    /** THE POINTER SAYS WHICH WAY A CONTROL MOVES before it is touched: a
        left-right arrow over a horizontal fader, up-down over everything that
        sweeps vertically. */
    juce::MouseCursor getMouseCursor() override;
    /** What a control is SHOWING right now. A probe that presses an arrow has to
        read the window beside it, not only the parameter behind it: the two are
        different things and it is the window the player is looking at. */
    [[nodiscard]] juce::String valueTextOf (const juce::String& id) const noexcept;
    void setPage (const juce::String& sectionId, int page);
    /** WHERE A NAMED PAGE SITS IN ITS SECTION'S LIST, or -1 if there is no such
        page. A unit that jumps to a page by NAME - "show me the ARP view" - would
        otherwise have to hard-code the index the layout happened to give it, and
        an index written down a second time is the one that goes stale the day a
        page is inserted. The names are the export's own, from the page select or
        the tab strip that drives the section. */
    /** EVERY PAGE THIS SECTION HAS, in the order the strip draws them. `pageNamed`
        answers "which index is this name" and a caller that wants to VISIT them
        all had to guess how many there were; a count written down twice is the
        one that goes stale the day a page is inserted. Empty when the plate has
        no such section. */
    [[nodiscard]] juce::StringArray pageNames (const juce::String& sectionId) const;
    [[nodiscard]] int pageNamed (const juce::String& sectionId, const juce::String& name) const;
    /** The pages this panel has, for a unit that wants to drive them itself -
        the shell's own tab strip, a MIDI CC, an automation lane. */
    [[nodiscard]] const std::vector<Page>& pages() const noexcept { return pageList; }
    /** MARK A PAGE SELECT WHEN SOMETHING BEHIND IT IS NOT AT ITS DEFAULT.

        This is what makes a page safe to have. A control on a page you cannot
        see is one step from a control nobody can reach, so the selector carries
        a dot and a patch cannot have a secret: the section says there is
        something on another page and you go and look. The unit sets it, because
        only the unit knows what its parameters' defaults are. */
    void setPageMark (const juce::String& sectionId, bool anyOffDefault);
private:

    /** ONE BAND: a dynamic well, its gate, its authored place and its raster. */
    struct Band
    {
        juce::String id, gate;
        juce::Rectangle<float> rect, imageRect;         // authored, design units
        juce::Image img, scaled;
        int scaledFor = -1;
        bool shown = false;
        float dy = 0.0f;                                // where the stack put it, from authored
    };
    /** ONE SECTION'S BANDS: the area they stack in and the gap between them.
        `order` is the runtime stacking order (indices into `bands`); it starts
        as the export's order and setBandOrder replaces it. */
    struct BandGroup
    {
        juce::String section, viewSection;
        int view = -1;
        juce::Rectangle<float> rect;
        float gap = 8.0f;
        std::vector<Band> bands;
        std::vector<int> order;
        /** THE RACK SCROLLS. A stack taller than its area used to drop the
            bands that did not fit; now the stack is placed from `rect.y -
            scroll`, the controls are clipped to the area, and the wheel over
            the area moves it. The author, 2026-09-19, on the TAPE strip growing its
            lane windows: "we just add a scroll bar in the FXs rack so if it
            becomes too long the user just scrolls." */
        float scroll = 0.0f;
        float stackH = 0.0f;   // the height of the shown bands and their gaps
    };
    [[nodiscard]] const BandGroup* groupOfBand (const juce::String& wellId) const noexcept;
    void paintBandScrollbars (juce::Graphics&, float k);
public:
    /** SCROLL A CONTROL INTO ITS RACK'S AREA, the way a player's wheel would,
        and return where it is drawn afterwards (design units). A control that
        is not on a band, or is already inside, is left where it is. A probe
        that clicks every control where it is drawn needs this for the rows a
        tall strip puts below the fold. */
    juce::Rectangle<float> reveal (const juce::String& id);
private:
    [[nodiscard]] bool bandGroupShowing (const BandGroup& gp) const noexcept;
    std::vector<BandGroup> bandGroups;
    bool forceBandGates = false;    // selftest: paint every band as if its gate were on
    bool bandHitsStale = false;     // selftest: a reflow that forgets the hit rects
    [[nodiscard]] bool gateOn (const juce::String& controlId) const noexcept;
    void reflowBands();
    void shiftBandControls (const juce::String& wellId, float dy);
    void paintBands (juce::Graphics&, float k);
    /** Reflow when this control id gates any band; a no-op for every other id. */
    void maybeReflowGate (const juce::String& id);

    void load();
    bool loadNative (const juce::String& prefixedLayout);
    void loadSpec();
    void finishCtl (Ctl&);
    Sprite* spriteFor (const juce::var& v);
    juce::Image loadPng (const juce::String& file) const;
    const char* blob (const juce::String& file, int& size) const;
    juce::String asset (const juce::String& file) const;
    const juce::Image& scaledSprite (Sprite&, float k);
    [[nodiscard]] juce::Rectangle<float> boundsOf (const Ctl&) const;   // design units, for repaint
    [[nodiscard]] int hitCtl (juce::Point<float> designPt) const;
    /** The scale everything is drawn at. Width-driven as it always was, raised to
        the legibility floor when the panel declares one, and never used to make a
        panel fit: see the viewport note above. */
    [[nodiscard]] float scale() const noexcept;
    static bool isValueCtl (const Ctl&) noexcept;
    Ctl* mainScreen();
    void repaintCtl (const Ctl&);
    void emitValue (Ctl&);
    void timerCallback() override;
    void drawKnob (juce::Graphics&, Ctl&, float k);
    void drawFader (juce::Graphics&, Ctl&, float k);
    void drawToggle (juce::Graphics&, Ctl&, float k);
    void drawSpritePair (juce::Graphics&, Ctl&, float k, bool on);
    void drawScreen (juce::Graphics&, Ctl&, float k);
    void drawReadout (juce::Graphics&, Ctl&, float k);
    void drawScope (juce::Graphics&, const Ctl&, float k);
    void drawField (juce::Graphics&, Ctl&, float k);
    /** ONE seven-segment window, for a field and a readout alike: the ghost of
        every unlit segment, then the glow, then the lit value, all in the LCD
        face at a size the window's own height decides. Falls back to the sans
        when the host handed over no face for this window. */
    void drawLcd (juce::Graphics&, const Ctl&, juce::Rectangle<float> box,
                  const juce::String& text, juce::Justification just, float k);
    [[nodiscard]] juce::Typeface::Ptr lcdTypefaceFor (const Ctl&) const;
    void drawTabStrip (juce::Graphics&, Ctl&, float k);
    void drawHandles (juce::Graphics&, const Ctl&, float k);
    juce::Point<float> handlePoint (juce::Rectangle<float>, const Handle&) const;
    /** Device pixels per logical pixel on the screen this panel is on. Every
        raster is scaled to DEVICE pixels and drawn into its LOGICAL rect, so the
        exporter's 2x render lands one to one on a 2x display instead of being
        halved and doubled - which was the whole of the blur. */
    [[nodiscard]] float deviceScale() const noexcept;
    /** Where the plate is drawn when it is smaller than its window: centred. */
    [[nodiscard]] juce::Point<int> centringOffset() const noexcept;

    // --- the drawn path -----------------------------------------------------
    void drawPathEditor (juce::Graphics&, const Ctl&, float k);
    void drawStepGrid (juce::Graphics&, const Ctl&, float k);
    /** The cell a design-space point falls in. `col` is -1 when the point is
        outside the plot rect, which is what makes a drag off the window's edge
        stop painting rather than wrap to the other side. */
    [[nodiscard]] juce::Point<int> gridCellAt (const Ctl&, juce::Point<float> designPoint) const noexcept;
    /** Where one cell is drawn, in design units, from the SAME plot rect a click
        is resolved against - so the cell you see and the cell a click lands on
        are one cell. */
    [[nodiscard]] juce::Rectangle<float> gridCellRect (const Ctl&, const GridCell&) const noexcept;
    /** The row index under a point in design units, -1 outside the plot or past
        the end of the list. ONE function, used by the paint pass and by the
        click, for the reason gridCellAt exists: two ideas of where a row is is
        how the name you see stops being the name you get. */
    [[nodiscard]] int listRowAt (const Ctl&, juce::Point<float>) const noexcept;
    [[nodiscard]] juce::Rectangle<float> listRowRect (const Ctl&, int visibleIndex) const noexcept;
    Ctl* listCtl (const juce::String& id);
    void drawList (juce::Graphics&, const Ctl&, float k);
    /** Not [[nodiscard]]: an EMPTY pattern is a legitimate answer, so most
        callers want the vector and not the count. */
    int gridFetch (const Ctl&, std::vector<GridCell>&) const;
    void gridEmit (const Ctl&, std::vector<GridCell>&);
    void gridBeginGesture (Ctl&, juce::Point<float> designPoint, const juce::ModifierKeys&);
    void gridDragTo (Ctl&, juce::Point<float> designPoint);
    void gridEndGesture();
    [[nodiscard]] Ctl* gridCtl (const juce::String& id);
    void drawMeter (juce::Graphics&, Ctl&, float k);
    /** THE BEND LAW, and it MUST be the same function the engine reads the path
        with - megasynth's `pathCurve` in src/engine/LfoPath.h. It is copied and
        not included because this library is shared by every machine and cannot
        depend on one machine's engine; that is a real cost, so it is written
        down here: if the two ever differ, a path SOUNDS different from the
        curve that was drawn, which is the one class of fault nobody debugs by
        looking at the picture. The law warps TIME rather than level, which is
        what keeps both ends exactly on their points however hard it is bent. */
    [[nodiscard]] static float pathBend (float t, float curve) noexcept;
    /** The curve that puts a segment's MIDPOINT at `f` of the way from its left
        level to its right one - `pathBend(0.5, curve)` solved for curve. This is
        what makes a bend handle follow the pointer rather than approach it. */
    [[nodiscard]] static float pathBendForMid (float f) noexcept;
    [[nodiscard]] static float pathValueAt (const std::vector<PathPoint>&, float x) noexcept;
    /** A path position (x 0..1, y 0..1 UP) as a point in DESIGN pixels, and back.
        Both go through `plot`, so what is drawn and what is grabbed are one map. */
    [[nodiscard]] juce::Point<float> pathToDesign (const Ctl&, float x, float y) const noexcept;
    [[nodiscard]] juce::Point<float> pathFromDesign (const Ctl&, juce::Point<float>) const noexcept;
    /** Where a segment's bend handle sits, in path coordinates: the segment's
        temporal middle ON the curve for a graph, its parametric middle on the
        bulged line for a figure. */
    [[nodiscard]] juce::Point<float> pathMid (const Ctl&, const std::vector<PathPoint>&, int seg) const noexcept;
    /** THE TAB STRIP'S GEOMETRY, one place: a cell's rect (the kit's, or an
        even share), the ring glyph's centre and radius by the kit's own
        formula, and a grip's rect. Drawing and hit-testing both read these, so
        a grip is caught where it is seen. All in design units. */
    [[nodiscard]] juce::Rectangle<float> tabCellRect (const Ctl&, int cell) const;
    [[nodiscard]] juce::Point<float> tabRing (const Ctl&, juce::Rectangle<float> cell, int cellIndex,
                                              float& radius, bool& room) const;
    [[nodiscard]] juce::Rectangle<float> tabGripRect (const Ctl&, juce::Rectangle<float> cell, int handle) const;
    [[nodiscard]] int tabHandleAt (const Ctl&, int cell, juce::Point<float> design) const;
    [[nodiscard]] juce::String tabDragName() const;
    [[nodiscard]] int pathFetch (const Ctl&, std::vector<PathPoint>&) const;
    void pathEmit (const Ctl&, std::vector<PathPoint>&);
    /** Replace one grid column with a segment running from `y0` to `y1`. What
        Flat, Ramp Up, Ramp Down and a shift-click step all are. */
    static void pathFillCell (std::vector<PathPoint>&, int col, int gridV, float y0, float y1);
    void pathBeginGesture (Ctl&, juce::Point<float> design, const juce::ModifierKeys&);
    void pathDragGesture (Ctl&, juce::Point<float> design, const juce::ModifierKeys&);
    void pathEndGesture();
    void pathDoubleClick (Ctl&, juce::Point<float> design);
    struct PathPopout;
    friend struct PathPopout;
    std::unique_ptr<PathPopout> popout;
    // the layer (openLayer): which view, where it is drawn, and its image cache
    int layerView = -1;
    juce::String layerSectionId;
    juce::Rectangle<float> layerRect;          // design units
    juce::Image layerScaled; int layerScaledFor = -1, layerScaledView = -1;
    [[nodiscard]] bool onLayer (const Ctl& c) const noexcept
    { return layerView >= 0 && c.view == layerView && c.viewSection == layerSectionId; }
    void drawCtl (juce::Graphics& g, Ctl& c, float k);
    void paintLayer (juce::Graphics& g, float k);
    void pathApplyTool (Ctl&, juce::Point<float> design, int tool);
    [[nodiscard]] Ctl* pathCtl (const juce::String& id);
    /** The LCD faces the host handed over, see setLcdTypefaces. */
    juce::Typeface::Ptr lcdDigits, lcdAlnum;
    ScopeSource scopeSource;
    /** A SECOND, DIM TRACE BEHIND THE FIRST, from the same seam. Serum draws
        ENV 1 faintly behind ENV 2 to 4 (guide p. 185: "the envelope of ENV 1 is
        faintly visible (in gray) in the background") so the one that opens the
        amplifier is always in view. The unit decides what a window's ghost is;
        return 0 for none. Drawn on the same axes as the main trace. */
    ScopeSource ghostSource;
    std::array<float, 2048> traceBuf {};
    HandleSource handleSource;
    std::array<Handle, 16> handleBuf {};
    /** THE RADIUS A POINT IS DRAWN AT, in design px, and the radius its grab
        area uses is a multiple of it - a point you can see but not catch is
        worse than no point. */
    static constexpr float kHandleRadius = 4.5f;
    juce::String dragScope, dragHandle;

    // --- the drawn path's state ---------------------------------------------
    GridSource gridSource;
    ListSource listSource;
    /** THE GESTURE IN FLIGHT on a step grid: which window, what the first click
        decided the gesture IS (paint a cell in, erase, or set a lane's value),
        and where a painted cell started so a drag to the right lengthens it
        rather than starting a new one every column. */
    juce::String dragGrid;
    enum GridGesture { kGridNone = 0, kGridPaint, kGridErase, kGridLevel };
    int gridGesture = kGridNone, gridAnchorCol = -1, gridAnchorRow = -1;
    PathSource pathSource;
    int pathToolNow = kPathPoint;
    /** The editor a gesture is happening on, empty when none is. */
    juce::String dragPath;
    /** THE PATH AS IT WAS WHEN THE POINTER WENT DOWN. A drag applies a DELTA to
        this rather than to whatever the last frame produced, so a gesture cannot
        accumulate its own rounding - which is how a point dragged slowly ends up
        somewhere a point dragged quickly does not. */
    std::vector<PathPoint> pathGrab;
    int dragPoint = -1;      // the point being moved, -1 for none
    int dragBend  = -1;      // the segment whose bend handle is held, -1 for none
    float bendStart = 0.0f;
    /** WHICH POINTS A DRAG MOVES TOGETHER. Indices into the path, and they are
        dropped on any STRUCTURAL edit: adding or removing a point renumbers
        everything after it, and a stale index would move a point the player
        never selected. */
    std::vector<int> pathSel;
    /** WHICH EDITOR THE SELECTION BELONGS TO. A panel can carry more than one
        path window - megasynth has one per LFO - and indices into one path mean
        nothing in another, so without this a selection made on LFO 1 lights up
        whichever points happen to share those numbers on LFO 2. */
    juce::String pathSelOn;
    bool pathBand = false;
    juce::Point<float> bandFrom, bandTo;                 // design units
    /** The last grid column a tool painted, so dragging a tool across the window
        fills each column once instead of fighting itself every mouse-move. */
    int pathCell = -1;

    /** THE DISPLAYS' OWN CLOCK. The panel already has a Timer and it is a
        one-shot for the edit display, so this is a second one rather than a
        shared one with two jobs and a flag - the bug that would follow is
        obvious and nobody would find it for a month. It only runs when there is
        something to draw: a source AND at least one display. */
    struct Refresher : juce::Timer
    {
        explicit Refresher (NativePanel& p) : panel (p) {}
        void timerCallback() override { panel.repaintDisplays(); }
        NativePanel& panel;
    };
    Refresher refresher { *this };
    /** The panel itself, drawn in PANEL coordinates. paint() puts the window over it. */
    void paintPanel (juce::Graphics&, float k);
    void paintPages (juce::Graphics&, float k);
    [[nodiscard]] bool visible (const Ctl&) const noexcept;
    std::vector<Page> pageList;
    std::map<juce::String, bool> pageMark;
    void paintPlaceholder (juce::Graphics&, float k);
    void paintViewportCues (juce::Graphics&);
    /** 0 neither, 1 the horizontal bar, 2 the vertical one. */
    [[nodiscard]] int overBar (juce::Point<float>) const;
    void thumbTo (bool horizontal, float coord);
    void clampView();
    [[nodiscard]] int maxViewX() const noexcept;
    [[nodiscard]] int maxViewY() const noexcept;
    /** The scrollbar tracks and thumbs, in component pixels. Empty when that axis fits. */
    [[nodiscard]] juce::Rectangle<float> barTrack (bool horizontal) const;
    [[nodiscard]] juce::Rectangle<float> barThumb (bool horizontal) const;
    [[nodiscard]] float barThickness() const noexcept;
    /** The sprite/background rescale cache key. It is the SCALE, not the width: with
        a floor in force those two stop agreeing and a width key then serves every
        panel the wrong-sized sprites. */
    [[nodiscard]] int scaleKey() const noexcept;
    static juce::Colour colourOf (const juce::var&, juce::Colour fallback);

    PanelAssets assets;
    juce::String machine;
    // Fallbacks only: the real numbers come from the export's or the spec's own
    // "size", and every unit's are different. A hard-coded assertion on them
    // lived in twenty-four of the copies this replaced and fired on any panel
    // that was not the M1's.
    int designW = 1240, designH = 486;
    float assetScale = 2.0f;   ///< the scale the pictures were rendered at
    bool artwork = false;                 // false while the spec placeholder is up
    juce::String panelTitle { "PANEL" };
    juce::Image background, bgScaled; int bgScaledFor = -1;
    juce::Colour ground { 0xff1b2230 }, ink { 0xffe0a34a };
    std::map<juce::String, Sprite> sprites;
    std::vector<Ctl> ctls;

    /** Is this control drawn INSIDE a screen? Then it is a segment of that
        display rather than a key, however it is typed - which is how a machine
        with a graphic LCD builds one out of the controls the format has. */
    [[nodiscard]] bool isInsideAScreen (const Ctl& c) const noexcept;
    [[nodiscard]] juce::Colour lcdInk() const;
    void drawGlassText (juce::Graphics&, const Ctl&, float k) const;
    std::map<juce::String, int> index;
    int dragIdx = -1; float dragStartValue = 0; juce::Point<float> dragStart; bool dragMoved = false;
    int bandDragGroup = -1, bandDragBand = -1; bool bandDragging = false; juce::Point<float> bandPressAt;
    bool bandGripEnabled = true;
    bool listDrag = false;                 // the drag began on a handle, not a strip
    std::map<juce::String, float> listY0;  // a band's list row, authored centre y
    std::map<juce::String, float> listDy;  // where the order has put that row
    /** MOVE THE LIST ROWS TO THE ORDER: row k of the list shows the band at
        order position k. The rows' authored positions are the slots. */
    void reflowLists();
    void shiftListControls (const juce::String& wellId, float d);
    void drawHandle (juce::Graphics&, Ctl&, float k);
    int hoverIdx = -1;
    /** THE NOTE THE HAND IS HOLDING, -1 for none, and the whole of what a
        glissando needs. A drag across the bed is one gesture and one note at a
        time: crossing into a new key releases the old one and plays the new,
        which is what a finger dragged along a keybed does and what every
        hardware keyboard's own scan produces. Holding the note rather than the
        control index means a republished layout cannot leave a note sounding
        with nothing to turn it off. */
    int noteHeld = -1;
    void keyGesture (juce::Point<float> design);
    void keyRelease();
    [[nodiscard]] float keyVelocity (const Ctl&, juce::Point<float> design) const noexcept;
    // --- a tab-strip cell being dragged onto a control -----------------------
    // The strip that was pressed and which of its cells, -1 for none. The press
    // is only a CANDIDATE until the pointer has moved kTabDragPx; before that a
    // release is the click it always was.
    int tabPressIdx = -1, tabPressCell = -1;
    int tabPressHandle = 0;                        // 0 the cell or its X grip, 1 the Y grip
    bool ignoreTabHandles = false;                 // selftest only
    juce::Point<float> tabPressAt;                 // component px, where it went down
    bool tabDragging = false;
    juce::Point<float> tabDragPos;                 // component px, where the pointer is now
    int tabDropIdx = -1;                           // the control under it that accepts a drop, -1 none
    /** Four pixels: under a hand that is clicking, not over a hand that is
        dragging. A tab is 14 design px tall, so a drag has left it long before
        it could be mistaken for a wobble. */
    static constexpr float kTabDragPx = 4.0f;
    /** Where the source's name is drawn beside the pointer, component px. */
    [[nodiscard]] juce::Rectangle<int> tabLabelBounds() const;
    void paintModDrag (juce::Graphics&);
    void endTabDrag();
    // The viewport. `viewX/viewY` are component pixels, always integers so a
    // scrolled sprite is still drawn on a pixel boundary and stays sharp.
    int viewX = 0, viewY = 0;
    float minLegend = 0.0f;      // screen px the smallest legend may not go under; 0 = no floor
    float labelDesign = 0.0f;    // theme.minSize (else labelSize), design units
    int barDrag = 0;             // 0 none, 1 horizontal thumb, 2 vertical thumb
    bool panning = false;
    juce::Point<float> viewDragFrom; juce::Point<int> viewDragView;
    bool ignoreViewForHits = false;   // selftest only
    juce::StringArray restLines, editLines; bool showingEdit = false;
    /** The screen id most recently written by id, for screenLine(). */
    juce::String lastWritten;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NativePanel)
};

} // namespace synthkit
