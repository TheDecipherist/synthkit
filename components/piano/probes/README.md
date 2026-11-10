# What to measure

Solera's two, in `Source/binding_probe.cpp`, and both exist because something was wrong:

**`--pianokeys`** plays all 88 keys at three velocities on every model and reports two things:
a silent key, and a key that is a HOLE against its neighbours. The second is the one that
matters - a key at 7 per cent of the keys either side sounds like a note that did not play, and
no "did it make a sound" check can see it. It allows a small, stated number of holes, because
after matching and retirement what remains is decay-rate variation between real recordings and
equalising that means reshaping them.

**`--pianoonset`** asks every recording in every set where its strike is, and it is the gate
this component most needed. Two numbers per recording, both facts about the finished audio and
both independent of the law that produced it:

- `strikeMs` - the time from the first sample a voice reads to half the note's own peak. Room
  noise never reaches half a note's peak, so a large number here means the kept audio opens
  with room, which is what a player hears as a late note.
- `frontDb` - the PEAK of the first millisecond, in dB under the note's peak. The opposite
  fault: an onset that lands inside the transient and takes its front off.

**Both halves or neither.** A fix for lateness ships a clipped attack if nothing measures the
other side, and the two bars together are what bracketed the constants. The measure matters as
much as the bar: `frontDb` over 5 ms of RMS could not tell a kept run-up from a clipped attack,
because a hard strike is at full level within two milliseconds, so five recordings read as
clipped at every setting of the law. One millisecond, and a peak rather than an RMS, separates
them - 1 ms of a 43 Hz bass note is a fortieth of a cycle, where RMS tells you which part of
the cycle you landed in and nothing about level.

**`--layers`**, the control sweep, with the nine piano rows in it. Its lesson is in the gate
list above: DAMPING and TWO STAGE need the key released, and MODEL needs more than one set
present.

## Why there is no standalone probe here, unlike the filter's two

The filter's probes compile in one line because its DSP is header-only arithmetic. This
component's loader is not: it opens files through `juce::AudioFormatManager`, so a probe for it
needs JUCE built, which is a build system and not a `c++` invocation. The two gates above drive
a plugin because the plugin is where JUCE already is.

**That is a reason, not an excuse, and it names the refactor that would remove it.** Four of the
five laws - the calibration gain, the common cap and its fade, outlier retirement and the
outward search for a missing key - never touch a file: they run over `notes`, in memory, after
the reading is done. Split those out of `loadFromFolder` and a probe can feed them synthetic
notes with a planted fault in each and need no JUCE at all. Only the first law, the onset trim,
genuinely needs a decoder. Until that split happens, a unit adopting this component writes the
two gates above; this file says what they must ask.

## One more thing both gates learned the hard way, 2026-10-07

**A gate must name the instrument it is measuring.** `--pianokeys` reported 0 of 264 silent on
all three models while this loader was reading ZERO files, because the host falls back to a
synthesised source and a synthesised source has no dead keys. An RMS check cannot tell an
instrument from its stand-in. So the gate asserts the recording COUNT first, per set and exact,
and only then measures audio. The two faults it was blind to were a `registerBasicFormats()`
that does not include FLAC and a renamed enum that rewrote three filename literals - both of
which left a folder full of recordings and a piano playing something else.
