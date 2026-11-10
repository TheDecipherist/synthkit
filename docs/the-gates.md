# The gates: what each one catches, and how it is proved to work

Twenty-eight tests run in this repo, fourteen for each template. Every one exists
because the thing it looks for actually happened, on a real unit, and was
invisible to a green test suite.

A gate that cannot fail is worse than no gate. Ours silently inverted once into
passing nothing and failing eleven healthy units, with every build green
throughout, so each of the four probes ships with a `--selftest` that injects the
defect it exists to catch and requires itself to report it. Those injections run
IN PROCESS and touch nothing on disk: a session that dies halfway through an
injection leaves a bug that looks exactly like a real failure, and that has
happened.

| test | catches | its negative control |
|---|---|---|
| `tripwires` | frozen parameter ids renumbered; a record that loses the machine's unmapped bytes; a loader that substitutes defaults; a stored value clamped by its own range; a default that is not neutral; a program that renders silence | `probes_fail_when_they_should_tests` injects all six |
| `follows_the_clock` | the tempo not reaching the engine, measured in the AUDIO at three tempos; INTERNAL following the host (the write-back), with two tempo pairs; sync not overriding the knob; a rate destination off the beat; the raw `0xF8` path; Stop not releasing the lock | `probes_fail_when_they_should_sync` unhooks the destinations |
| `controls_are_alive` | a binding with no handler; a control that reaches no audio; a parameter with no control; a binding pointing at a control the spec does not draw, and a drawn control bound to nothing | `probes_fail_when_they_should_controls` cuts one control's write path |
| `bank_still_spreads` | a bank that has collapsed: stored fields not reaching the engine, programs resolving to the same coefficients, cutoffs bunching dark, twins, and a regression floor on the median difference | `probes_fail_when_they_should_spread` collapses every program onto one setting |
| `clock_gate_is_itself_checked` | the SHARED gate having inverted | runs it against a healthy host and three broken ones |
| `panel_renders` | the editor failing to draw at all | the PNG is the artefact; look at it |
| `generated_headers_are_current` | a CSV and its generated header disagreeing in silence | regenerates into a scratch directory and diffs |
| `panel_scrolls_and_still_hits` | a panel too big for its window drawn in one place and clicked in another; and the other way round, the scale or `controlBounds()` moving on a panel that still fits | `probes_fail_when_they_should_viewport` makes hit testing forget the scroll offset |

## What `shell::conformance::checkClock` can and cannot see

The shared gate is worth understanding precisely, because trusting it further
than it reaches is how nine units shipped deaf.

**It reads a `ShellHost` and never calls `processBlock`.** So it can see that a
unit declares destinations, that INTERNAL is genuinely internal, and that
EXTERNAL follows a transport, and it cannot see anything about whether the
engine received any of it.

**Five of its own numbered checks cannot be made to fail from outside.** Checks 2
(divisions land on the 192nd grid), 3 (OFF reports nothing), 5 (EXTERNAL follows
a tempo change), 6 (at least two clock sources) and 7 (`syncReachable` agrees
with the declared range) all read members of `ShellHost` that a subclass cannot
override, check 7 in particular compares `syncReachable()` against a
recomputation of exactly what `syncReachable()` computes, so it holds by
construction. They are checks on the SHELL, not on the unit.

`clock_gate_is_itself_checked` therefore drives the three defects that ARE
reachable, no destinations, the tempo write-back, and a dead internal clock, and everything a unit can actually get wrong is measured in the audio by
`tools/sync_probe.cpp` instead. That division of labour is deliberate and is the
reason the audio probe is not optional.

## What `panel_scrolls_and_still_hits` is really watching

A panel that no longer fits its window scrolls instead of shrinking, and the
whole feature turns on one thing. The mouse arrives in component pixels, the hit
boxes are in design units, and every handler has to put the scroll offset back
before it asks which control it reached. Miss it in one handler and every knob
under the cursor is the wrong one.

The reason this needs its own gate is that it looks perfect at offset zero, which
is where every other gate, every render and every casual click happens to sit.
`controls_are_alive` drives controls by id and would stay green through all of it.

So the probe does not compare a scrolled reading against another scrolled
reading, which would agree with itself no matter how wrong both were. It records
what each control resolves to with the panel at its origin, where a viewport
fault cannot show, and then requires that answer to survive every scroll offset
twice over: once by taking `controlBounds()` back through the panel's own
conversion, and once by dispatching a real `mouseDown` at that pixel and reading
the id off the event. Without the injection 0 of 21 controls move. With it, 21 of
21.

The same probe is what holds the no-op down, and that is half its value. Thirty-
five units are on this component. Before it touches anything else it asserts, on
the rect a unit's editor gives the panel today, that the scale is the same float
it always was, that `controlBounds()` returns the same rectangle the pre-viewport
formula produced for every control, and that an offset cannot be forced onto a
panel that fits.

What it cannot see is whether the panel LOOKS right, so `viewport_probe --png`
writes the panel fitting, parked at the origin and parked at the far corner, one
above the other. The first version of the edge cue drew four black gradients over
a dark navy plate and every number in this probe was green.

## Running them

```
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

Test names carry the unit's name (`ExampleSynth.tripwires`). That is not
decoration: CTest looks test PROPERTIES up by NAME across the whole project, so
two units in one tree with a test called `panel_renders` share a
`WORKING_DIRECTORY` and one unit's snapshot lands in the other's build folder, with both tests passing. That happened here between these two templates.

Read the OUTPUT, not the exit code. Several of these print numbers a person has
to look at, where each program's cutoff settles, the level per program, the
effective distinct sound count, and print them whether or not anything failed.


## Three meter faults the Emulator II found (2026-09-09)

All three were the MEASUREMENT, not the engine, and all three made a correct
build look broken - which is the failure mode that teaches people to ignore a
probe. They are fixed in both templates.

1. **A period read as 2x or 5x itself.** The autocorrelation took the largest
   correlation, and a periodic signal correlates just as well at every multiple
   of its period. 214 ms read as 1070, 400 ms read as 795. Take the SMALLEST lag
   within 90% of the best correlation that is a local maximum.

2. **A period 4% out on an exact clock.** The window hop puts +/-1 hop of grid
   error on the answer, which on a 250 ms period is 0.8% and on the 5 ms hop the
   template shipped with was 4%. Measure the LAST correlation peak in range and
   divide by how many periods it is: the same absolute grid error spread over N
   periods. The hop is now 2 ms and the error is 0.5%.

3. **A resonant filter is brightest TWICE a cycle.** The absolute beat-alignment
   assertion reported a synced LFO 115 ms off a 500 ms beat on an engine that
   was exactly on it, because with Q up the resonant peak crosses harmonics on
   the way up and on the way down. The synth template now sets resonance to zero
   for that measurement and says why.

The same class of fault, in an f0 estimator rather than a period meter, is
written up in the Emulator II's own `bugs.md` 6: a bell with partials 1, 5, 11
and 17 read an octave down and an electric piano with a strong 14th read exactly
13/14 flat. The fix generalises - do not extract a value and compare it, ASK
whether the signal does what the value would imply and require it to do so
better than at a neighbouring value.
