#include "PianoSamples.h"
#include <cstdio>
#include <cstdlib>

namespace synthkit::piano
{

/** A WINDOW TOO QUIET TO BE A CANDIDATE for the attack, as a fraction of the loudest window.

    Five per cent, 26 dB down. Its only job is to stop the step OUT of a file's opening - some
    recordings begin with digital silence or a converter still settling - from scoring as the
    biggest rise in the file. The attack is never this far down. */
/** A WINDOW TOO QUIET TO BE A CANDIDATE for the attack, as a fraction of the loudest window.

    A quarter, 12 dB down. SWEPT, not chosen: with the walk-back below in place, every layer of
    all three sets passes both halves of the onset gate from about 0.23 to 0.35, and fails on
    either side - under it the Steinway's pp top notes keep room (3 late at 0.05, 2 at 0.15, 1 at
    0.20), over it the detector reaches too far into the file (1 late at 0.40, 2 at 0.42).
    A quarter sits inside that bracket, and it is preferred over the more central 0.30 for a
    reason that has nothing to do with onsets: a later onset shifts the body window that law 2
    measures for level matching, so 0.30 retires two more pp takes as outliers - 85 recordings
    against 87. Two real recordings beat two transposed neighbours. */
static constexpr float kTooQuiet = 0.25f;

/** AND HOW LOUD A FIRST WINDOW HAS TO BE for the file to count as starting inside the note.

    A quarter, 12 dB down. A recording that opens this loud has no room in front of it and is
    passed through untrimmed; see `beginsInNote` below. Measured flat from 0.15 to 0.25 - nothing
    moves across that span - and worse at 0.10, where two of the Steinway's pp notes go late. */
static constexpr float kAlreadyIn = 0.25f;

/** HOW QUIET THE FIRST MILLISECOND WE KEEP HAS TO BE, as a fraction of the recording's peak.

    A quarter, 12 dB down, and it is the same number the gate checks - deliberately, because the
    rule IS "do not open inside the transient" and the loader should enforce it rather than hope
    the detector happens to. See the walk-back at the end of the onset search. */
static constexpr float kFrontFrac = 0.25f;

/** Set `SYNTHKIT_PIANO_ONSET_TRACE=1` to print the floor, the loudest window and the chosen
    onset for every recording. This is how `kRoomMargin` was chosen and how the next set's
    numbers get checked before anyone argues about a constant. */
static const bool kOnsetTrace = std::getenv ("SYNTHKIT_PIANO_ONSET_TRACE") != nullptr;

/** And `SYNTHKIT_PIANO_ONSET_DUMP=<part of a filename>` prints the whole window RMS curve for
    the recordings whose name contains it, in dB under their loudest window. The room's SHAPE is
    what this detector is really up against, and a single number for the room is what got the
    law wrong twice. */
static const char* kOnsetDump = std::getenv ("SYNTHKIT_PIANO_ONSET_DUMP");

/** `Piano.ff.A0.flac` (or `.wav`) -> dynamic and MIDI note, or -1. The Iowa set writes a flat as `Bb`
    and never a sharp, and the octave number is scientific pitch (A0 is MIDI 21, C4 is 60). */
static int parseName (const juce::String& fileName, Samples::Dyn& dynOut)
{
    auto parts = juce::StringArray::fromTokens (fileName.upToLastOccurrenceOf (".", false, false),
                                                ".", "");
    if (parts.size() < 3) return -1;
    const auto dyn = parts[1].toLowerCase();
    if      (dyn == "pp") dynOut = Samples::pp;
    else if (dyn == "mf") dynOut = Samples::mf;
    else if (dyn == "ff") dynOut = Samples::ff;
    else return -1;

    const auto note = parts[2];
    static const char* letters = "CDEFGAB";
    static const int   semis[] = { 0, 2, 4, 5, 7, 9, 11 };
    const int li = juce::String (letters).indexOfChar (note[0]);
    if (li < 0) return -1;
    int semi = semis[li], i = 1;
    if (note.length() > 1 && (note[1] == 'b' || note[1] == '#'))
    { semi += (note[1] == 'b' ? -1 : 1); ++i; }
    const int octave = note.substring (i).getIntValue();
    // MIDI: C-1 is 0, so C4 = 60 means (octave + 1) * 12 + semi.
    const int midi = (octave + 1) * 12 + semi;
    return (midi >= 0 && midi <= 127) ? midi : -1;
}

bool Samples::loadFromFolder (const juce::File& dir)
{
    for (auto& v : notes) v.assign ((size_t) (kHighKey - kLowKey + 1), Note{});
    loadedCount = missingCount = 0;
    if (! dir.isDirectory()) return false;

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    // AND FLAC BY HAND. `registerBasicFormats` is WAV and AIFF, nothing else - so the line
    // below is not a nicety, it is the difference between 262 recordings and none. Without it
    // every FLAC failed to open, the set loaded zero notes and reported itself absent, while
    // the folder sat there full of files. One flag away from a silent piano.
   #if JUCE_USE_FLAC
    fm.registerFormat (new juce::FlacAudioFormat(), false);
   #endif
    const auto cap = (juce::int64) (kMaxSeconds * sr);

    // FLAC FIRST, WAV IF THAT IS WHAT IS THERE. 2026-10-07. The recordings are stored trimmed
    // to the eight seconds this loader keeps and encoded as FLAC: the Steinway's files were 35
    // seconds of which 27 were read and thrown away on every load, and the tail of a piano note
    // is near-silence, which FLAC crushes. Measured on three of our own files: 8568 KB to 1304.
    // The format is registered above; this is the only line that chooses between the two.
    auto found = dir.findChildFiles (juce::File::findFiles, false, "Piano.*.flac");
    if (found.isEmpty()) found = dir.findChildFiles (juce::File::findFiles, false, "Piano.*.wav");
    for (const auto& f : found)
    {
        Dyn d {};
        const int key = parseName (f.getFileName(), d);
        if (key < kLowKey || key > kHighKey) continue;

        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (f));
        if (rd == nullptr || rd->numChannels < 1) { ++missingCount; continue; }

        // ---- THE ROOM BEFORE THE NOTE -------------------------------------------
        //
        // The author, 2026-10-02: "First of it takes about 3 seconds holding a key till you hear sound."
        //
        // The recordings begin with the room, not with the piano. Measured across all 261: the
        // median note has 250 ms of silence before the strike and the bottom three at ff have
        // over FOUR SECONDS - `Piano.ff.Bb0` is 4.262, `ff.B0` 4.154, `ff.A0` 4.010. So a key
        // press played the studio for a moment and the low keys were unplayable.
        //
        // Worse, the eight-second cap was measured from the FILE's start, so `ff.A0` kept four
        // seconds of silence and only four seconds of note - half the memory spent on nothing.
        //
        // So: find the onset, keep a few milliseconds of run-up so the attack transient is not
        // clipped off the front, and measure the cap FROM THERE.
        //
        // ---- AND THE THRESHOLD MUST COME FROM THE ROOM, NOT FROM THE PEAK, 2026-10-07 -------
        //
        // The author: the search "gets fooled by the room noise on the grand's soft notes. Middle C
        // played softly started 425 ms late, and most soft notes were silent for their first
        // moments." Measured: 431.5 ms at pp middle C, and SEVENTY-EIGHT of the Steinway's 79
        // pp recordings starting more than 25 ms in, median 238 ms, worst 657 ms at key 56.
        //
        // The old law fired on the first sample past two per cent of the FILE'S OWN PEAK, and
        // its comment claimed that was "well clear of a noise floor that sits fifteen decibels
        // down on the quietest pp note". That arithmetic is backwards. Fifteen decibels down
        // from the peak IS seventeen per cent of the peak - nearly nine times ABOVE a two per
        // cent threshold - so for any recording whose room sits within 34 dB of its peak the
        // threshold is UNDER the room and the detector fires on the first noise sample. A pp
        // note peaks around a thirtieth of an ff note, so pp is where that happens and ff is
        // where it does not, which is why every earlier measurement of this looked fine: they
        // were all taken at ff.
        //
        // ---- AND NO LEVEL THRESHOLD CAN WORK AT ALL, WHICH THE DUMP SETTLED ----------
        //
        // The first repair measured the room instead of scaling off the peak - the quietest 5 ms
        // window times a margin - and it fixed middle C (431.5 ms to 16.5) and left twenty-six
        // pp notes late. `SYNTHKIT_PIANO_ONSET_DUMP=pp.Bb1` printed why, and it is worth reading
        // before anyone proposes another threshold. The window RMS curve of that recording, in
        // dB under its own loudest window:
        //
        //      0 ms   -30.5  rising smoothly
        //    200 ms   -14.5  and then FLAT, for four hundred milliseconds
        //    635 ms   -15.0  -8.6  0.0      <- the strike, 15 dB in two windows
        //
        // So the room in a pp recording sits FOURTEEN dB under the note, and the attack's own
        // first window is at -15. There is no level between them; the two overlap. The quietest
        // window, -30.5, is the file's opening before the chain settles and is 16 dB below the
        // room it is supposed to represent, which is why a margin of six landed exactly ON the
        // room and fired where the ramp crossed it. Every level law has this problem on a pp
        // note, and none of them has it on an ff note - which is the whole reason this stayed
        // hidden: the room is the same in both and only the note changes.
        //
        // THE ATTACK IS A STEP, SO MEASURE THE STEP. The strike rises 15 dB in two windows while
        // the ramp rises 0.4 dB per window, so a rise test has fifteen decibels of margin where
        // a level test has none. Two details make it reliable:
        //
        //   - ONLY BEFORE THE LOUDEST WINDOW. A piano's attack IS its peak, so nothing after it
        //     can be the onset - and the tail must be excluded, because a decaying piano beats
        //     by seven or eight dB from one window to the next, which rivals the strike itself.
        //   - the candidate window must not be below `kTooQuiet`, so the step out of a file's
        //     opening silence cannot win.
        const auto total = (juce::int64) rd->lengthInSamples;
        if (total <= 0) { ++missingCount; continue; }

        // one pass over the head, for the floor, the peak and the onset
        const auto scan = (int) juce::jmin (total, (juce::int64) (12.0 * sr));
        juce::AudioBuffer<float> head ((int) rd->numChannels, scan);
        if (! rd->read (&head, 0, scan, 0, true, rd->numChannels > 1))
        { ++missingCount; continue; }

        const int win  = juce::jmax (1, (int) (0.005 * sr));
        const int nWin = scan / win;
        const int chans = head.getNumChannels();
        float floorRms = 1.0f, loudRms = 0.0f;
        std::vector<float> wr ((size_t) juce::jmax (0, nWin), 0.0f);
        for (int w = 0; w < nWin; ++w)
        {
            double sq = 0.0;
            for (int ch = 0; ch < chans; ++ch)
            {
                const float* p2 = head.getReadPointer (ch) + (size_t) w * (size_t) win;
                for (int i = 0; i < win; ++i) sq += (double) p2[i] * (double) p2[i];
            }
            const float r = (float) std::sqrt (sq / ((double) win * chans));
            wr[(size_t) w] = r;
            floorRms = juce::jmin (floorRms, r);
            loudRms  = juce::jmax (loudRms,  r);
        }

        juce::int64 onset = 0;
        // A FILE THAT BEGINS INSIDE THE NOTE HAS NOTHING TO TRIM, and must not be searched.
        //
        // The third shape in these sets, after "room then a step" and "a fade straight into the
        // strike": a recording somebody already trimmed to its own strike, or past it.
        // `Piano.mf.F1.flac` opens at -8 dB of its loudest window and is note from its first
        // sample. With no room to find, the biggest rise in the file is just the note beating
        // against itself - an F1 at 43 Hz swings 6 dB from one 5 ms window to the next - so the
        // detector picked 15 ms out of a file with nothing to spare and took the front off the
        // attack. Five recordings across the three sets are like this, and they were invariant
        // under every setting of the law, which is what said the law was being asked the wrong
        // question rather than answering it badly.
        const bool beginsInNote = nWin > 0 && loudRms > 0.0f && wr[0] > loudRms * kAlreadyIn;
        if (nWin >= 2 && loudRms > 0.0f && ! beginsInNote)
        {
            int wLoud = 0;
            for (int w = 0; w < nWin; ++w) if (wr[(size_t) w] >= loudRms) { wLoud = w; break; }

            const float tiny = loudRms * 1.0e-4f;        // so a zero window cannot divide
            const float bar  = loudRms * kTooQuiet;
            float best = 0.0f; int bestW = 0;
            for (int w = 0; w + 1 < wLoud; ++w)
            {
                if (wr[(size_t) w + 1] < bar) continue;
                const float rise = wr[(size_t) w + 1] / juce::jmax (wr[(size_t) w], tiny);
                if (rise > best) { best = rise; bestW = w; }
            }
            onset = (juce::int64) bestW * win;
        }
        // A SHORT RUN-UP, because the detector fires once the strike is already under way and a
        // piano's hammer noise starts below it. Five milliseconds is inaudible as a delay and
        // keeps the front of the transient, which is most of what tells one dynamic from another.
        onset = juce::jmax ((juce::int64) 0, onset - (juce::int64) (0.005 * sr));

        // ---- AND NEVER TRIM INTO THE ATTACK ------------------------------------------
        //
        // The last shape, and the one no threshold reaches: a recording with a twenty
        // millisecond attack and no room at all, where every candidate the rise test can offer
        // is already inside the transient. Three of them across the three sets - the Upright's
        // pp Eb1 and Gb1, the Honky Tonk's mf B0 - were having ten to fifteen milliseconds
        // taken off a twenty millisecond attack, and they did not move under any setting of
        // either constant above.
        //
        // So the onset is walked BACK while the audio at it is still loud. This is not another
        // guess at where the strike is: it is the definition of not clipping an attack, applied
        // directly - if the first millisecond we would keep is within 12 dB of the note's peak,
        // we are standing inside the transient and have to step back. A file with real room in
        // front of it never moves here, because 12 dB under a piano note is far above its room.
        {
            const int ms1 = juce::jmax (1, (int) (0.001 * sr));
            float headPeak = 0.0f;
            for (int ch = 0; ch < chans; ++ch)
                headPeak = juce::jmax (headPeak, head.getMagnitude (ch, 0, scan));
            const float frontBar = headPeak * kFrontFrac;
            const auto peakAt = [&] (juce::int64 at)
            {
                const int n2 = (int) juce::jmin ((juce::int64) ms1, (juce::int64) scan - at);
                float p3 = 0.0f;
                for (int ch = 0; ch < chans && n2 > 0; ++ch)
                    p3 = juce::jmax (p3, head.getMagnitude (ch, (int) at, n2));
                return p3;
            };
            while (onset > 0 && peakAt (onset) > frontBar)
                onset = juce::jmax ((juce::int64) 0, onset - ms1);
        }

        if (kOnsetDump != nullptr && f.getFileName().contains (kOnsetDump))
        {
            // The window RMS curve, in dB under the loudest window, 10 windows (50 ms) a line.
            // This is how an onset argument gets settled: the room's SHAPE is the thing in
            // question and a single number for it is what got this wrong twice.
            std::fprintf (stderr, "dump %s  floor %.6f  loud %.6f\n",
                          f.getFileName().toRawUTF8(), floorRms, loudRms);
            for (int w = 0; w < nWin && w < 400; w += 10)
            {
                std::fprintf (stderr, "  %6.0f ms ", 1000.0 * w * win / sr);
                for (int k = w; k < w + 10 && k < nWin; ++k)
                {
                    const float r = wr[(size_t) k];
                    std::fprintf (stderr, " %5.1f", r > 0.0f && loudRms > 0.0f
                                                    ? 20.0 * std::log10 (r / loudRms) : -99.0);
                }
                std::fprintf (stderr, "\n");
            }
        }
        if (kOnsetTrace)
            std::fprintf (stderr, "onset  %-22s floor %.6f  loud %.6f  ratio %6.1f  at %7.1f ms\n",
                          f.getFileName().toRawUTF8(), floorRms, loudRms,
                          floorRms > 0.0f ? 20.0 * std::log10 (loudRms / floorRms) : 0.0,
                          1000.0 * (double) onset / sr);

        const auto want = (int) juce::jmin (total - onset, cap);
        if (want <= 0) { ++missingCount; continue; }

        juce::AudioBuffer<float> buf ((int) rd->numChannels, want);
        if (! rd->read (&buf, 0, want, onset, true, rd->numChannels > 1))
        { ++missingCount; continue; }

        // MONO BY SUM, NOT BY LEFT. The recordings are a stereo pair of one instrument, so
        // taking one channel throws away half the room and shifts the tone; summing and
        // halving keeps both. Solera's PCM half is mono (`const float*`), which is why this
        // is here rather than a decision the voice could make.
        auto& n = notes[(size_t) d][(size_t) (key - kLowKey)];
        n.trimMs = (float) (1000.0 * (double) onset / sr);
        n.audio.assign ((size_t) want, 0.0f);
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const float* src = buf.getReadPointer (ch);
            for (int i = 0; i < want; ++i) n.audio[(size_t) i] += src[i];
        }
        if (buf.getNumChannels() > 1)
            for (auto& x : n.audio) x *= 0.5f;
        n.rootKey = key;
        ++loadedCount;
    }

    // ---- ONE GAIN FOR THE WHOLE SET, WHICH IS A CALIBRATION AND NOT A DYNAMIC ----
    //
    // The recordings come off the disc at the level they were cut: the loudest ff peaks around
    // 0.19, which is 14 dB under a synth patch of ours, so the piano arrived quiet enough to
    // read as broken. Measured, not guessed - `--steinway` prints all three peaks.
    //
    // ONE gain across every note and every dynamic, from the loudest sample in the set. A
    // per-layer or per-note normalisation would flatten exactly what three dynamics are FOR:
    // pp peaks at 0.0075 against ff's 0.1878, which is 28 dB of real dynamic range, and that
    // spread survives this untouched because every sample is multiplied by the same number.
    //
    // Headroom of 0.89 rather than 1.0, because several notes sum when a chord is held and the
    // voice has its own level and envelope after this.
    // ORDER MATTERS, AND THIS WAS WRONG ONCE: the outlier scan runs BEFORE the matching.
    // Run after it, every take has already been pulled toward the median and clamped, so an
    // outlier no longer looks like one and the scan retires nothing. Measured: with the scan
    // second, widening its bar from 25 to 45 per cent retired one extra take and moved not a
    // single hole.
    // ---- A TAKE THAT IS 30 dB OUT OF LINE IS A BAD TAKE, 2026-10-07 -------------
    //
    // The matching above is clamped to 4x either way on purpose: lifting a note 30 dB raises
    // its room noise with it, and a note that needs that is not a level problem. Measured in
    // this set: at mf, E1 sits at 0.00047 against Gb1 at 0.01530 two semitones away - a 32-fold
    // spread between neighbours, which no piano does and no gain can honestly fix.
    //
    // So the outlier is RETIRED rather than amplified. `pick` already searches outwards for the
    // nearest valid recording and transposes it, which is the machinery the three genuinely
    // missing files use; marking a bad take invalid simply hands it the same path. A semitone
    // of transposition is a smaller lie than a hole in the keyboard.
    //
    // THE BAR IS 22 PER CENT of the mean of its two neighbours, 13 dB down, and it is set
    // against the RAW levels because the scan runs first.
    //
    // 45 per cent was tried and is wrong HERE: against raw takes it retired 25 recordings to
    // close one hole, because real neighbour-to-neighbour variation in an unmatched set is far
    // wider than in a matched one. 22 per cent catches the takes that no gain can honestly fix
    // and leaves the rest to the matching below, which is the mechanism for ordinary variation.
    {
        const int skip = (int) (0.020 * sr);
        const int window = (int) (0.300 * sr);
        const auto bodyOf = [skip, window] (const Note& n)
        {
            if (! n.valid()) return 0.0;
            const int a = std::min (skip, (int) n.audio.size());
            const int m = std::min (window, (int) n.audio.size() - a);
            if (m <= 0) return 0.0;
            double sq = 0.0;
            for (int i = a; i < a + m; ++i) sq += (double) n.audio[(size_t) i] * n.audio[(size_t) i];
            return std::sqrt (sq / m);
        };
        for (auto& row : notes)
        {
            if (row.size() < 3) continue;
            std::vector<double> lvl (row.size(), 0.0);
            for (size_t i = 0; i < row.size(); ++i) lvl[i] = bodyOf (row[i]);
            std::vector<size_t> drop;
            for (size_t i = 1; i + 1 < row.size(); ++i)
            {
                if (lvl[i] <= 1.0e-12) continue;
                const double nb = 0.5 * (lvl[i - 1] + lvl[i + 1]);
                if (nb > 1.0e-9 && lvl[i] < nb * 0.22) drop.push_back (i);
            }
            // Never retire two in a row: that would be a REGION of the keyboard being quiet,
            // which is the instrument rather than a take, and dropping both would leave a gap
            // wide enough to hear as a transposition.
            for (size_t k = 0; k < drop.size(); ++k)
                if (k == 0 || drop[k] != drop[k - 1] + 1)
                {
                    row[drop[k]].audio.clear();
                    row[drop[k]].rootKey = -1;
                    ++retiredCount;
                }
        }
    }


    // ---- EVERY NOTE MATCHED WITHIN ITS OWN DYNAMIC, 2026-10-07 ----------------
    //
    // The author: "you need to fix 1. and level match everything including length."
    //
    // One gain for the whole set was right about the thing it was protecting and wrong about
    // the thing it was not. It keeps the 28 dB between pp and ff, which is what three dynamics
    // are FOR. It also keeps the variation BETWEEN ADJACENT NOTES inside one dynamic, and that
    // is not music, it is eighty-eight separate recordings of a real piano on a real day.
    //
    // Measured before this: at pp, A4 sits at 0.00071 and B4 at 0.00487, a seven-fold spread
    // between neighbours, and through the engine keys 70, 72 and 74 came out at 6 to 11 per
    // cent of the keys either side - a 24 dB hole that reads as a note that did not play.
    //
    // So: each note is scaled to its OWN DYNAMIC'S MEDIAN. Inside a layer the keyboard is even;
    // between layers nothing moves, because every layer is normalised to its own median and the
    // medians keep the distance they were recorded with. The median rather than the mean,
    // because a handful of outliers is exactly what this is correcting and a mean would chase
    // them.
    {
        // A NOTE'S LEVEL IS ITS BODY, NOT ITS PEAK. A hammer transient is a few samples and
        // varies with how the note was struck; what the ear matches is the first third of a
        // second after the attack. Peak-matching would equalise the transients and leave the
        // bodies as uneven as they started.
        // AND THE WINDOW SKIPS THE HAMMER. Matching from sample zero matches the TRANSIENT,
        // which is the part that varies most with how a note was struck and the part the ear
        // weights least for level. Starting 20 ms in and running to 320 ms measures the body -
        // and it is the same span the key sweep hears, so what is matched is what is measured.
        const int skip = (int) (0.020 * sr);
        const int window = (int) (0.300 * sr);
        const auto bodyOf = [skip, window] (const Note& n)
        {
            if (! n.valid()) return 0.0;
            const int a = std::min (skip, (int) n.audio.size());
            const int m = std::min (window, (int) n.audio.size() - a);
            if (m <= 0) return 0.0;
            double sq = 0.0;
            for (int i = a; i < a + m; ++i) sq += (double) n.audio[(size_t) i] * n.audio[(size_t) i];
            return std::sqrt (sq / m);
        };
        for (auto& row : notes)
        {
            std::vector<double> levels;
            for (auto& n : row) { const double b = bodyOf (n); if (b > 1.0e-9) levels.push_back (b); }
            if (levels.size() < 4) continue;              // too few to have a median worth using
            std::sort (levels.begin(), levels.end());
            const double median = levels[levels.size() / 2];
            for (auto& n : row)
            {
                const double b = bodyOf (n);
                if (b <= 1.0e-9) continue;
                // CLAMPED, because a note that is genuinely 10 dB down is a recording to
                // correct and one that is 40 dB down is a different problem - lifting it that
                // far would raise its noise floor with it. 4x either way covers every note in
                // this set and refuses to invent signal. 6x up rather than 4x once the outliers above are
                // retired: what remains needs a lift a clean take can carry.
                const float g = (float) std::clamp (median / b, 0.25, 6.0);
                for (auto& x : n.audio) x *= g;
            }
        }
    }

    // ---- AND THE SAME LENGTH, with the tail faded rather than cut ----------------
    //
    // The recordings run until the note has died, so a bass note is seconds longer than a
    // treble one and the set is 2 GB of mostly decay. A common cap makes every note end the
    // same way; the fade is what stops the cap being a click, which is the fault a hard trim
    // always ships with.
    {
        const int trim = (int) (kMaxSeconds * sr);
        const int fade = (int) (0.040 * sr);             // 40 ms, under one cycle of nothing
        for (auto& row : notes)
            for (auto& n : row)
            {
                if (! n.valid() || (int) n.audio.size() <= trim) continue;
                n.audio.resize ((size_t) trim);
                for (int i = 0; i < fade && i < trim; ++i)
                {
                    const float g = (float) i / (float) fade;
                    n.audio[(size_t) (trim - 1 - i)] *= g;
                }
            }
    }

    // ---- ONE HEADROOM GAIN LAST, over a set that is now even -------------------
    double loudest = 0.0;
    for (auto& row : notes)
        for (auto& n : row)
            for (float x : n.audio) loudest = std::max (loudest, (double) std::abs (x));
    if (loudest > 1.0e-6)
    {
        setGain = (float) (0.89 / loudest);
        for (auto& row : notes)
            for (auto& n : row)
                for (auto& x : n.audio) x *= setGain;
    }
    // ---- WHERE THE STRIKE ACTUALLY IS, MEASURED ON THE FINISHED AUDIO ----------
    //
    // Added 2026-10-07, because the onset law above had been wrong at pp since it was written
    // and nothing could see it. This is a fact about each recording as it will PLAY, and it is
    // deliberately independent of the law that produced it: the time from the first sample of
    // the kept audio to the point the note first reaches HALF ITS OWN PEAK.
    //
    // Half peak is the strike and nothing else is. A piano reaches it within a few milliseconds
    // of the hammer; room noise, at three per cent of a pp note's peak, never reaches it at all.
    // So a large number here means the kept audio begins with room - which is exactly what a
    // player hears as a late note - and it means that whatever the threshold arithmetic claims.
    for (int d = 0; d < kDyns; ++d)
        for (auto& n : notes[(size_t) d])
        {
            n.strikeMs = 0.0f; n.frontDb = -99.0f;   // trimMs is set by the read, above
            if (! n.valid()) continue;
            float pk = 0.0f;
            for (auto x : n.audio) pk = juce::jmax (pk, std::abs (x));
            if (pk <= 0.0f) continue;
            const float half = pk * 0.5f;
            for (size_t i = 0; i < n.audio.size(); ++i)
                if (std::abs (n.audio[i]) >= half)
                { n.strikeMs = (float) (1000.0 * (double) i / sr); break; }
            // AND THE FRONT: the PEAK of the first millisecond, in dB under the note's peak.
            //
            // One millisecond and a peak, both deliberately. Five milliseconds of RMS cannot
            // tell a kept run-up from a clipped attack, because a hard strike reaches full
            // level in a millisecond or two - so a 5 ms window that correctly STARTS before
            // the strike is still three quarters full of note and reads as loud as a clipped
            // one. Measured: five recordings across the three sets sat at -7 to -12 dB on that
            // measure at EVERY setting of the onset law, which is what showed the measure
            // rather than the law was wrong. And a peak rather than an RMS because 1 ms of a
            // 50 Hz bass note is a fortieth of a cycle, where RMS depends on which part of the
            // cycle it lands in and says nothing about level.
            const size_t fw = (size_t) juce::jmin ((double) n.audio.size(), 0.001 * sr);
            float fp = 0.0f;
            for (size_t i = 0; i < fw; ++i) fp = juce::jmax (fp, std::abs (n.audio[i]));
            n.frontDb = fp > 0.0f ? (float) (20.0 * std::log10 (fp / pk)) : -99.0f;
        }
    return loadedCount > 0;
}

Sample Samples::pick (int key, Dyn d) const noexcept
{
    Sample out {};
    key = juce::jlimit (kLowKey, kHighKey, key);

    // THE NEAREST RECORDED NOTE IN THIS DYNAMIC, searching outwards. The set is chromatic so
    // this is nearly always the note itself and the interpolator runs at unity; the search
    // exists for the three genuine holes - A0 at pp and mf, Bb0 at mf - where the next note
    // up is transposed down rather than the key going silent.
    //
    // IT DOES NOT FALL BACK TO ANOTHER DYNAMIC WHEN THE SET HAS ONE, 2026-10-07.
    //
    // A missing pp note answered with its ff recording would be a piano that gets louder when
    // you play softer, which is worse than a semitone of transposition. That still holds for a
    // set with three recorded layers.
    //
    // But a set with ONE layer is a different fact: the honky-tonk was recorded once per note
    // and the upright twice, and writing one recording into three rows held it three times on
    // disk and three times in RAM for no information at all. So an EMPTY row - not a missing
    // note, an entirely empty dynamic - uses the nearest populated one, preferring the softer
    // side so a quiet touch never reaches for a louder take.
    int use = (int) d;
    if (! hasLayer ((Dyn) use))
    {
        use = -1;
        for (int step = 1; step < kDyns && use < 0; ++step)
            for (int dir : { -1, 1 })                       // softer first
            {
                const int k = (int) d + step * dir;
                if (k >= 0 && k < kDyns && hasLayer ((Dyn) k)) { use = k; break; }
            }
        if (use < 0) return out;
    }
    const auto& row = notes[(size_t) use];
    if (row.empty()) return out;
    const int span = kHighKey - kLowKey;
    const Note* found = nullptr;
    for (int step = 0; step <= span && found == nullptr; ++step)
    {
        // UP FIRST, THEN DOWN, and at step 0 only once. The first version had the
        // `if (step == 0) break` after a `return`, which is unreachable, so key 0 was simply
        // examined twice - harmless and wrong, and exactly the kind of thing that survives
        // because it cannot be heard.
        for (int dir : (step == 0 ? std::initializer_list<int> { 1 }
                                  : std::initializer_list<int> { 1, -1 }))
        {
            const int k = key + step * dir;
            if (k < kLowKey || k > kHighKey) continue;
            const auto& n = row[(size_t) (k - kLowKey)];
            if (n.valid()) { found = &n; break; }
        }
    }
    if (found == nullptr) return out;

    out.data        = found->audio.data();
    out.length      = (uint32_t) found->audio.size();
    out.start       = 0;
    out.loopStart   = 0;
    out.end         = (uint32_t) found->audio.size();
    out.looped      = false;          // a piano note ends; it does not loop
    out.rate        = sr;
    out.rootKey     = found->rootKey;
    out.fine        = 0;
    out.noTranspose = false;
    return out;
}

} // namespace synthkit::piano
