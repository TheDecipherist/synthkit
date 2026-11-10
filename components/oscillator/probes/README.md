# OSCILLATOR probes

`word_laws.cpp` checks the TRANSCRIPTION, which is the half that can be wrong
in silence. Every law in `dsp/OscModule.h` was copied out of a reading of the
ROM, and a digit lost on the way in does not fail anything. The generator is
dspblocks' and is not re-implemented here, so it is not what drifts.

    c++ -std=c++17 -I../.. -I../../../../dspblocks/include word_laws.cpp -o word_laws && ./word_laws

It prints its anchors as well as passing, because a probe that only says "ok"
cannot be checked against the playbook.
