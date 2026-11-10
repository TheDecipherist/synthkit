// THE COEFFICIENT BUILDER: the one path from a stored program to the numbers the
// engine runs on. No JUCE, so the probes call the SAME function the processor
// calls and no law can drift between what is measured and what is heard.
//
// EVERY LAW IN HERE NEEDS A PROVENANCE TAG. Not a comment saying what it does -
// a tag saying who says so. See docs/provenance.md.
#pragma once
#include "Engine.h"
#include "../patch/Program.h"

namespace examplefx {

/** Program -> Coeffs.

    TODO(new machine): every line below is a LAW and every one is currently
    [REF] - ordinary DSP practice, nobody's machine. Replace them with the
    firmware's own arithmetic and re-tag. And WATCH FOR A GLOBAL THE HARDWARE
    DOES NOT HAVE: the H3500 had an invented global `mix` defaulting to 0.5 on a
    machine where each algorithm carries its own wet percentage, so every preset
    played half wet and fixing it moved the musical difference between presets by
    11x. Ask of every global: does the front panel have this control? */
Coeffs makeCoeffs (const Program&);

/** Every default must compute its own neutral value, EXACTLY.
    Returns the number that do not, printing each. */
int checkNeutralDefaults (const Program& defaults);
int checkNeutralDefaults();

} // namespace examplefx
