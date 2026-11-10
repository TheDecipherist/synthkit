// The program record: what the machine stores, and the only path a value takes
// from a bank into the engine.
//
// TWO RULES THIS FILE EXISTS TO ENFORCE, both from units that shipped wrong:
//
//  1. ROUND-TRIP UNMAPPED BYTES VERBATIM. On a real machine most of the record
//     is unnamed on day one - 230 of the Supernova's 270 body bytes still are -
//     and discarding what you cannot yet read breaks sysex export and makes
//     later attribution impossible. `raw` carries the remainder untouched.
//  2. HALF A PATCH IS NO PATCH. Prove the loader touches every byte the machine
//     stores, and exports the same set back out, BEFORE writing any engine code.
//     The Supernova's extension record was parsed as zeros for weeks and every
//     effect, pan, comb and EQ default was chased as a separate bug.
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "../generated/ExampleSynthParams.h"
#include "../generated/ExampleSynthFactory.h"

namespace examplesynth {

/** One program. `value` is indexed by ParamIndex, so there is exactly one place
    a parameter's number lives (the generated table) and no second copy of a
    range anywhere. */
struct Program
{
    std::string id      = "U00";
    std::string name    = "Init";
    std::string category = "Other";
    std::string bank    = "FACTORY";
    float value[kNumParams] {};

    /** The bytes this build does not yet understand. They travel with the
        program and come back out of exportSysex() unchanged.
        TODO(new machine): fill this from the real record and shrink it as
        attribution lands. An empty `raw` on a real machine means the loader is
        throwing the machine's data away. */
    std::vector<uint8_t> raw;

    Program()
    {
        for (int i = 0; i < kNumParams; ++i)
            value[i] = kParams[i].defaultValue;
    }
};

/** The factory bank, built once from the generated table. */
std::vector<Program> factoryBank();

/** A program as a byte record, and back. Byte-exact both ways: the test in
    tools/tests_main.cpp round-trips every factory program and compares, which is
    the only thing that proves a patch made in the plugin goes back to the
    hardware.

    TODO(new machine): replace with the machine's OWN sysex layout. On most of
    these machines the sysex layout IS the RAM layout, so body[n] is a fixed
    record offset and there is no repacking on receive - check that before
    inventing a mapping. */
std::vector<uint8_t> toRecord (const Program&);
bool fromRecord (const uint8_t* data, size_t n, Program& out);

/** Clamp a stored value into its parameter's range, and say whether it moved.

    Call this on load and COUNT the moves. A range typed from a manual's rounded
    numbers clamps real presets in silence: the Reverb 4000's firmware grid is
    20*2^(n/24), so its printed "500 Hz" is 507.968, and typing the printed
    numbers clamped 91 stored values across 174 programs. The PCM 91 had a 1 ms
    minimum in one table against 0 in another and silently clamped 383 of 433
    presets into a 1 kHz comb. Both were invisible. */
inline float clampStored (int paramIndex, float v, bool* moved = nullptr)
{
    const auto& p = kParams[paramIndex];
    const bool out = v < p.minimum || v > p.maximum;
    if (moved != nullptr) *moved = out;
    return v < p.minimum ? p.minimum : (v > p.maximum ? p.maximum : v);
}

} // namespace examplesynth
