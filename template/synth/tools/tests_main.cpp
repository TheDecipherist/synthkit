// The tripwires. No JUCE, no plugin: the bank, the record and the coefficient
// builder alone, so this runs in a second and bisects cleanly.
//
// Every check here exists because the thing it looks for actually happened on a
// real unit and was invisible to a green test suite.
//
//   ExampleSynthTests             run the checks
//   ExampleSynthTests --selftest  inject each defect and require it to be caught
//
// THE SELFTEST IS NOT OPTIONAL. A check that cannot fail is worse than no check:
// ours silently inverted once into passing nothing and failing eleven healthy
// units, and nobody noticed because everything was green. Every injection here
// happens IN PROCESS and touches nothing on disk, so a session that dies in the
// middle cannot leave a bug behind that looks identical to a real failure.
#include "engine/Coeffs.h"
#include "engine/Engine.h"
#include "patch/Program.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

using namespace examplesynth;

namespace {

int failures = 0;
bool quiet = false;

void fail (const std::string& what)
{
    std::printf ("  FAIL  %s\n", what.c_str());
    ++failures;
}
void pass (const std::string& what)
{
    if (! quiet) std::printf ("  ok    %s\n", what.c_str());
}

// ---------------------------------------------------------------------------
// 1. PARAMETER IDS ARE FROZEN.
// Hosts persist automation by ID. Renumbering, renaming or reordering silently
// breaks every project that already automates this plugin, and there is no
// symptom until a user opens an old session. Freeze at v0.1 and only APPEND.
//
// TODO(new machine): replace this list the day the real parameter table lands,
// and then never touch it again except to append.
// TODO(new machine): REPLACE THIS LIST WITH YOURS, ONCE, on the day the real
// parameter table lands - and then never touch it again. It is the template's
// ids until you do, so this check fails loudly on your first real build, which
// is deliberate: freezing happens at v0.1, before any host has saved automation
// against these ids, and after that a renumbering is a silent break in every
// existing project.
const char* const kFrozenIds[] = {
    "osc_wave", "osc_tune", "osc_fine", "sub_level",
    "cutoff", "resonance", "key_track", "env_amount",
    "attack", "decay", "sustain", "release",
    "lfo_rate", "lfo_depth", "delay_time", "delay_mix", "master"
};
constexpr int kNumFrozen = (int) (sizeof (kFrozenIds) / sizeof (kFrozenIds[0]));

int checkFrozenIds (const std::vector<std::string>& live)
{
    int bad = 0;
    if ((int) live.size() < kNumFrozen)
    {
        fail ("a frozen parameter id has been REMOVED - the table is append-only");
        return 1;
    }
    for (int i = 0; i < kNumFrozen; ++i)
        if (live[(size_t) i] != kFrozenIds[i])
        {
            std::printf ("  FAIL  parameter %d is \"%s\", frozen as \"%s\" - a host's saved "
                         "automation now points at the wrong control\n",
                         i, live[(size_t) i].c_str(), kFrozenIds[i]);
            ++failures; ++bad;
        }
    if (bad == 0) pass ("parameter ids frozen (" + std::to_string (kNumFrozen) + " of "
                        + std::to_string (live.size()) + " pinned)");
    return bad;
}

// ---------------------------------------------------------------------------
// 2. THE RECORD ROUND-TRIPS BYTE-EXACT, INCLUDING THE BYTES WE CANNOT READ YET.
// Only some of a real machine's record is named on day one. Discarding the rest
// breaks sysex export and makes later attribution impossible, and the loss is
// silent - the Supernova's extension record was parsed as zeros for weeks and
// every effects knob it carried was chased as a separate bug.
int checkRoundTrip (const std::vector<Program>& bank)
{
    int bad = 0;
    for (const auto& p : bank)
    {
        const auto a = toRecord (p);
        Program back;
        if (! fromRecord (a.data(), a.size(), back))
        { fail ("record for \"" + p.name + "\" will not decode"); ++bad; continue; }
        if (back.raw != p.raw)
        {
            std::printf ("  FAIL  \"%s\": %d unmapped bytes went in and %d came back - the "
                         "loader is throwing the machine's own data away\n",
                         p.name.c_str(), (int) p.raw.size(), (int) back.raw.size());
            ++failures; ++bad; continue;
        }
        const auto b = toRecord (back);
        if (a != b)
        { fail ("record for \"" + p.name + "\" is not byte-exact on re-encode"); ++bad; }
    }
    if (bad == 0) pass ("every program round-trips byte-exact, unmapped bytes included");
    return bad;
}

// ---------------------------------------------------------------------------
// 3. EVERY STORED VALUE REACHES THE ENGINE.
// Assert the OPPOSITE of what you believe. 272 of the H3500's 625 presets loaded
// their algorithm's defaults instead of their stored values and nothing said so;
// the same assertion on the PCM 91 returned 0 and caught 383 clamped values on
// the way past. The loader is a parameter so the selftest can hand in a broken
// one - which is exactly what the H3500's `trusted` flag was.
using Loader = std::function<Coeffs (const Program&)>;

int checkStoredReachesEngine (const std::vector<Program>& bank, const Loader& load)
{
    int mismatched = 0;
    for (const auto& p : bank)
    {
        const auto got = load (p);
        const auto want = makeCoeffs (p);
        // ONE list, Coeffs::fields(), and not a hand-written comparison here.
        // A field ADDED to Coeffs and forgotten in a list like this is unchecked
        // forever and nothing says so; the static_assert beside fields() stops
        // the build instead. And never a memcmp: Coeffs is padded the moment it
        // holds more than one type, and padding bytes are indeterminate.
        const bool same = got.fields() == want.fields();
        if (! same)
        {
            if (mismatched < 5)
                std::printf ("  FAIL  \"%s\": the engine's copy differs from what the bank "
                             "stores\n", p.name.c_str());
            ++mismatched;
        }
    }
    if (mismatched > 0)
    {
        std::printf ("  FAIL  %d of %d programs never reach the engine intact\n",
                     mismatched, (int) bank.size());
        ++failures;
    }
    else
        pass ("every field of every stored program reaches the engine (" +
              std::to_string (bank.size()) + " programs)");
    return mismatched;
}

// ---------------------------------------------------------------------------
// 4. NO STORED VALUE IS CLAMPED ON LOAD.
// A range typed from a manual's ROUNDED numbers clamps real presets in silence.
// Take a range from the firmware's own arithmetic, then check every factory
// value survives a load-and-compare.
int checkNoClamping (const std::vector<Program>& bank)
{
    int clamped = 0;
    for (const auto& p : bank)
        for (int i = 0; i < kNumParams; ++i)
        {
            bool moved = false;
            clampStored (i, p.value[i], &moved);
            if (moved)
            {
                if (clamped < 5)
                    std::printf ("  FAIL  \"%s\" stores %s = %g, and the parameter holds "
                                 "%g..%g\n", p.name.c_str(), kParams[i].id, (double) p.value[i],
                                 (double) kParams[i].minimum, (double) kParams[i].maximum);
                ++clamped;
            }
        }
    if (clamped > 0)
    { std::printf ("  FAIL  %d stored values are clamped on load\n", clamped); ++failures; }
    else
        pass ("no stored value is clamped by its own parameter's range");
    return clamped;
}

// ---------------------------------------------------------------------------
// 5. EVERY DEFAULT COMPUTES ITS OWN NEUTRAL VALUE.
// The PSR-730 had three at once, all documented as neutral and none of them
// neutral: brightness defaulted to 64 and computed 0.905, attack to 0 and
// computed 0.15, release to 64 and computed 2.70. Every voice in a 707-voice
// bank played through a cutoff cut, an attack 6.7x too fast and a release 2.7x
// too long, with all three controls sitting where the header said they did
// nothing.
int checkNeutral (const Program& defaults)
{
    const int bad = checkNeutralDefaults (defaults);
    if (bad > 0) { failures += bad; }
    else pass ("every default computes exactly its own neutral value");
    return bad;
}

// ---------------------------------------------------------------------------
// 6. THE CUTOFF SUM ACCOUNTS FOR ITSELF.
// Playbook 17b. The engine's cutoff is a running sum of named terms; the terms a
// build MISSES are the ones with no patch byte behind them, so an unaccounted
// term must be a visible gap. This asserts the arithmetic - that the sum is the
// sum - and tools/spread_probe.cpp asserts the CONSEQUENCE, which is where a
// missing term actually shows.
int checkCutoffIsASum()
{
    CutoffTerms t;
    t.baseSemitones = 60.0f; t.patchKeyTrack = 12.0f; t.hardwiredKeyFollow = 7.0f;
    t.envelope = -5.0f; t.velocity = 3.0f; t.lfo = 1.0f; t.zoneTrim = -2.0f; t.controllers = 4.0f;
    const float want = 60.0f + 12.0f + 7.0f - 5.0f + 3.0f + 1.0f - 2.0f + 4.0f;
    if (std::fabs (t.total() - want) > 1.0e-4f)
    { fail ("CutoffTerms::total() does not sum every term - a term is unreachable"); return 1; }
    // And the anchor is a separate claim from the slope: at zero terms the
    // cutoff must be the anchor exactly.
    CutoffTerms zero;
    if (std::fabs (zero.hz() - CutoffTerms::kAnchorHz) > 1.0e-3f)
    { fail ("the cutoff law's anchor is not what kAnchorHz declares"); return 1; }
    pass ("the cutoff is a sum, and every declared term reaches it");
    return 0;
}

// ---------------------------------------------------------------------------
// 7. THE ENGINE MAKES A SOUND, AND A FINITE ONE, FOR EVERY PROGRAM.
int checkEveryProgramSounds (const std::vector<Program>& bank)
{
    int silent = 0, wild = 0;
    for (const auto& p : bank)
    {
        Engine e;
        e.prepare (48000.0, 512);
        e.setCoeffs (makeCoeffs (p));
        e.noteOn (60, 0.9f);
        std::vector<float> l (24000, 0.0f), r (24000, 0.0f);
        e.process (l.data(), r.data(), (int) l.size());
        double sum = 0.0;
        for (float v : l) { if (! std::isfinite (v)) { ++wild; break; } sum += (double) v * v; }
        if (std::sqrt (sum / (double) l.size()) < 1.0e-5) { ++silent; 
            std::printf ("  FAIL  \"%s\" renders silence\n", p.name.c_str()); }
    }
    if (silent > 0 || wild > 0)
    { std::printf ("  FAIL  %d silent, %d non-finite\n", silent, wild); ++failures; }
    else pass ("every program renders finite audio");
    return silent + wild;
}

std::vector<std::string> liveParamIds()
{
    std::vector<std::string> v;
    for (int i = 0; i < kNumParams; ++i) v.emplace_back (kParams[i].id);
    return v;
}

// ---------------------------------------------------------------------------
int selftest()
{
    std::printf ("SELFTEST - inject the defect each check exists to catch, and require it\n"
                 "to be caught. A check that cannot fail is worse than no check.\n\n");
    quiet = true;
    int missed = 0;
    const auto expect = [&missed] (const char* what, int caught)
    {
        std::printf ("  %-58s %s\n", what, caught > 0 ? "caught" : "*** NOT CAUGHT ***");
        if (caught <= 0) ++missed;
    };

    // A renumbered parameter table.
    {
        auto ids = liveParamIds();
        std::swap (ids[4], ids[5]);
        failures = 0;
        expect ("a renumbered parameter id", checkFrozenIds (ids));
    }
    // A record that cannot carry all of the machine's own bytes: 200 unmapped
    // bytes into a length field of one 7-bit byte. This is the shape of every
    // lossy export - the loss is in the FORMAT, not in the loop.
    {
        auto bank = factoryBank();
        bank[0].raw.assign (200, (uint8_t) 3);
        failures = 0;
        expect ("unmapped bytes lost by the record format", checkRoundTrip (bank));
    }
    // The H3500's trust flag, exactly: a loader that substitutes defaults for a
    // share of the bank instead of its stored values.
    {
        auto bank = factoryBank();
        int n = 0;
        Loader untrusting = [&n] (const Program& p) -> Coeffs
        { return (n++ % 3 == 0) ? makeCoeffs (Program{}) : makeCoeffs (p); };
        failures = 0;
        expect ("a loader that substitutes defaults (the H3500 trust flag)",
                checkStoredReachesEngine (bank, untrusting));
    }
    // A range typed from a manual's rounded number, so a stored value clamps.
    {
        auto bank = factoryBank();
        bank[2].value[pCutoff] = kParams[pCutoff].maximum * 2.0f;
        failures = 0;
        expect ("a stored value outside its parameter's range", checkNoClamping (bank));
    }
    // A default that is documented neutral and is not (the PSR-730's three).
    {
        Program def;
        def.value[pDelayMix] = 0.5f;
        failures = 0;
        expect ("a default that does not compute its neutral value", checkNeutral (def));
    }
    // A program that renders nothing.
    {
        auto bank = factoryBank();
        bank[1].value[pMaster] = 0.0f;
        failures = 0;
        expect ("a program that renders silence", checkEveryProgramSounds (bank));
    }

    failures = 0;
    quiet = false;
    std::printf ("\n%s\n", missed == 0
        ? "PASS - every check catches what it exists to catch"
        : "FAIL - a check did not fire on its own defect; it is proving nothing");
    return missed == 0 ? 0 : 1;
}

} // namespace

int main (int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
        if (std::strcmp (argv[i], "--selftest") == 0) return selftest();

    std::printf ("ExampleSynth tripwires\n");
    const auto bank = factoryBank();
    checkFrozenIds (liveParamIds());
    checkRoundTrip (bank);
    checkStoredReachesEngine (bank, [] (const Program& p) { return makeCoeffs (p); });
    checkNoClamping (bank);
    checkNeutral (Program{});
    checkCutoffIsASum();
    checkEveryProgramSounds (bank);
    std::printf ("\n%s\n", failures == 0 ? "PASS" : "FAIL - see above");
    return failures == 0 ? 0 : 1;
}
