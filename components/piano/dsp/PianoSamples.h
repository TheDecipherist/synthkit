#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

/*  THE SAMPLED PIANO AS A BUILDING BLOCK.

    One multisampled acoustic piano, loadable by any synth on the rack, with the handling that a
    folder of real recordings turns out to need. The DSP here is a sample PLAYER's front half -
    which recording a key and a velocity get - plus the loading laws, which are the part that was
    expensive to learn and the part every machine would otherwise learn again.

    WHY IT IS A COMPONENT. Solera grew it for its PIANO oscillator type and every law in it is
    about recordings rather than about Solera: that a set is not level-matched, that a take can
    be 30 dB out of line, that a recording is four times longer than anything will play of it,
    that a missing note is better transposed than silent. The next machine with a piano in it
    needs all four and none of them are its own.

    WHAT THE HOST SUPPLIES. A folder. The naming is `Piano.<dyn>.<note>.<ext>` with `dyn` one of
    pp, mf, ff and `note` spelled with flats (`Bb4`), which is the University of Iowa set's own
    spelling and the one the free FreePats sets were converted to. FLAC is preferred and WAV is
    read if that is what is there.

    WHAT IT IS NOT. Not a piano MODEL - `dspblocks/include/dsp/PianoString.h` is the physical
    string and is a different instrument with different laws. Not an engine: it returns a sample
    and a root key, and the host's own voice reads, pitches, filters and envelopes it.
*/
namespace synthkit::piano
{

/** One recording, ready to read. Deliberately a POD with no owner: the set owns the audio and
    this points into it, so a voice can hold one per note with nothing to free. */
struct Sample
{
    const float* data = nullptr;
    std::uint32_t length = 0;
    std::uint32_t start = 0, loopStart = 0, end = 0;
    bool looped = false;
    double rate = 48000.0;          // words per second at unity increment
    int  rootKey = 60;              // the note this recording plays at unity
    int  fine = 0;                  // its cents offset
    bool noTranspose = false;

    bool valid() const noexcept { return data != nullptr && end > start; }
};

/** A multisampled piano: three dynamic layers over 88 keys, and the laws for loading one.

    THE FIVE LOADING LAWS, each of them measured rather than assumed, and each one the reason a
    set of real recordings does not simply work:

    1. THE RECORDINGS BEGIN WITH THE ROOM, AND THE ATTACK IS A STEP RATHER THAN A LEVEL.
       Measured across the Iowa Steinway: the median note has 250 ms of silence before the strike
       and the bottom three at ff have over four seconds. No LEVEL threshold can find it, which
       cost two attempts to establish: a pp recording's room sits 14 dB under the note and the
       attack's own first window sits at 15 - the two overlap, so any threshold between them is
       imaginary. The strike is instead the biggest RISE before the loudest window, which has 15
       dB of margin where a level has none; then the onset is walked back out of the transient,
       and 5 ms of run-up is kept so its front survives. Three file shapes all have to work:
       room then a step, a fade straight into the strike, and a file already trimmed past it.

    2. A SET IS NOT LEVEL-MATCHED. Adjacent notes at pp varied seven-fold in that same set, which
       is eighty-eight separate recordings of one instrument on one day rather than music. Each
       note is scaled to its OWN DYNAMIC'S MEDIAN, measured on the body (20 to 320 ms, skipping
       the hammer) rather than the peak. The medians are left where they are, so the real
       difference between pp and ff - 28 dB in that set - survives untouched.

    3. A TAKE 13 dB BELOW BOTH ITS NEIGHBOURS IS A BAD TAKE. No honest gain fixes one: lifting it
       raises its room noise with it. It is retired, and `sampleFor`'s outward search hands the
       key a neighbour transposed a semitone, which on a chromatic set is inaudible.

    4. A MISSING NOTE IS TRANSPOSED, NEVER BORROWED FROM ANOTHER DYNAMIC. A pp note answered with
       its ff recording is a piano that gets louder when you play softer, which is worse than a
       semitone. The one exception is a dynamic that is ENTIRELY empty - a set recorded with one
       velocity layer rather than three - where there is nothing to confuse and holding the same
       audio three times is waste.

    5. A NOTE IS LONGER THAN ANYTHING WILL PLAY OF IT. Capped, with the tail faded rather than
       cut, because a hard trim ships a click.
*/
class Samples
{
public:
    enum Dyn : int { pp = 0, mf = 1, ff = 2, kDyns = 3 };
    static constexpr int kLowKey  = 21;      // A0
    static constexpr int kHighKey = 108;     // C8
    /** The common length. Eight seconds holds every note's useful decay in the sets measured so
        far; the loader fades the last 40 ms so the cap is not a click. */
    static constexpr double kMaxSeconds = 8.0;

    bool loadFromFolder (const juce::File& dir);

    /** THE ONE THAT MATTERS: a key and a velocity choose a recording. */
    Sample sampleFor (int key, float velocity) const noexcept
    { return pick (key, layerFor (velocity)); }

    bool   isReal()  const noexcept { return loadedCount > 0; }
    int    loaded()  const noexcept { return loadedCount; }
    int    missing() const noexcept { return missingCount; }
    /** How many takes were retired as outliers. A set that quietly drops recordings is worse
        than one that says how many. */
    int    retired() const noexcept { return retiredCount; }
    float  gain()    const noexcept { return setGain; }
    /** Which recorded note a key actually plays, for a gate to check. */
    int    rootFor (int key, float velocity) const noexcept
    { const auto s = pick (key, layerFor (velocity)); return s.valid() ? s.rootKey : -1; }

    /** HOW LATE THIS RECORDING STARTS, in ms, or -1 if there is no recording for it. The time
        from the first sample a voice will read to the strike; see `Note::strikeMs`. A gate asks
        this instead of rendering audio, because the number is a property of the set. */
    float  strikeMsOf (Dyn d, int key) const noexcept
    {
        if ((unsigned) d >= (unsigned) kDyns) return -1.0f;
        if (key < kLowKey || key > kHighKey) return -1.0f;
        const auto& n = notes[(size_t) d][(size_t) (key - kLowKey)];
        return n.valid() ? n.strikeMs : -1.0f;
    }

    /** How loud a recording's first 5 ms is, in dB under its own peak, or 1 for no recording.
        See `Note::frontDb`: a number near zero is a clipped attack. */
    float  frontDbOf (Dyn d, int key) const noexcept
    {
        if ((unsigned) d >= (unsigned) kDyns) return 1.0f;
        if (key < kLowKey || key > kHighKey) return 1.0f;
        const auto& n = notes[(size_t) d][(size_t) (key - kLowKey)];
        return n.valid() ? n.frontDb : 1.0f;
    }

    /** How much the loader cut off a recording's front, in ms, or -1 for no recording. */
    float  trimMsOf (Dyn d, int key) const noexcept
    {
        if ((unsigned) d >= (unsigned) kDyns) return -1.0f;
        if (key < kLowKey || key > kHighKey) return -1.0f;
        const auto& n = notes[(size_t) d][(size_t) (key - kLowKey)];
        return n.valid() ? n.trimMs : -1.0f;
    }

    /** The latest-starting recording in a dynamic: its key, and how late. Key is -1 when the
        dynamic holds nothing. */
    std::pair<int, float> worstStrike (Dyn d) const noexcept
    {
        int worstKey = -1; float worst = -1.0f;
        for (int k = kLowKey; k <= kHighKey; ++k)
        {
            const float ms = strikeMsOf (d, k);
            if (ms > worst) { worst = ms; worstKey = k; }
        }
        return { worstKey, worst };
    }

    /** Does this dynamic hold any recording? A one-layer set leaves two rows empty. */
    bool hasLayer (Dyn d) const noexcept
    {
        if ((unsigned) d >= (unsigned) kDyns) return false;
        for (const auto& n : notes[(size_t) d]) if (n.valid()) return true;
        return false;
    }
    int layers() const noexcept
    { int n = 0; for (int d = 0; d < kDyns; ++d) if (hasLayer ((Dyn) d)) ++n; return n; }

    /** An even split of three layers. Not fitted - the place to change it is here, once there is
        a crossfade to fit against. */
    static Dyn layerFor (float velocity) noexcept
    {
        if (velocity < 1.0f / 3.0f) return pp;
        if (velocity < 2.0f / 3.0f) return mf;
        return ff;
    }

private:
    struct Note
    {
        std::vector<float> audio;     // mono, at the set's own rate
        int  rootKey = -1;
        /** How far into the kept audio the strike is, in ms: the time to half of this note's own
            peak. A fact about the recording as it will play, measured without reference to the
            onset law that trimmed it - see the note at the end of `loadFromFolder`. */
        float strikeMs = 0.0f;
        /** THE OTHER SIDE OF THE SAME BRACKET: the peak of the first MILLISECOND of the kept
            audio, in dB under this note's peak. An onset that lands too LATE clips the front off the attack, and
            the symptom is the opposite of a late note - the sample opens already loud and the
            piano sounds clicky. A healthy note opens on its run-up, 20 dB or more down; one
            that opens near its peak had its transient cut. Nothing measured this, which is how
            a fix for lateness could have shipped a clipped attack instead. */
        float frontDb = -99.0f;
        /** How much the loader cut off the FRONT of this file, in ms. The clipped-attack test
            above is only meaningful where this is non-zero: a recording already trimmed to its
            strike has no run-up to keep, so its first 5 ms is loud because that is the file,
            not because the detector overshot. 23 of the Steinway's mf notes are like that and
            they offend at every setting of the law, which is what proved it. */
        float trimMs = 0.0f;
        bool valid() const noexcept { return rootKey >= 0 && ! audio.empty(); }
    };
    std::array<std::vector<Note>, kDyns> notes;
    int loadedCount = 0, missingCount = 0, retiredCount = 0;
    double sr = 48000.0;
    float setGain = 1.0f;

    Sample pick (int key, Dyn d) const noexcept;
};

} // namespace synthkit::piano
