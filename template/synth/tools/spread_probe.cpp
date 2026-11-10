// Do the factory programs actually sound different from each other?
//
// "Almost all the patches sound the same" has meant three different things on
// three machines and only a measurement tells them apart:
//
//   * the PSR-730, where 707 voices really did collapse to 38 effective sounds
//     because the voice model read the category string and nothing else;
//   * the H3500, where 625 presets sat at 146 effective sounds because 272 of
//     them loaded their algorithm's DEFAULTS instead of their stored values, and
//     an invented global mix played every one of them half wet;
//   * the TC 2290, where the engine was fine and the AUDITION SOURCE was wrong -
//     a click gave a median preset-to-preset difference of 0.0001 and a strings
//     loop gave 0.0104, a hundred times more.
//
// So this renders every program FIVE ways and measures with dsp/Measure.h, the
// shared metric, so 0.164 here means what 0.164 means on every other unit.
//
// An instrument is not an effect. shell::AuditionSource generates AUDIO to feed
// a box; a synth makes its own, so what carries across is the four musical
// SHAPES as note patterns, named from the same enum so the vocabulary is shared.
// Two calibrations were paid for on the PSR-730 and both are built in below: a
// figure must FILL the render (a note that stops early leaves a nine-second
// window mostly silent, and comparing silence to silence says nothing), and it
// must not REPEAT tightly (dsp/Measure.h frames are log-spaced in TIME, so a
// repeating line makes every frame an average of the same notes - a 14-note
// figure dropped one machine's LEAD median from 0.24 to 0.026).
//
// What it prints:
//   1. COVERAGE  - every field of every stored program against the engine's copy
//   2. SETTINGS  - how many DISTINCT coefficient sets the bank resolves to
//   3. CUTOFF    - where each program's filter settles (playbook 17b)
//   4. SPREAD    - median / min / mean / max pairwise difference per excitation
//   5. TWINS     - programs that are, for listening purposes, another program
//
//   ExampleSynthSpreadProbe             measure
//   ExampleSynthSpreadProbe --selftest  collapse the bank on purpose and require
//                                       this probe to notice
#include <shell/AuditionSource.h>
#include <dsp/Measure.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "engine/Coeffs.h"
#include "engine/Engine.h"
#include "patch/Program.h"

using namespace examplesynth;

namespace {

constexpr double kSr = 48000.0;
constexpr int    kBlock = 512;

// FLOORS, NOT TARGETS. They sit well under what the bank measures today, so they
// catch a REGRESSION - a law that flattens, a loader that stops loading, an
// engine that stops responding - rather than the normal wobble of a metric.
// TODO(new machine): set these from the first honest measurement of the real
// bank, and RAISE them when the bank improves. A floor that never moves is a
// floor nobody is using.
constexpr double kFloorNote    = 0.10;
constexpr double kFloorMusical = 0.05;

struct Excitation { const char* name; shell::AuditionSource::Mode mode; };
struct Note { double at, len; int key, vel; };

std::vector<Note> pattern (shell::AuditionSource::Mode m)
{
    switch (m)
    {
        case shell::AuditionSource::off:      // ONE STRUCK NOTE: attack, body, release
            return { { 0.0, 7.0, 60, 100 } };
        case shell::AuditionSource::drums:    // transients and gaps
        {
            std::vector<Note> v;
            for (int bar = 0; bar < 4; ++bar)
            {
                const double b = 4.0 * bar;
                v.push_back ({ b + 0.0, 0.30, 36, 120 });
                v.push_back ({ b + 1.0, 0.30, 48, 110 });
                v.push_back ({ b + 1.75, 0.20, 55,  85 });
                v.push_back ({ b + 2.0, 0.30, 36, 120 });
                v.push_back ({ b + 3.0, 0.30, 48, 110 });
                v.push_back ({ b + 3.5, 0.25, 60,  95 });
            }
            return v;
        }
        case shell::AuditionSource::lead:     // a SPARSE line: note changes, not a loop
            return { { 0.0, 1.5, 64, 105 }, { 1.5, 1.0, 67, 105 }, { 2.5, 2.0, 71, 110 },
                     { 4.5, 1.0, 69, 100 }, { 5.5, 2.5, 62, 108 } };
        case shell::AuditionSource::pad:      // ONE HELD CHORD, filling the window
            return { { 0.0, 8.0, 48, 90 }, { 0.0, 8.0, 55, 88 }, { 0.0, 8.0, 60, 86 },
                     { 0.0, 8.0, 64, 84 } };
        default:                              // strings: a slow swell of moving voices
            return { { 0.0, 4.0, 52, 80 }, { 0.5, 4.0, 59, 85 }, { 1.0, 4.0, 64, 90 },
                     { 4.5, 3.5, 55, 95 }, { 4.5, 3.5, 62, 95 }, { 4.5, 3.5, 67, 95 } };
    }
}

/** Everything about a program that CAN reach the engine, so "did it arrive?" is
    a comparison rather than an opinion.

    NOT a memcmp on the Coeffs struct. The moment it holds anything but one type
    the compiler pads it, and padding bytes are INDETERMINATE: two Coeffs built
    from the same program by the same function compare unequal roughly at random.
    That reads as "every program's stored fields never reach the engine" on a
    build where every one of them does, which is a false alarm from the check
    that exists to catch the H3500's 272 real ones. */
struct Setting
{
    std::vector<double> f;
    explicit Setting (const Coeffs& c) : f (c.fields()) {}
    bool operator< (const Setting& o) const { return f < o.f; }
};

std::vector<float> render (const Coeffs& coeffs, const Excitation& x, double seconds)
{
    Engine e;
    e.prepare (kSr, kBlock);
    e.setCoeffs (coeffs);

    const auto notes = pattern (x.mode);
    const double bpm = 120.0;
    const int total = (int) (seconds * kSr);
    std::vector<float> mono;
    mono.reserve ((size_t) total);
    std::vector<float> l ((size_t) kBlock), r ((size_t) kBlock);

    size_t nextOn = 0;
    std::vector<std::pair<double, int>> offs;      // (beat, key)
    for (int done = 0; done < total; done += kBlock)
    {
        const double beat = (double) done / kSr * bpm / 60.0;
        while (nextOn < notes.size() && notes[nextOn].at <= beat)
        {
            e.noteOn (notes[nextOn].key, (float) notes[nextOn].vel / 127.0f);
            offs.emplace_back (notes[nextOn].at + notes[nextOn].len, notes[nextOn].key);
            ++nextOn;
        }
        for (auto it = offs.begin(); it != offs.end();)
            if (it->first <= beat) { e.noteOff (it->second); it = offs.erase (it); } else ++it;

        std::fill (l.begin(), l.end(), 0.0f);
        std::fill (r.begin(), r.end(), 0.0f);
        e.process (l.data(), r.data(), kBlock);
        for (int i = 0; i < kBlock; ++i) mono.push_back (0.5f * (l[(size_t) i] + r[(size_t) i]));
    }
    return mono;
}

int run (bool collapseTheBank)
{
    const auto bank = factoryBank();
    const int n = (int) bank.size();
    const double seconds = 10.0;                   // >= dsp::measure::kMinSeconds

    std::printf ("ExampleSynth program spread - %d factory programs, %.0f s each,\n"
                 "metric dsp::measure (0 = identical, 1 = nothing in common)\n\n",
                 n, seconds);

    // THE INJECTION for --selftest: every program resolving to the same
    // coefficients. That is what a voice model reading only the category field
    // does, and what a trust flag substituting defaults does. The bank still has
    // sixteen names.
    const auto load = [collapseTheBank] (const Program& p)
    { return collapseTheBank ? makeCoeffs (Program{}) : makeCoeffs (p); };

    int bad = 0;

    // ---- 1. COVERAGE -------------------------------------------------------
    // Assert the OPPOSITE of what you believe. 272 of the H3500's 625 presets
    // failed this and nothing else in its suite noticed.
    {
        int mismatched = 0;
        for (const auto& p : bank)
        {
            const auto got = load (p);
            const auto want = makeCoeffs (p);
            if (got.fields() != want.fields()) ++mismatched;
        }
        std::printf ("1. COVERAGE - every field of every stored program against the engine's copy\n");
        std::printf ("   programs                            %d\n", n);
        std::printf ("   programs whose stored fields DIFFER %d\n\n", mismatched);
        if (mismatched > 0)
        { std::printf ("FAIL  %d programs' stored fields never reach the engine\n\n", mismatched);
          ++bad; }
    }

    // ---- 2. DISTINCT SETTINGS ---------------------------------------------
    // The ceiling on the spread is set by the DATA. A bank that resolves to
    // thirteen settings cannot be fixed in the DSP, and the two need different
    // fixes, so the diagnosis has to come before the number.
    int distinct = 0;
    {
        std::map<Setting, std::vector<int>> settings;
        for (int i = 0; i < n; ++i) settings[Setting (load (bank[(size_t) i]))].push_back (i);
        int shared = 0;
        for (auto& kv : settings) if (kv.second.size() > 1) shared += (int) kv.second.size();
        distinct = (int) settings.size();
        std::printf ("2. DISTINCT ENGINE SETTINGS\n");
        std::printf ("   programs                            %d\n", n);
        std::printf ("   distinct coefficient sets           %d\n", distinct);
        std::printf ("   programs sharing one with another   %d (%.1f %%)\n",
                     shared, 100.0 * (double) shared / (double) std::max (1, n));
        std::printf ("   -> no rendering can separate these; the ceiling on the spread is set\n"
                     "      by the DATA reaching the engine, not by the DSP.\n\n");
        if (distinct < n)
        { std::printf ("FAIL  %d programs resolve to another program's settings\n\n", n - distinct);
          ++bad; }
    }

    // ---- 3. WHERE EACH PROGRAM'S CUTOFF SETTLES ---------------------------
    // Playbook 17b, and it needs no disassembly. FACTORY BANKS DO NOT SHIP
    // SILENT PROGRAMS. Render the whole bank, look at where each voice's cutoff
    // settles, and if a meaningful share sit implausibly dark - or bunch against
    // a limit - a term is missing from the sum. Under the M1's old three-term
    // sum, 47 of 126 active oscillator halves rested below 400 Hz at C4 and
    // three sat on the filter's floor.
    {
        int dark = 0, onTheFloor = 0;
        std::printf ("3. WHERE THE CUTOFF SETTLES at C4 (the sum, not the parameter)\n");
        for (const auto& p : bank)
        {
            Engine e;
            e.prepare (kSr, kBlock);
            e.setCoeffs (load (p));
            // TODO(new machine): "BELOW 400 Hz" ONLY MEANS DARK IN A LOW-PASS.
            // On a MULTIMODE filter a low cutoff in HIGH PASS is the filter
            // wide open, and a 6 dB/oct band-pass passes most of the spectrum
            // wherever its centre sits. The Synthex reported four correct
            // programs as a missing cutoff term until this counted per mode.
            // If your machine has filter modes, gate this on the mode.
            const float hz = e.restingCutoffHz (60);
            if (hz < 400.0f) ++dark;
            if (hz <= CutoffTerms::kAnchorHz * 1.05f) ++onTheFloor;
        }
        std::printf ("   programs resting below 400 Hz       %d of %d\n", dark, n);
        std::printf ("   programs sitting on the floor       %d\n", onTheFloor);
        std::printf ("   (a meaningful share sitting dark, or bunched against a limit, means a\n"
                     "    term is missing from CutoffTerms - and the terms you miss are exactly\n"
                     "    the ones with no patch byte behind them.)\n\n");
        if (onTheFloor > 0 || dark * 4 > n)
        { std::printf ("FAIL  the bank's cutoffs bunch dark - read the coefficient builder again\n\n");
          ++bad; }
    }

    // ---- 4/5. RENDER AND MEASURE ------------------------------------------
    const Excitation ex[] = {
        { "NOTE",    shell::AuditionSource::off },
        { "DRUMS",   shell::AuditionSource::drums },
        { "LEAD",    shell::AuditionSource::lead },
        { "PAD",     shell::AuditionSource::pad },
        { "STRINGS", shell::AuditionSource::strings },
    };
    constexpr int kNumEx = (int) (sizeof (ex) / sizeof (ex[0]));

    std::vector<std::vector<dsp::measure::Features>> feats ((size_t) kNumEx);
    for (auto& f : feats) f.resize ((size_t) n);
    for (int x = 0; x < kNumEx; ++x)
        for (int i = 0; i < n; ++i)
            feats[(size_t) x][(size_t) i] =
                dsp::measure::analyse (render (load (bank[(size_t) i]), ex[x], seconds), kSr);

    std::printf ("4. SPREAD ACROSS THE BANK\n");
    std::printf ("   %-9s %8s %8s %8s %8s %12s %8s\n",
                 "source", "median", "min", "mean", "max", "identical", "silent");
    for (int x = 0; x < kNumEx; ++x)
    {
        const auto r = dsp::measure::compare (feats[(size_t) x]);
        std::printf ("   %-9s %8.4f %8.4f %8.4f %8.4f %6ld/%-5ld %8d\n",
                     ex[x].name, r.median, r.worst, r.mean, r.best, r.identical, r.pairs, r.silent);
    }
    std::printf ("   (`identical` counts pairs under %.2f, the same-sound threshold in\n"
                 "    dsp/Measure.h. Two thirds of that metric is inert on an INSTRUMENT: every\n"
                 "    voice playing the same note has the same pitch period, so the comb term is\n"
                 "    zero for every pair, and none of them have discrete echoes. An instrument's\n"
                 "    numbers are compressed against an effects unit's on the same scale - say so\n"
                 "    when quoting them, and do not \"fix\" the shared metric to flatter one.)\n\n", dsp::measure::Report::kIdentical);

    // ---- 5. TWINS ---------------------------------------------------------
    // A program is a twin when it is below the threshold on EVERY excitation: a
    // held note cannot see a delay and a fast line cannot show a long release,
    // so a pair has to fail all five before it is called one sound.
    {
        std::vector<int> parent ((size_t) n);
        for (int i = 0; i < n; ++i) parent[(size_t) i] = i;
        std::function<int (int)> find = [&] (int a)
        { while (parent[(size_t) a] != a) { parent[(size_t) a] = parent[(size_t) parent[(size_t) a]];
                                            a = parent[(size_t) a]; } return a; };
        long pairs = 0;
        for (int i = 0; i < n; ++i)
            for (int j = i + 1; j < n; ++j)
            {
                bool same = true;
                for (int x = 0; x < kNumEx && same; ++x)
                    if (dsp::measure::difference (feats[(size_t) x][(size_t) i],
                                                  feats[(size_t) x][(size_t) j])
                        >= dsp::measure::Report::kIdentical) same = false;
                if (same) { const int a = find (i), b = find (j); if (a != b) parent[(size_t) b] = a; ++pairs; }
            }
        std::map<int, std::vector<int>> groups;
        for (int i = 0; i < n; ++i) groups[find (i)].push_back (i);
        int inTwins = 0;
        for (auto& kv : groups) if (kv.second.size() > 1) inTwins += (int) kv.second.size();
        std::printf ("5. TWINS - programs that are another program on EVERY one of the five sources\n");
        std::printf ("   pairs                               %ld\n", pairs);
        std::printf ("   programs involved                   %d of %d (%.1f %%)\n",
                     inTwins, n, 100.0 * (double) inTwins / (double) std::max (1, n));
        std::printf ("   EFFECTIVE DISTINCT SOUNDS           %d of %d\n\n", (int) groups.size(), n);
        for (auto& kv : groups)
            if (kv.second.size() > 1)
            {
                std::printf ("   [%d] %s", (int) kv.second.size(), bank[(size_t) kv.second[0]].name.c_str());
                for (size_t m = 1; m < kv.second.size(); ++m)
                    std::printf (" = %s", bank[(size_t) kv.second[m]].name.c_str());
                std::printf ("\n");
            }
        if ((int) groups.size() < n)
        { std::printf ("FAIL  the bank has %d effective sounds for %d programs\n",
                       (int) groups.size(), n); ++bad; }
    }

    // ---- the floors --------------------------------------------------------
    const auto noteR = dsp::measure::compare (feats[0]);
    std::vector<double> musical;
    for (int x = 1; x < kNumEx; ++x) musical.push_back (dsp::measure::compare (feats[(size_t) x]).median);
    std::sort (musical.begin(), musical.end());
    const double musicalMedian = musical[musical.size() / 2];

    std::printf ("\nHEADLINE  single-note median %.4f, musical median %.4f, %d effective sounds\n",
                 noteR.median, musicalMedian, distinct);
    if (noteR.silent > 0)
    { std::printf ("FAIL  %d renders are SILENT\n", noteR.silent); ++bad; }
    if (noteR.median < kFloorNote)
    { std::printf ("FAIL  single-note median %.4f is under the floor of %.2f\n",
                   noteR.median, kFloorNote); ++bad; }
    if (musicalMedian < kFloorMusical)
    { std::printf ("FAIL  musical median %.4f is under the floor of %.2f\n",
                   musicalMedian, kFloorMusical); ++bad; }
    return bad;
}

} // namespace

#include <functional>

int main (int argc, char** argv)
{
    bool self = false;
    for (int i = 1; i < argc; ++i) if (std::strcmp (argv[i], "--selftest") == 0) self = true;

    if (! self)
    {
        const int bad = run (false);
        std::printf ("%s\n", bad == 0 ? "PASS - the bank still spreads"
                                      : "FAIL - the bank has collapsed, see above");
        return bad == 0 ? 0 : 1;
    }

    std::printf ("SELFTEST - collapse every program onto one setting and require the probe to\n"
                 "notice. That is what a voice model reading only the category field does, and\n"
                 "it is how 707 PSR-730 voices came out as 38 effective sounds with every other\n"
                 "test green.\n\n");
    const int bad = run (true);
    std::printf ("\nthe collapsed bank produced %d findings\n", bad);
    std::printf ("%s\n", bad > 0 ? "PASS - the probe catches a collapsed bank"
                                 : "FAIL - the probe passed a bank of one sound");
    return bad > 0 ? 0 : 1;
}
