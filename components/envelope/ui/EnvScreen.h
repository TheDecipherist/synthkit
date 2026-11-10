#pragma once

// THE ENVELOPE, DRAWN AS THE ENVELOPE - not as a decoration of it.
//
// The author, 2026-09-27: "obviously the user should be able to see the actual filter env and
// amp env on the screen and drag them and their controls should also move the lines. It
// will make it easier to visibly see what the FILTER ENV and AMP ENV are actually doing."
//
// So this draws the curve the engine actually produces. A one-pole ADSR is the
// overwhelmingly common implementation and it has a closed form, so there is no reason to
// draw a decorative approximation of it:
//
//     attack   level = over * (1 - exp(-t/a)),  clamped at 1     (over = 1.02)
//     decay    level = s + (1 - s) * exp(-t/d)
//     sustain  level = s
//     release  level = s * exp(-t/r)
//
// AND THE TIME CONSTANT IS NOT THE SEGMENT DURATION, which is the fact that makes a naive
// envelope display wrong by a factor of four. The engine's attack chases 1.02 and finishes
// when it crosses 1.0, so it takes `a * ln(1.02/0.02)` = 3.93 time constants, not one. The
// decay and release finish at an epsilon rather than at their target, so they are
// `ln((1-s)/eps)` and `ln(s/eps)` constants. A display that plots one tau per segment shows
// an attack four times shorter than the one being heard.
//
// THE SAME THREE SEAMS AS THE FILTER SCREEN (components/filter/ui/FilterScreen.h): Get and
// Set speak the component's LOCAL names in the component's own units, and the host converts
// in the lambdas. Times are SECONDS here and sustain is 0..1. A host whose parameters are
// 0..1 knobs converts in one place, once.
//
//     attack   seconds
//     decay    seconds
//     sustain  0..1
//     release  seconds
//
// WHY THE TIME AXIS SNAPS. A proportional axis that always fills the window cannot be
// dragged: the release point lands at the right-hand edge whatever the release is, so its
// handle has nowhere to go. An absolute axis fixes that, and a window that recomputed
// itself continuously would slide under the cursor mid-drag. So the window is the smallest
// step of a snap ladder that fits the envelope, which is stable for the whole of a drag and
// jumps a step when you outgrow it. `windowSeconds()` reports it so the panel can print the
// scale rather than leaving the player to guess.

#include <algorithm>
#include <cmath>
#include <functional>
#include <string_view>

namespace dsp { namespace env {

/** One draggable point, in window units: x and y are 0..1 across and up the glass, and
    `size` multiplies the drawn radius. Deliberately the same shape as the filter
    component's `ScreenHandle` so a panel needs ONE hit-test and ONE painter for both. */
struct ScreenHandle
{
    const char* name = "";
    float x = 0.0f, y = 0.0f, size = 1.0f;
};

/** What the host's parameters can hold, so a drag is clamped to a reachable value rather
    than to something the parameter will silently refuse. Defaults are Solera's. */
struct Limits
{
    float aMin = 0.001f, aMax = 8.0f;
    float dMin = 0.002f, dMax = 10.0f;
    float rMin = 0.002f, rMax = 10.0f;
};

/** The engine's own constants. Change these only to match a DIFFERENT envelope; they are
    not taste. `overshoot` is the target the attack chases, `epsilon` the threshold the
    decay and release are considered finished at. */
struct Shape
{
    float overshoot = 1.02f;
    float epsilon   = 1.0e-4f;
};

class Screen
{
public:
    using Get = std::function<float (const char*)>;
    using Set = std::function<void (const char*, float)>;

    Screen (Get g, Set s, Limits lim = {}, Shape sh = {})
        : get (std::move (g)), set (std::move (s)), limits (lim), shape (sh) {}

    /** The fraction of the window given to the sustain plateau. The sustain has no
        duration of its own - it lasts as long as the key is held - so it is drawn as a
        readable constant rather than as a time. */
    static constexpr float kHoldFraction = 0.22f;

    /** AND A MARGIN AFTER THE RELEASE, so its handle is never on the border.
        Found by the gate rather than by eye: with the envelope filling the glass exactly,
        the release point lands at x = 1.0, where a click is on the frame and a drag to the
        right is clamped to where it already was. The handle could not be grabbed and could
        not be moved outward. Reserving a tenth of the window means the release handle
        always has glass on both sides of it. */
    static constexpr float kTailFraction = 0.10f;

    // ---------------------------------------------------------------- segment durations
    /** How long the attack ACTUALLY takes, in seconds. See the header: this is 3.93 time
        constants for the usual 1.02 overshoot, not one. */
    [[nodiscard]] float attackSeconds() const
    {
        const float a = std::max (0.0f, get ("attack"));
        const float o = std::max (1.0001f, shape.overshoot);
        return a * std::log (o / (o - 1.0f));
    }

    /** HOW MANY TIME CONSTANTS OF DECAY AND RELEASE ARE DRAWN, and this is a display
        decision with a reason rather than a constant picked to look right.

        The engine finishes a decay when it comes within `epsilon` of its target, and with
        epsilon at 1e-4 that is `ln((1-s)/eps)` time constants: eight or nine of them. A
        10-second decay therefore takes about 84 seconds to technically complete, and the
        last two thirds of it is a change no one can hear. Plotting that put the whole
        envelope off the right-hand edge of every sensible window, which the gate caught as
        a release handle pinned at x = 1.0 where it could be neither grabbed nor dragged.

        So the drawn extent is the audible part. 3.93 time constants reaches 98 per cent of
        the way, and it is not an arbitrary number: it is exactly what the engine's own
        attack takes, `ln(1.02/0.02)`, so all three segments are drawn on one rule. The
        CURVE inside each segment is still the true exponential - only where the segment is
        considered finished is a display choice. */
    [[nodiscard]] static float drawnTaus() { return std::log (1.02f / 0.02f); }

    /** From full toward the sustain level, in seconds. Zero when the sustain is already at
        the top, because then there is nothing to decay through. */
    [[nodiscard]] float decaySeconds() const
    {
        const float d = std::max (0.0f, get ("decay"));
        const float s = std::clamp (get ("sustain"), 0.0f, 1.0f);
        if (1.0f - s <= shape.epsilon) return 0.0f;
        return d * drawnTaus();
    }

    /** From the sustain level toward silence, in seconds. Zero from a sustain of zero. */
    [[nodiscard]] float releaseSeconds() const
    {
        const float r = std::max (0.0f, get ("release"));
        const float s = std::clamp (get ("sustain"), 0.0f, 1.0f);
        if (s <= shape.epsilon) return 0.0f;
        return r * drawnTaus();
    }

    /** The time axis, in seconds. The smallest rung of the ladder that fits. */
    [[nodiscard]] float windowSeconds() const
    {
        const float moving = attackSeconds() + decaySeconds() + releaseSeconds();
        const float need   = moving / (1.0f - kHoldFraction - kTailFraction);
        static constexpr float ladder[] = { 0.1f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f,
                                            8.0f, 16.0f, 32.0f, 64.0f, 128.0f };
        for (const float w : ladder) if (w >= need) return w;
        return ladder[(sizeof ladder / sizeof ladder[0]) - 1];
    }

    // ---------------------------------------------------------------- the curve
    /** The four x positions the shape turns at: end of attack, end of decay, end of the
        sustain plateau, end of release. All 0..1 across the glass. */
    struct Breaks { float a = 0.0f, d = 0.0f, s = 0.0f, end = 0.0f; };

    [[nodiscard]] Breaks breaks() const
    {
        const float w = windowSeconds();
        Breaks b;
        b.a   = attackSeconds() / w;
        b.d   = b.a + decaySeconds() / w;
        b.s   = b.d + kHoldFraction;
        b.end = std::min (1.0f, b.s + releaseSeconds() / w);
        return b;
    }

    /** The level at a point across the glass, 0..1 up. This IS the engine's curve. */
    [[nodiscard]] float levelAt (float x) const
    {
        const float w = windowSeconds();
        const float s = std::clamp (get ("sustain"), 0.0f, 1.0f);
        const auto  b = breaks();
        const float t = std::clamp (x, 0.0f, 1.0f) * w;

        if (x <= b.a)
        {
            const float a = std::max (1.0e-6f, get ("attack"));
            const float o = std::max (1.0001f, shape.overshoot);
            return std::min (1.0f, o * (1.0f - std::exp (-t / a)));
        }
        if (x <= b.d)
        {
            const float d = std::max (1.0e-6f, get ("decay"));
            return s + (1.0f - s) * std::exp (-(t - b.a * w) / d);
        }
        if (x <= b.s) return s;
        if (x <= b.end)
        {
            const float r = std::max (1.0e-6f, get ("release"));
            return s * std::exp (-(t - b.s * w) / r);
        }
        return 0.0f;
    }

    /** `max` points of the curve, evenly across the glass. Returns how many were written.
        Same contract as the filter component's `fillCurve`, so a panel can paint either
        from one code path. */
    int fillCurve (float* dest, int max) const
    {
        if (dest == nullptr || max <= 1) return 0;
        for (int i = 0; i < max; ++i)
            dest[i] = levelAt ((float) i / (float) (max - 1));
        return max;
    }

    /** FOUR HANDLES, ONE PER CONTROL.

        The author, 2026-09-27: "the filter env screen and the amp env screen only has 3 drag
        points. they should have a drag point for each control so 4."

        It was three, with the middle one carrying two parameters - sideways for the decay
        and up for the sustain - which is what most envelope displays do and which this
        file used to defend as "the difference between four handles and three". He is
        right and that defence was wrong for this panel. A point that moves in two axes
        and writes two parameters cannot be dragged along one of them without touching the
        other, so setting a decay always nudged the sustain and setting a sustain always
        nudged the decay. On a plate where every other control is one knob for one
        parameter, that is the odd one out.

        So the plateau gets a handle of its own and each point now moves in ONE direction:

            attack    where the rise ends          sideways
            decay     where the fall ends          sideways, at the sustain's height
            sustain   the middle of the plateau    up and down
            release   where the tail ends          sideways

        The decay handle still SITS at the sustain's height, because that is where the
        curve is - it just no longer writes it. */
    int fillHandles (ScreenHandle* out, int max) const
    {
        if (out == nullptr || max <= 0) return 0;
        const auto b = breaks();
        const float s = std::clamp (get ("sustain"), 0.0f, 1.0f);
        const ScreenHandle all[] = {
            { "attack",  b.a,   1.0f, 1.0f },
            { "decay",   b.d,   s,    1.0f },
            // MID-PLATEAU, so it is never on top of the decay point at a long decay or on
            // top of the release point at a short one. The plateau is a fixed fraction of
            // the window, so its middle is a fixed distance from both.
            { "sustain", b.d + kHoldFraction * 0.5f, s, 1.0f },
            { "release", b.end, 0.0f, 1.0f },
        };
        const int n = std::min (max, 4);
        for (int i = 0; i < n; ++i) out[i] = all[i];
        return n;
    }

    // ---------------------------------------------------------------- dragging
    /** `x` and `y` are 0..1 across and up the glass. Writes through Set in the
        component's units, clamped to what the host said it can hold.

        THE WINDOW IS READ ONCE, at the top, and every segment in the drag is measured
        against that one value. Reading it per segment would let the ladder step mid-
        calculation and move a handle the player is not touching. */
    void dragged (const char* handle, float x, float y) const
    {
        if (handle == nullptr) return;
        const float w  = windowSeconds();
        const float xs = std::clamp (x, 0.0f, 1.0f);
        const auto  b  = breaks();
        const float o  = std::max (1.0001f, shape.overshoot);
        const std::string_view h { handle };

        if (h == "attack")
        {
            const float dur = xs * w;
            set ("attack", std::clamp (dur / std::log (o / (o - 1.0f)),
                                       limits.aMin, limits.aMax));
            return;
        }
        if (h == "decay")
        {
            // SIDEWAYS ONLY. `y` is ignored on purpose: the sustain has a handle of its
            // own now, and a decay drag that also wrote the sustain is the fault that
            // change fixed.
            const float dur = std::max (0.0f, xs - b.a) * w;
            set ("decay", std::clamp (dur / drawnTaus(), limits.dMin, limits.dMax));
            return;
        }
        if (h == "sustain")
        {
            // UP AND DOWN ONLY, and `x` is ignored because the plateau has no duration of
            // its own - it lasts as long as the key is held.
            set ("sustain", std::clamp (y, 0.0f, 1.0f));
            return;
        }
        if (h == "release")
        {
            const float s = std::clamp (get ("sustain"), 0.0f, 1.0f);
            if (s <= shape.epsilon) return;        // nothing to release from
            const float dur = std::max (0.0f, xs - b.s) * w;
            set ("release", std::clamp (dur / drawnTaus(), limits.rMin, limits.rMax));
        }
    }

private:
    Get get;
    Set set;
    Limits limits;
    Shape  shape;
};

}} // namespace dsp::env
