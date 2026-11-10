// The FILTER component's response screen and its six drag points.
//
// MOVED OUT OF MEGASYNTH, 2026-09-26, where it was ~250 lines of MegaSynthEditor
// members (filterDb, fillFilterCurve, fillHandles, handleDragged, applyFilterMacro,
// macroX/macroY and their constants). A component is a WHOLE module of a synth, and
// a filter whose screen stayed behind in one machine is a filter every other machine
// has to draw again. The author, 2026-09-26: "everything including the screen and dragpoints
// was supposed to be in the global component."
//
// IT KNOWS NO HOST. No JUCE, no APVTS, no parameter tree: it reads and writes through
// two callbacks keyed by the component's own LOCAL names, so a synth wires it once
// and the component never learns what that synth's ids are called. That is the same
// seam that lets two machines with different patch formats share one filter.
//
//     cutoff  resonance  shape  width  drive  slope  env_amount
//
// CUTOFF IS IN HERTZ HERE. The display axis is ten octaves from 20 Hz, so the screen
// has to be handed a frequency. A machine whose cutoff parameter is a 0..1 control
// converts in its adapter, with its own law, and nothing in here changes - which is
// how Solera keeps `30 + x^2 * 12000` and its 319 stored patches keep sounding the
// same. The component owning the curve does not mean the component owning the range.
//
// EVERY POINT SITS ON THE CURVE, read from the same function the window draws, so a
// point can never be somewhere the response is not and a drag cannot jump under the
// finger. The macro point is the exception and floats above it on purpose: a point
// that stands for the whole filter does not belong on any one part of the response.
//
// STILL TO COME: A LIVE WAVEFORM UNDER THE CURVE. The author, 2026-09-26: "I want to be able
// to see a live waveform generated in the filter display so when you manipulate the
// filter controls or drag points you can actually see the sound change shape
// according to the filter... should probably be added to the global filter module."
//
// The curve is a claim about what the filter WOULD do; a waveform is what it DID, and
// db() below cannot show either the drive folding a wave over or the resonance
// ringing after the corner, because it is a display model of the magnitude response
// and nothing else. So the wave has to be a tap of the real output: behind the curve
// and dimmer, held and faded when nothing is playing rather than flashing empty, and
// fed off the audio thread through a lock-free ring.
//
// THE OPEN QUESTION IS WHERE THE TAP COMES FROM, and it has to be answered without
// breaking the seam above. Audio is a third thing beside Get and Set; the honest
// options are a third callback the host fills or the host pushing frames in. A screen
// that reaches into the audio engine itself is the seam gone. Full note in Solera's
// docs/filter-swap-checklist.md section 6.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>

#include "../dsp/FilterModule.h"   // kCutoffLoHz / kCutoffHiHz

namespace dsp { namespace filter {

/** One draggable point, in window units: x and y are 0..1 across and up the glass,
    and `size` is a multiplier on the drawn radius (the macro point is 2.2). */
struct ScreenHandle
{
    const char* name = "";
    float x = 0.0f, y = 0.0f, size = 1.0f;
};

class Screen
{
public:
    /** Read and write the host's parameters by the component's LOCAL name. */
    using Get = std::function<float (const char*)>;
    using Set = std::function<void (const char*, float)>;

    /** THE SOUND ITSELF, and this is the third seam.
        The host fills `dest` with up to `max` samples of the FILTER'S OUTPUT, most
        recent last, and returns how many it wrote. Returning 0 means nothing is
        playing.

        A PULL, NOT A REACH. The component never touches an audio buffer, a device or
        a thread: it asks, at whatever rate it is being drawn, and the host decides
        where that comes from and how it got off the audio thread. That is the same
        seam Get and Set are, for the same reason - it is what lets two machines with
        different patch formats and different engines share one screen. A screen that
        reached into the engine would have to know which engine. */
    using Wave = std::function<int (float*, int)>;

    Screen (Get getter, Set setter, Wave waver = {})
        : get (std::move (getter)), set (std::move (setter)), wave (std::move (waver)) {}

    // ---- THE WINDOW, STATED IN DECIBELS ----------------------------------------
    // Ten octaves across, kDbFloor to kDbCeil down, and the resonance peak reaches
    // the ceiling at the top of the knob - which is what makes the corner point
    // draggable to the very top of the glass. Stating the axis in dB rather than in
    // pixels is why the curve and the points cannot disagree about which pixel a
    // decibel is.
    static constexpr float kLo = kCutoffLoHz, kHi = kCutoffHiHz;   // dsp/FilterModule.h
    static constexpr float kDbFloor = -36.0f, kDbCeil = 24.0f;

    /** WHERE THE FOUR EXTRA POINTS SIT on the ten-octave axis. Named once, because
        the point that is DRAWN and the point that is DRAGGED must be the same place. */
    static constexpr float kDriveX = 0.06f;     ///< the passband, far left
    static constexpr float kShapeX = 0.94f;     ///< where the four responses differ most
    static constexpr float kSlopeDx = 0.20f;    ///< two octaves past the corner
    static constexpr float kWidthMin = 0.05f, kWidthMax = 0.30f;
    /** THE MACRO'S OWN CUTOFF RANGE, short of the parameter's extremes at both ends:
        a macro that can mute the instrument, or take the filter out of the audio
        altogether, is a macro nobody drags twice. */
    static constexpr float kMacroLo = 60.0f, kMacroHi = 16000.0f;

    static constexpr float clamp01 (float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    static constexpr float dbToUnit (float db) noexcept
    { return clamp01 ((db - kDbFloor) / (kDbCeil - kDbFloor)); }
    static constexpr float unitToDb (float u) noexcept
    { return kDbFloor + (kDbCeil - kDbFloor) * clamp01 (u); }
    static constexpr float widthDx (float width) noexcept
    { return kWidthMin + (kWidthMax - kWidthMin) * clamp01 (width); }

    /** The response in decibels at `x` (0..1 across the ten octaves), given the six
        controls that shape it. Static because the handle code calls it for a
        HYPOTHETICAL setting, not only for the live one.

        THIS IS A DISPLAY MODEL, NOT THE BLOCK'S MAGNITUDE RESPONSE, and that is worth
        knowing before anyone tries to reconcile the two: the filter has four
        topologies and a limiter in its feedback path, and none of that has a closed
        form. What this owes the player is a curve whose corner, slope, band and peak
        move exactly as the knobs do, so the picture teaches the controls. */
    [[nodiscard]] static float db (float x, float cutN, float res, float shp,
                                   float width, float drive, float slope) noexcept
    {
        // Octaves from the corner, over the display's ten-octave axis.
        const float d = (x - cutN) * 10.0f;
        // WIDTH IS THE BAND'S OWN SPREAD, so it scales the distance the shapes see: a
        // wide setting flattens every skirt, a narrow one sharpens it. 0.5 is the
        // parameter's default and is the unit case.
        const float w = 0.45f + 1.35f * clamp01 (width);
        const float dw = d / w;
        // SLOPE IS 6, 12 OR 24 dB/oct AND EVERYTHING BETWEEN: the exponent on the
        // rolloff, which is exactly what the number means.
        //
        // THE CLAMP TRACKS THE PARAMETER'S RANGE, and it has now been wrong in both
        // directions. The author, 2026-09-27, with a six-pole filter running: "on the screen. a 6
        // pole looks the exact same as 4 pole. but 4 pole shape looks alot different than 2
        // pole. WHY?" Because the ceiling was still 24 while the audio rolled off at
        // -38.86: the one control whose whole job is to show what the filter is doing was
        // the part of the machine nobody had told. That anchor was withdrawn the same day
        // and the third one moved to the BOTTOM of the travel, so the ceiling goes back to
        // 24 and the FLOOR is what now has to follow.
        //
        // A display model is only worth having while it tracks the thing it models, and the
        // lesson survives the anchor that taught it: when the parameter's range moves, this
        // clamp is the line to change.
        const float poles = std::clamp (slope, 6.0f, 24.0f) / 12.0f;         // 0.5 .. 2
        const float lpDb = dw <= 0.0f ? 0.0f : -6.0f * poles * dw;
        const float hpDb = dw >= 0.0f ? 0.0f :  6.0f * poles * dw;
        // A FIRST-ORDER NETWORK HAS NO BANDPASS AND NO NOTCH, so below 12 the band shapes
        // draw the 12 dB anchor rather than a half-pole skirt nothing can produce. That is
        // not a display convenience: a one-pole lowpass and highpass sum to the input, so
        // any blend of them is a shelf, and the engine answers a band request at slope 6
        // with the 12 dB anchor bit for bit. The picture agrees with the audio by using the
        // same floor the audio uses.
        const float bandPoles = std::max (poles, 1.0f);
        const float bpDb = -6.0f * bandPoles * std::fabs (dw);
        // NOTCH is the band REMOVED, so it is the bandpass inverted about unity
        // rather than a fourth hand-written shape.
        const float ntDb = -std::pow (2.0f, -std::fabs (dw) * 2.0f) * 30.0f;
        const float s = std::clamp (shp, 0.0f, 3.0f);
        float out = s < 1.0f ? lpDb + (bpDb - lpDb) * s
                  : s < 2.0f ? bpDb + (hpDb - bpDb) * (s - 1.0f)
                             : hpDb + (ntDb - hpDb) * (s - 2.0f);
        // THE CORNER LIFTS BY Q, in decibels, which is what resonance IS: a second
        // order section's magnitude at its own corner is Q. res 0 is 0 dB of lift and
        // res 1 is the ceiling, so the peak lands exactly on the top of the window
        // when the knob is at the top and the point can be dragged there.
        const float lift = res * kDbCeil;
        out += lift / (1.0f + dw * dw * (4.0f + 60.0f * res * res));
        // DRIVE lifts the whole passband, which is what it does to the sound: more
        // level and more harmonics through the band the filter is passing.
        out += clamp01 (drive) * 6.0f;
        return out;
    }

    /** The corner's place on the axis, 0..1, from a cutoff in hertz. */
    [[nodiscard]] static float cutNorm (float cutHz) noexcept
    { return std::log (std::clamp (cutHz, kLo, kHi) / kLo) / std::log (kHi / kLo); }

    // ---- WHAT THE WINDOW DRAWS -------------------------------------------------
    /** The curve, as `n` heights in 0..1. 256 points is the width of the glass at
        the scale these panels are drawn, so more is invisible. */
    int fillCurve (float* dest, int max) const
    {
        const int n = std::min (max, 256);
        if (n < 2) return 0;
        const float cutN = cutNorm (get ("cutoff"));
        const float res = get ("resonance"), shp = get ("shape");
        const float width = get ("width"), drive = get ("drive"), slope = get ("slope");
        for (int i = 0; i < n; ++i)
        {
            const float x = (float) i / (float) (n - 1);
            dest[i] = dbToUnit (db (x, cutN, res, shp, width, drive, slope));
        }
        return n;
    }

    /** THE SIX POINTS, computed from the parameters every time rather than
        remembered, so a point can never disagree with its knob. Order matters: the
        macro is LAST so it draws on top of the corner rather than fighting it for
        the same pixels. */
    int fillHandles (ScreenHandle* out, int max) const
    {
        if (out == nullptr || max <= 0) return 0;
        const float res = get ("resonance"), shp = get ("shape");
        const float width = get ("width"), drive = get ("drive"), slope = get ("slope");
        const float cutN = cutNorm (get ("cutoff"));
        const auto at = [&] (float x) { return dbToUnit (db (x, cutN, res, shp, width, drive, slope)); };

        int n = 0;
        const auto put = [&] (const char* name, float x, float y, float size = 1.0f)
        { if (n < max) out[n++] = { name, x, y, size }; };

        put ("cutoff", cutN, at (cutN));
        // THE PASSBAND, at the far left, is what DRIVE lifts.
        put ("drive", kDriveX, at (kDriveX));
        // THE SKIRT, a fifth of the window right of the corner: where the filter's
        // steepness is visible and nowhere else.
        const float sx = std::clamp (cutN + kSlopeDx, 0.06f, 0.97f);
        put ("slope", sx, at (sx));
        // THE BAND'S OTHER EDGE, left of the corner: WIDTH moves it, and its distance
        // from the corner IS the bandwidth.
        const float wx = std::clamp (cutN - widthDx (width), 0.02f, 0.94f);
        put ("width", wx, at (wx));
        // THE FAR RIGHT, where the four responses differ most: a lowpass has nothing
        // there and a highpass has everything.
        put ("shape", kShapeX, at (kShapeX));
        // AND THE MACRO, which moves all seven at once. The author: "Can we add another
        // bigger point on the filter that when dragged changes all the knobs at the
        // same time? I think you can discover some cool effects that way."
        put ("morph", macroX(), macroY(), 2.2f);
        return n;
    }

    [[nodiscard]] float macroX() const
    {
        const float cut = std::clamp (get ("cutoff"), kMacroLo, kMacroHi);
        return std::clamp (std::log (cut / kMacroLo) / std::log (kMacroHi / kMacroLo), 0.04f, 0.96f);
    }
    [[nodiscard]] float macroY() const { return std::clamp (get ("resonance"), 0.06f, 0.94f); }

    // ---- WHAT A DRAG WRITES ----------------------------------------------------
    /** Each point writes what it stands for, and each travels the WHOLE window. Every
        mapping here is inverted from the same response function the curve is drawn
        from, which is what makes a point land where the hand left it. */
    void dragged (const char* handle, float x, float y) const
    {
        if (handle == nullptr) return;
        const float ux = clamp01 (x), uy = clamp01 (y);
        const auto is = [handle] (const char* n) { return std::strcmp (handle, n) == 0; };

        if (is ("cutoff"))
        {
            set ("cutoff", kLo * std::pow (kHi / kLo, ux));
            // THE PEAK IS THE RESONANCE. At the corner the response is the passband
            // plus res * kDbCeil, so the inverse is the decibels above the passband
            // over that ceiling - which puts the top of the glass at exactly res 1.
            const float base = clamp01 (get ("drive")) * 6.0f;
            set ("resonance", clamp01 ((unitToDb (uy) - base) / kDbCeil));
            return;
        }
        // The passband's own level, 0 dB to +6 dB over the knob.
        if (is ("drive")) { set ("drive", clamp01 (unitToDb (uy) / 6.0f)); return; }
        if (is ("slope"))
        {
            // STEEPER IS LOWER. At kSlopeDx octaves past the corner a 12 dB/oct filter
            // is at -6 * 1 * dx*10 dB and a 24 is twice that, so the point's depth
            // inverts straight into dB per octave.
            const float octaves = kSlopeDx * 10.0f;
            const float want = std::max (0.5f, -unitToDb (uy)) / octaves;
            set ("slope", std::clamp (want, 6.0f, 24.0f));
            return;
        }
        if (is ("width"))
        {
            // ITS DISTANCE FROM THE CORNER IS THE BANDWIDTH, so the x it was dragged
            // to gives the width directly.
            const float cutN = cutNorm (get ("cutoff"));
            const float dx = std::max (0.0f, cutN - ux);
            set ("width", clamp01 ((dx - kWidthMin) / (kWidthMax - kWidthMin)));
            return;
        }
        if (is ("morph")) { applyMacro (ux, uy); return; }
        if (is ("shape"))
        {
            // AT THE FAR RIGHT the responses run lowpass (nothing there) to highpass
            // (everything), so the point's height chooses the morph the way the eye
            // reads it. It stops at highpass: the notch is past the top and is reached
            // by the knob, never by this point.
            set ("shape", std::clamp (uy * 2.0f, 0.0f, 2.0f));
            return;
        }
    }

    /** ONE GESTURE, SIX CONTROLS. It goes through the caller's own writer, so the
        macro takes exactly the path a knob does: one undo step per control, the host
        told, the panel refreshed.

        `key_track` is DELIBERATELY not among them - it is how the filter follows the
        keyboard, which is a property of the instrument rather than of the sound. Nor
        is `slope`, for the same reason and one more: pole count is WHICH filter rather
        than how it sounds, and it is the only one of these a panel gives detented
        buttons. The author, 2026-09-27: "when I drag the main morph point on the filter screen
        the 4-pole or 2-pole if selected turns off. they shouldnt be affected." A drag
        left the slope at 18 dB/oct with both lamps dark, which reads as the morph
        switching something off rather than moving a continuous control. */
    void applyMacro (float x, float y) const
    {
        x = clamp01 (x); y = clamp01 (y);
        // ACROSS: the cutoff over the macro's own range.
        set ("cutoff", kMacroLo * std::pow (kMacroHi / kMacroLo, x));
        // THE SHAPE MOVES ACROSS THE WHOLE TRAVEL, on a gentle S so the middle of the
        // drag - where a hand spends most of its time - is the bandpass rather than a
        // corner of it. It used to be (x - 0.55) / 0.45, which left the shape at ZERO
        // for the entire left 55 per cent: more than half the drag changed the corner
        // frequency and nothing else. The author: "when I drag to the left and downwards I am
        // expecting it to use the shape to decrease bandpass."
        const float t = x * x * (3.0f - 2.0f * x);              // smoothstep
        // AND THE VERTICAL REACHES THE SHAPE AT THE LEFT. The author: "when I am all the way
        // to the left on the filter screen when I pull the matrix point down I expect
        // the SHAPE to go up because I want to see the actual curve go down to the
        // left." He is describing the physics rather than the parameter: taking the
        // lows away IS a high-pass, which on this morph is a HIGHER shape. The lift is
        // strongest at the bottom left and vanishes at the right, where x has already
        // carried the morph past high-pass on its own.
        const float lift = (1.0f - y) * (1.0f - x);
        set ("shape", std::clamp (3.0f * t + 2.0f * lift, 0.0f, 3.0f));
        // UP: resonance, then the three that make resonance mean something.
        set ("resonance", y);
        set ("drive", y * y);                                   // only bites at the top
        // WIDTH ANSWERS BOTH AXES: narrower as resonance rises, and narrower toward
        // the left, so dragging down and left closes the band instead of leaving it
        // wide. A band the same width wherever the hand goes is why the sweep could
        // not be felt.
        set ("width", clamp01 ((0.85f - 0.65f * y) * (0.35f + 0.65f * x)));
        // AND THE ENVELOPE THROUGH ZERO, which is where the surprises are: the bottom
        // half closes as the note sounds, the top half opens.
        set ("env_amount", std::clamp ((y - 0.5f) * 2.0f, -1.0f, 1.0f));
    }

    // ---- THE SOUND, BEHIND THE CURVE -------------------------------------------
    /** The waveform to draw behind the response, as `n` points 0..1 up the glass with
        silence at 0.5. Returns 0 when there is nothing to draw and nothing held.

        WHY THE CURVE IS NOT ENOUGH. db() is a DISPLAY MODEL of the magnitude response:
        it can say where the corner is, and it cannot show the drive folding the top of
        a wave over or the resonance ringing after the corner, because neither is in it.
        The curve is a claim about what the filter WOULD do; this is what it DID.
        The author, 2026-09-26: "You should be able to see the actual sound/waveform playing in
        the background of the display so you can see it changing based on the filter."

        SILENCE IS NOT BLANK. With no note held the host returns 0 and the window would
        flash empty at the end of every chord, which reads as a fault rather than as
        quiet. The last frame is held and faded instead, here rather than in each host,
        so every machine fades the same way.

        AND IT FILLS THE GLASS. The author, 2026-09-27: "the waveform behind the filters display
        never takes up the full height of the screen." It never could: the trace was the
        raw sample mapped straight onto the window, and a patch that sits at 0.2 peak -
        which is most of them, because a synth leaves headroom for chords - drew a flat
        ribbon through the middle using a fifth of the height it had.

        A SCOPE AUTO-SCALES, and the thing being read here is SHAPE, not level: whether
        the drive is folding the top over, whether the resonance is ringing. The level is
        already on the meters. So the frame is normalised - but against a peak that FALLS
        SLOWLY rather than against this frame's own, because a per-frame normaliser makes
        the trace breathe at the frame rate and every note look identical. Fast to rise,
        slow to fall, so a struck note fills the window at once and a decay visibly
        shrinks inside it. */
    [[nodiscard]] int fillWave (float* dest, int max) const
    {
        if (dest == nullptr || max <= 0) return 0;
        const int n = max < kWaveMax ? max : kWaveMax;
        const int got = wave ? wave (scratch, n) : 0;
        if (got > 0)
        {
            held = got;
            fade = 1.0f;
            for (int i = 0; i < got; ++i) held_[i] = scratch[i];
        }
        else if (held > 0)
        {
            // ABOUT A SECOND AT FRAME RATE. Slow enough to read as the sound dying
            // away, fast enough that a held frame is never mistaken for a live one.
            fade *= 0.97f;
            if (fade < 0.01f) { held = 0; return 0; }
        }
        else return 0;

        // THE SLOW PEAK. Rises to this frame at once, falls by a twentieth a frame, so
        // roughly a second and a half from full scale to the floor at frame rate.
        float framePeak = 0.0f;
        for (int i = 0; i < held; ++i)
        {
            const float a = held_[i] < 0.0f ? -held_[i] : held_[i];
            if (a > framePeak) framePeak = a;
        }
        peak = framePeak > peak ? framePeak : peak * kPeakFall;
        // AND A FLOOR, or the room tone between notes is amplified into a full-height
        // scribble and the window looks broken when the instrument is silent.
        const float ref = peak > kQuiet ? peak : kQuiet;
        const float gain = kFill / ref;

        for (int i = 0; i < held; ++i)
        {
            const float v = held_[i] * gain * fade;
            const float u = 0.5f + 0.5f * (v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v));
            dest[i] = u;
        }
        return held;
    }

    /** How faded the held frame is, 1 while sounding. A host dims the trace by this so
        the fade is one decision rather than two that can disagree. */
    [[nodiscard]] float waveFade() const noexcept { return held > 0 ? fade : 0.0f; }

private:
    static constexpr int kWaveMax = 512;

    /** HOW THE TRACE IS SCALED. `kFill` is how much of the half-height the slow peak is
        drawn at - under one so a peak touches close to the frame without being clipped
        flat along it, which reads as the window being too small rather than as a loud
        note. `kPeakFall` is per frame. `kQuiet` is the level below which the signal is
        treated as silence rather than scaled up: -40 dBFS, quiet enough that a real
        decay still shrinks and loud enough that nothing amplifies noise. */
    static constexpr float kFill     = 0.92f;
    static constexpr float kPeakFall = 0.95f;
    static constexpr float kQuiet    = 0.01f;

    Get get;
    Set set;
    Wave wave;

    // THE HELD FRAME. mutable because drawing is const from the host's side and this is
    // display state, not parameter state - nothing here reaches a patch or the audio.
    mutable float scratch[kWaveMax] {};
    mutable float held_[kWaveMax] {};
    mutable int   held = 0;
    mutable float fade = 0.0f;
    mutable float peak = 0.0f;   // the slow peak the trace is normalised against
};

} } // namespace dsp::filter
