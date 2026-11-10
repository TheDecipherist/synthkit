// THE COEFFICIENT BUILDER: the one path from a stored program to the numbers the
// engine runs on.
//
// It lives here, on its own, without JUCE, for two reasons. The probes call the
// SAME function the processor calls, so no law can drift between what is
// measured and what is heard - that is how the PSR-730's probe stayed honest.
// And on the real machines this IS the shape of the firmware: the host CPU read
// the patch record and computed every coefficient, and the DSPs held only blocks.
//
// EVERY LAW IN HERE NEEDS A PROVENANCE TAG. Not a comment saying what it does -
// a tag saying who says so. See docs/provenance.md.
#pragma once
#include "Engine.h"
#include "../patch/Program.h"

namespace examplesynth {

/** Program -> Coeffs. Called on the message thread; the result is published to
    the audio thread through the processor's double buffer.

    TODO(new machine): every line below is a LAW and every one of them is
    currently [REF] - ordinary DSP practice, nobody's machine. Replace them with
    the firmware's own arithmetic and re-tag. Never ear-calibrate: the ear
    accepted the Supernova's wheel-to-pitch law at 1/4.7 of its true strength and
    its envelope Tmax at 1/5, and each felt fine in isolation. */
Coeffs makeCoeffs (const Program&);

/** The neutral check, from playbook 17a, and it is worth running on every
    machine before anything else.

    Three controls on the PSR-730 sat at the value their own header called
    neutral and computed 0.905, 0.15 and 2.70 - so every voice in a 707-voice
    bank played through a cutoff cut, an attack 6.7x too fast and a release 2.7x
    too long. Same family as the H3500's invented global mix at 0.5 and the
    Reverb 4000's Ambiator wet gain. Grep every default against its own curve and
    check the answer is EXACTLY the neutral one.

    Returns the number of parameters whose default does not compute its own
    neutral value, printing each. */
int checkNeutralDefaults (const Program& defaults);
/** The same check on the parameter table's own declared defaults. */
int checkNeutralDefaults();

} // namespace examplesynth
