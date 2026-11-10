# The PIANO component

One multisampled acoustic piano, loadable by any synth on the rack, with the handling a folder
of real recordings turns out to need.

```
component.json        the nine controls, with ranges and provenance
dsp/PianoSamples.h    the set, the five loading laws, and sampleFor
dsp/PianoSamples.cpp  the loader
bindings.md           the wiring checklist - work down it, tick every row
probes/README.md      what to measure, and the two gates Solera runs
```

## Why it is a component

Solera grew this for its PIANO oscillator type, and every law in it is about RECORDINGS rather
than about Solera. The next machine with a piano in it needs all five and none of them are its
own:

1. **The recordings begin with the room.** The median note in the Iowa Steinway set has 250 ms of
   silence before the strike and the bottom three at ff have over four seconds. A key press
   played the studio for a moment and the low keys were unplayable.
2. **A set is not level-matched.** Adjacent notes at pp varied seven-fold in that set - 88
   separate recordings of one instrument on one day. Through the engine that reached 24 dB, which
   reads as a note that did not play.
3. **A take 13 dB below both its neighbours is a bad take.** No honest gain fixes one; lifting it
   raises its room noise with it. It is retired and the key is transposed from a neighbour.
4. **A missing note is transposed, never borrowed from another dynamic.** A pp note answered with
   its ff recording is a piano that gets louder when you play softer.
5. **A note is four times longer than anything will play of it.** The Iowa files are 35 seconds
   and eight are used.

## What it is NOT

Not a piano MODEL. `dspblocks/include/dsp/PianoString.h` is the physical string, with real
inharmonicity, and is a different instrument with different laws - its own header lists what it
does not have yet (no hammer, no coupled unison strings, no soundboard). A machine chooses one;
they are not layers of each other.

Not an engine. It returns a sample and a root key. Reading it, pitching it, filtering it and
enveloping it are the host's voice's job.

## How a synth links it

The same way it links the FILTER component:

```cmake
include_directories("${SYNTHKIT_ROOT}/components")
```

and then `#include <piano/dsp/PianoSamples.h>`. The host writes ONE adapter between its own
frozen parameter ids and these local names - Solera's is `Source/Steinway.h`, which also bridges
this to the M1 sample interface its PCM half already speaks.

## The recordings

They are not in this repo. Three public-domain sets live in
`~/projects/dspblocks/reference/{steinway,upright,honkytonk}`, each with a README naming its
recordist, its instrument and its licence, stored trimmed to the eight seconds the loader keeps
and encoded as FLAC. `dspblocks/tools/trimflac.py` is the conversion and it **no longer knows
anything about the onset** - it keeps every file from its first sample and caps the length at
the loader's scan window. It used to duplicate the threshold, with a comment promising the two
would be kept in step; the law changed on 2026-10-07 and the script did not, which is what a
duplicated constant always does. The loader owns that law alone now.

One consequence of the old script survives in the files on disk: because its threshold was a
fraction of FULL SCALE rather than of each file's peak, it never fired on a pp recording at all
and the eight-second cap was taken from sample zero. The Steinway's pp notes therefore hold
their room plus about 7.6 seconds of note rather than 8 - 364080 samples against 384000. What
is missing is the end of a decayed tail and the loader fades the last 40 ms regardless, so it
is inaudible; re-running the conversion over the original downloads would repair it, and those
were deleted after the first pass, so it is a 2.5 GB download rather than a command.
