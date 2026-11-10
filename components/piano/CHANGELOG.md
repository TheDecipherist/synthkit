# What a synth gains by rebuilding against this today

Newest first. A host that carries a COPY of this component reads this file to find
out what its copy is missing.

## 2026-10-07 — the onset is a step, not a level. Re-copy for this one.

**A host pinned before this date plays soft notes hundreds of milliseconds late.**

`loadFromFolder` set its onset threshold at two per cent of each file's own peak.
On a soft recording that sits UNDER the room noise, so the search returned the
file's first sample and the kept audio began with the room. Measured on the Iowa
Steinway: middle C at pp started **431.5 ms** in, and **78 of 79 pp recordings**
started more than 25 ms in, median 238 ms, worst 657 ms.

It cannot show at ff. The room is identical in every recording and only the note
changes, so an ff note puts its room 45 dB down and any threshold works.

**No level threshold can fix it.** A pp recording's room sits 14 dB under the note
and the attack's own first window sits at 15 dB under; they overlap. The onset is
now the biggest RISE before the loudest window, which has 15 dB of margin where a
level has none, then walked back out of the transient.

After: every layer of all three reference sets starts within 25 ms, pp middle C at
16.5 ms. The pp layer also keeps 87 recordings instead of 79 - a correct onset
moves the body window that the level-matching law measures, so eight takes stop
reading as outliers and play as themselves rather than as a transposed neighbour.

### If your copy has its own fix for this, it is probably a level threshold

One did: "the first millisecond at a tenth of the loudest millisecond", which is
−20 dB. Measured against all 404 recordings in the three sets, side by side with
this component's law:

| | a −20 dB level | this component |
|---|---|---|
| middle C at pp | 212.5 ms | **16.5 ms** |
| Steinway pp over 25 ms | 41 of 84 | **0 of 87** |
| Steinway pp median | 76.1 ms | **14.5 ms** |
| Steinway pp worst | 488.2 ms | **24.5 ms** |

It halves the symptom and leaves 41 notes late. Drop the local change and take
this.

### New API, for the gate you will want

- `strikeMsOf (dyn, key)` - how far into the kept audio the strike is: the time to
  half that note's own peak. Room never reaches half a peak, so a large number
  means the audio opens with room.
- `frontDbOf (dyn, key)` - the peak of the first millisecond in dB under the note's
  peak. The opposite fault: an onset that cut the attack's front off.
- `trimMsOf (dyn, key)` - how much was cut from the front. `frontDbOf` is only
  meaningful where this is non-zero; a file already trimmed to its strike has no
  run-up to keep.
- `worstStrike (dyn)` - the latest-starting recording in a layer, and how late.
- `hasLayer (dyn)` / `layers()` - a one- or two-dynamic set leaves rows empty.

**Check both directions or a fix for either ships the other.** `probes/README.md`
says what to ask.

### Diagnostics, for arguing about a constant with numbers

- `SYNTHKIT_PIANO_ONSET_TRACE=1` - the floor, the loudest window and the chosen
  onset, per recording.
- `SYNTHKIT_PIANO_ONSET_DUMP=<part of a filename>` - the whole window RMS curve in
  dB under the loudest window. This is what settled the law; a single number for
  the room is what got it wrong twice.

## 2026-10-07 — lifted out of Solera as a component

The loader, its five recording laws and the nine controls' declared meanings were
`Source/Steinway.{h,cpp}` in Solera. Every law turned out to be about RECORDINGS
rather than about Solera - that a set is not level-matched, that a take can be 30 dB
out of line, that a note is four times longer than anything plays of it, that a
missing note is better transposed than silent - so the next machine with a piano
needs all of them.

Solera keeps one adapter, `Source/Steinway.h`, because its PCM half speaks the M1's
sample interface and this component speaks its own.

Four faults the move itself introduced, all invisible, all fixed here:

1. `registerBasicFormats()` is WAV and AIFF. **Not FLAC.** The loader found 262
   files, opened none, and reported itself absent.
2. A rename that qualified the nested `Dyn` enum rewrote three string literals, so
   `dyn == "pp"` became `dyn == "Samples::pp"`. Compiles, no warning, parses nothing.
3. Loading one model on demand made a gate print "not on this machine" on a machine
   with all three sets on disk. A gate loads what it is about to measure.
4. A key-coverage gate passed, 0 of 264 silent on all three models, **while the
   loader was reading zero files** - the host falls back to a synthesised source and
   a synthesised source has no dead keys. Assert the recording COUNT before
   measuring audio.
