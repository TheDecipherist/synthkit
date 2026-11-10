# Sound-design laws every machine in this kit obeys

The per-machine references (`arcturus/docs/sound-design.md`, a unit's own `laws.md`)
hold that machine's constants. This file holds the laws that are not one machine's:
things that were got wrong on one unit, measured, and are now the floor for all of them.

Provenance markers are the kit's usual ones: `[FW]` firmware-exact, `[MAN]` stated in a
manual, `[SCH]` from a schematic, `[PUB]` an identified published part, `[COR]` arbitrated
from a factory corpus, `[EAR]` ear-calibrated and therefore a debt.

**Read `~/projects/synthcatalog` before designing any block.** 64 effects over 26 machines,
45 filters, plus oscillators, envelopes and LFOs, each with the machine's own law and its
provenance. A block written from memory when the catalog holds the machine's law is the
drift the catalog exists to prevent.

---

## 1. Distortion

### 1.1 Drive and distortion are different circuits, and a machine with both must not build one twice

The author, 2026-09-27: *"drive serving as an umbrella term, overdrive offering a mild and natural
sound, and distortion providing a heavy and aggressive sound."*

| | overdrive | distortion |
|---|---|---|
| clipping | soft, tops rounded off | hard, chopped flat |
| harmonics | warm, the note stays defined | sharp, the character changes |
| dynamics | very sensitive to how hard you play | roughly the same crunch soft or loud |

A unit that ships a DRIVE control and a DIST control has to put a genuinely different
circuit behind each. Solera shipped a tanh soft-clipper behind DIST, which is an
overdrive, so the two knobs did the same job and neither did the aggressive one.

**The useful way to spend one knob's travel is to move between them**: soft at the bottom,
hard blended in toward the top. The knob then changes character rather than only amount,
which is also the only thing that keeps it interesting past the point where gain stops
mattering (1.3).

### 1.2 A 50-per-cent duty cycle has NO even harmonics, so asymmetry has to be a BIAS

This is the one that is not obvious, and it silently undoes the most common attempt at a
warm distortion.

Even harmonics are what a valve, a transformer or a mismatched diode pair sound like, and
every catalogued machine that sounds warm is asymmetric: `[FW]` the Access Virus's four
dumped 514-point curves all reach further negative than positive, by 2x to 7.7x; `[SCH]`
the Metasonix TS-22's pentode stops dead at -2/3 while the positive side expands to +1.22;
`[EAR]` the Sherman Filterbank's input stage compresses the negative half harder, k = 1.0
positive against 1.35 negative.

So the obvious implementation is a curve that is steeper on one half. **It does not
survive gain.** A sine's zero crossings are symmetric. Drive any symmetric-in-time stage
hard and the output becomes a square whose edges sit at those crossings, so the duty cycle
is 50 per cent, and a 50-per-cent square contains only odd harmonics. Measured on Solera:
the even content FELL from 34.5 per cent to 13.6 as the knob came up. The valve character
was strongest where the distortion was weakest.

**Put the offset inside the gain instead:**

    u = (x + bias) * g          // NOT  u = x*g + bias

The crossing then sits at `x = -bias` whatever `g` is, so the DUTY CYCLE is set by the
control and survives any amount of saturation. Grow the bias with the control and the even
harmonics grow with it: measured 13 per cent to 47 across the travel, instead of
collapsing. That is what a valve's grid bias does as it is driven.

Writing `x*g + bias` looks equivalent and is not. At high gain the signal swamps a fixed
offset and the duty cycle returns to 50 per cent.

### 1.3 A soft clipper's control is finished long before its travel is

The author, 2026-09-27: *"why does it sound like the DIST knob sounds best at around 40 and then
flattens out the rest of the way to 100?"*

Two things saturate, and they compound:

**tanh saturates.** Once every sample is a flat-topped square, more gain cannot change the
shape, only the edge steepness. Past roughly g = 12 a sine at healthy level is already a
square. THD is also bounded: a square is about 48 per cent, so THD is the wrong thing to
aim at and a THD curve that flattens is not necessarily a fault.

**A concave amount taper front-loads the travel.** `2v - v^2` is at 0.64 by knob 40. That
taper is real, `[FW]` Novation Supernova II, and it is right for an AMOUNT control. The
mistake is putting the character terms behind it as well, because then every term is 64
per cent done at knob 40 and the top of the knob has nothing left to move.

`[EAR]` sherman-filterbank.md is the counter-law and it is worth following: *"Above the
detent the law is LINEAR in the knob, not exponential"*, `gain = 1 + (v-0.5)*2*23`, one to
forty-seven. Linear in the knob means every step adds the same amount.

**What to measure.** THD alone hides this. Sweep the control and look at all of:

| what | why |
|---|---|
| THD per step | the delta, not the value. A step adding 0.1 point is a dead step |
| even-harmonic share | it must not fall as the control rises (1.2) |
| spectral centroid | brightness should keep moving |
| harmonics within 40 dB | how far the spectrum reaches |
| output level | see 1.5 |
| THD against INPUT level | dynamics, which is what makes it an overdrive (1.1) |

Measure the shaper in isolation with a SINE. A saw's own harmonics swamp the measurement,
and a filtered saw through a real voice gives numbers that bounce and cannot be read.

### 1.4 Mids are the body; a bass cut in front is wrong for a synth

The author: *"Boosting the middle frequencies gives distortion its body and power. Too much
treble causes an annoying fizz, while too much bass makes the sound muddy."* Gillis on the
Filterbank: *"I looked at Marshall amps and studied how they achieved that warm sound."*

The guitar-pedal move is a bass cut before the clipper and a treble cut after. **Only half
of that transfers.** Keep the fizz cut, one pole around 9 kHz, because a hard clipper's
corners put energy at every harmonic up to Nyquist. Do NOT copy the bass cut: a 180 Hz
highpass in front measured as removing the fundamental of every bass patch, and a synth
bass IS its fundamental. The author's own low-end bands put the physical weight at 30-60 Hz and
the body at 60-90 Hz, which is exactly what a guitar highpass throws away.

### 1.5 Distortion gets LOUDER, and output normalisation is where that quietly dies

`[MAN]` roland-jd800.md: *"A JD-800 distortion gets LOUDER as you drive it, by design, and
the Level control exists to put it back."*

Two ways this goes wrong. Normalising the output flat removes the loudness that is part of
the effect. Worse, a normaliser written as a function of gain can be dead without looking
dead: `0.55 + 0.45*tanh(g)` is pinned at 1.00000 from g = 4 upward, because tanh saturates
too. That divisor sat in Solera across the whole knob doing nothing while its comment
claimed it gained about 4 dB, and the block actually got 3 dB quieter as it was driven.

A hard clipper's ceiling delivers the loudness on its own. Put a gentle peak follower
behind it to stop the loud half running away, `[FW]` novation-supernova2.md, and check the
measured level across the sweep rather than trusting the arithmetic.

### 1.6 DC blocking after an asymmetric clipper is structural, not cosmetic

`[MAN]` sherman-filterbank.md is explicit: *"the asymmetric input clipper generates a DC
offset that a low-pass filter passes at full gain. Without a blocker that offset dominates
the output whenever the cutoff is low."*

The same mistake read a 12 dB/octave slope as 5 dB/octave on the sibling Filter Factory,
because the meter was measuring the offset. One pole around 12 Hz. Every asymmetric stage
needs one, and 1.2 makes every warm distortion asymmetric by construction.

### 1.7 Where the nonlinearity SITS is a bigger decision than its curve

`[MAN]` The Sherman's drive stage is inside the resonance feedback loop, and that one
placement produces the manual's own most-quoted behaviour with no rule of its own: *"too
much input can push away the resonance peaks, giving you the impression that the resonance
knobs don't work properly."* The interaction is emergent.

Put a distortion after the filter and the resonant peak drives it, which is where a
screaming filter comes from and is not reachable from anywhere downstream of the VCA. Put
it inside the loop and the drive changes the filter's own behaviour. Neither is a curve
choice. Decide placement first, and say which you chose and why.

Rail the integrator STATES, not the feedback: at Q = 30 the state runs an order of
magnitude above the output, so compressing it removes the damping term and the filter
diverges. Same rule on the Sherman and the TS-22.

---

## 2. Exclusive control groups

### 2.1 A default-on member of an exclusive group is a phantom selection

An exclusive group drawn as a radio row, and read by the engine as a priority chain, must
reset to ALL OFF before a patch is applied. Not to its defaults.

Solera's oscillator 2 defaulted to `o2pulse = true`, which is a correct factory init for
an OB-Xa. The engine picks with `else if` in the order saw, pulse, tri, sine. So any patch
that wrote `o2saw = 1` and never mentioned pulse loaded with two lamps lit and played the
saw: 79 of 352 patches. Nothing in the patch asked for the pulse and nothing cleared it.

It surfaced as three separate complaints, none of which sounded like the same bug:

- multiple waveforms lit at once, *"which is not possible"*
- *"I still dont think any of the PULSE WIDTH buttons or knob is wired up"*, because every
  `o1pulse=1` patch was playing the default saw and PWM had no pulse to modulate
- *"I still havent heard one good bass patch"*, because every sine sub-bass was a saw

The author's fix, and it is the right one: *"add an actual RESET that applies before a patch gets
applied, so you are always positive that everything turns 100 per cent off."*

**Not literally every parameter to zero.** A cutoff, release or sustain of 0 loads every
sparse patch silent or clicking. It is the exclusive groups that have no safe non-zero
neutral, because any member left on is a selection the patch did not make. Groups go hard
off; continuous parameters keep their defaults.

### 2.2 Make the stored patch self-describing, and gate it

A reset fixes new loads. It does not fix a bank that only reads correctly under one set of
defaults. Write every member of a group into the patch, so the file says what it means
whatever the defaults later become, and normalise at the one seam where a patch's keys
become a file rather than at each authoring site.

Then gate it, because this returns otherwise. The rule that holds:

- **two waveforms on one oscillator is a bug.** Fail.
- **zero is a DECISION**, not a bug, when FM, a Virus stack, a supersaw or a SUB supplies
  the sound. Fail only when nothing does.

### 2.3 An audit is only as good as the defaults it assumes

Worth recording because it cost a whole pass. The first audit of this bug assumed
`o2saw` defaulted to true, found 127 affected patches, and migrated 76 files. Every
outcome it wrote was defensible and it fixed the wrong set: the real default was
`o2saw = false, o2pulse = true`, so the affected class was patches naming `o2saw` and the
audit had skipped exactly those.

Read the declaration, do not infer the default from the parameter's name or from what the
other oscillator does. Better, read the defaults from the running plugin rather than from a
file: the gate that finally found this asked the live parameter tree and printed the raw
values next to the patch's own keys, which made the answer obvious in one line.

### 2.4 Say a decision with the control that means it

Five patches expressed "no oscillator 2" by turning its default waveform off and naming
nothing in its place, which left the oscillator enabled and silent with its lamp lit. That
was the only way to say it before an `o2On` parameter existed. Once the control exists, the
old spelling is indistinguishable from a mistake. Migrate it.

---

## 3. Every module answers the sync question

The author, 2026-09-27: *"remember to verify that SYNC is perfectly wired up to REVERB because we
never checked that. Honestly it should be a law that all modules should check if SYNC is
needed."*

It is a law now. The reverb was the case that proved it was needed: it has three time bases
and had declared **no sync destination at all**, so nobody had ever asked the question of
it. Not a wrong answer. No answer.

### 3.1 The rule

**Every module declares every parameter with a time base, and a verdict for each.** Synced,
naming the division parameter that drives it, or free, with the reason it is not a rhythm.
A time base with no verdict is a fault, and the gate fails on it.

### 3.2 It has to be a declared table, not a scan

The obvious implementation is to scan for parameters whose unit is seconds, milliseconds or
Hertz. On Solera that finds 8 of 16. The delay's times are choice parameters, the pitch
echoes are plain floats, and the reverb's pre-delay is a float in milliseconds with no unit
string at all. Nothing in the source says "this is a time" except the person who wrote it.

So they say it once, in a table, with the verdict and the reason.

### 3.3 What makes it a law rather than a list

**Any parameter whose NAME looks like a time base and is absent from the table fails.**
Match on `Rate`, `Time`, `Echo`, `Predelay`, `Delay`, `Attack`, `Decay`, `Release`, `Glide`,
`Ms`. A name is what the person adding the next one gets right, and the check means they
cannot skip the question by omission.

It earned that on its first run, catching `revDecay`, which is unmistakably a time and which
the person writing the table (me) had not thought to declare.

### 3.4 The verdicts that generalise

Written out because they came up on every module and the reasoning transfers:

| time base | verdict | why |
|---|---|---|
| delay time, echo time | **synced** | it is a rhythm. This is the case sync exists for |
| LFO rate | **synced** | a wobble on the grid is a rhythm |
| chorus / modulation rate | **synced, per unit's choice** | see below |
| arp rate | **synced** | it is a rhythm |
| reverb pre-delay | **synced** | landing the tail off the beat is ordinary practice |
| envelope A, D, R | free | an attack is the front of a note, not a rhythm. A patch whose attack changed length when the drummer sped up would be a different patch |
| glide / portamento | free | a gesture between two notes, paced by the hand |
| reverb decay | free | decay is the size of the room, and a room does not change size with the tempo |
| reverb mod rate | free | the tail's shimmer is a timbre |
| filter cutoff, resonance | not a time base | no verdict needed |

**The chorus is the genuinely arguable one, and the unit decides.** Solera's own note, which
is the right way to think about it: *"a TREMOLO on a 1/8 is a rhythm and a ROTARY switching
speed is a performance; a Juno CHORUS at 0.5 Hz is a TIMBRE, and a chorus whose wobble
changes when the drummer speeds up is not a chorus. The program chooses."* So it is synced
with FREE available, and the program picks. The same argument is why a reverb's mod rate is
free: it is the same wobble one block further on.

### 3.5 And then measure the synced ones with a TEMPO change

The audit proves a verdict exists and a division parameter is wired. It does not prove the
tempo reaches the audio, and that is the trap that cost these repos more than anything else.
From the playbook: the TC 2290 read 500 ms at 120 BPM and **still 500 ms at 90**, because
the transport resolved into a private variable the shell could not see. Every division
measured correctly. Every test was green. Nine units shipped that way.

So a division change proves nothing. Measure the AUDIO out of the real processor at 120, 90
and 140 BPM, where a quarter is 500 / 666.7 / 428.6 ms, and put it in the unit's ctest.
Pair the audit with that measurement: the audit says every module answered, the clock gate
says the answer is true.

---

## 4. Insert, send and parallel, and which one an effect wants

The author, 2026-09-27: *"We should definetely add this to our sound design and make it law."*

### 4.1 The three paths

| | insert (series) | send (parallel) | parallel blend |
|---|---|---|---|
| what it does | the whole signal passes through | a COPY goes to a return, the dry is untouched | a heavily processed duplicate sits beside the clean one |
| the mix knob | 1 to 99 per cent, the balance | **100 per cent wet**, the send level decides the amount | whatever the blend wants |
| what it is for | shaping a track's core tone | sharing a space across several sources | punch without losing the clean transient |

**A time-based effect is almost always a SEND.** The author's brief: *"A delay is almost always
used as a send effect... rather than a series/insert effect"*, and the same for reverb.
The reasons are worth keeping because they are also the reasons it sounds better:
clarity (the dry keeps its transient instead of being partly replaced), independent
processing of the return, and one shared room rather than twenty.

**An insert is for when the effect IS the tone**: a delay feeding distortion so every
repeat is mangled further, a rhythmic stutter locked into an instrument's core sound, a
wash automated across one transition.

### 4.2 The law for a synth's internal effects

A synth's built-in delay, reverb, chorus and pitch are not plugins on a channel. The
player has one mix control per block and expects it to mean "how much of this do I hear".
So:

**Every internal effect block adds its wet on top of the dry. `out = in + wet * mix`.
Never `in * (1 - mix) + wet * mix`.**

Turning an effect up must make the sound bigger. That sounds too obvious to state, which
is exactly why it went unnoticed on a shipping instrument: Solera carried **four blocks
with three different mix laws**, and only the delay added.

    Delay    out = in + wet*mix               ADD
    Pitch    out = in*(1-mix) + wet*mix       CROSSFADE
    Reverb   out = in*(1-mix) + wet*mix       CROSSFADE
    Chorus   out = in*(1-0.5*mix) + wet*mix   HALF a crossfade, and half of nothing

Measured on one patch: pitch at full mix made it **6.7 dB quieter**, and in parallel
routing reverb at 25 per cent cost **4.8 dB**. Adding an effect removed level.

### 4.3 And in parallel routing a crossfade is not a preference, it is a bug

This is the part that is not a taste question. A host that offers parallel routing
extracts the wet by subtracting the dry it sent in:

    wet = block(dry) - dry

With the add law that is exactly `wet * mix`. With a crossfade it is `wet*mix - dry*mix`:
**the parallel path carries a negative copy of the dry** and partially cancels the main
signal. The louder the effect is set, the more of the instrument it removes.

So the add law is what makes a parallel switch mean anything at all.

### 4.4 What this costs, stated plainly

With the add law, mix at 100 per cent is dry plus full wet, so a **pure wet output is no
longer reachable from the mix control**. That is the right trade for an instrument - no
player of a hardware synth expects the reverb knob to remove the sound - but it is a real
limitation, and a unit that needs a 100-per-cent-wet mode should give it its own control
rather than overload the mix.

### 4.5 Measuring it, which took three attempts

Worth recording because the first two both produced confident wrong answers:

1. **Total RMS with the effect at several mixes.** Unusable. An effect's wet is a DELAYED
   COPY of the dry, so the two comb, and a comb legitimately lowers the total RMS at some
   settings. It reported eight failures on blocks that were working correctly.
2. **Subtracting the dry run sample by sample.** Worse, and it looked rigorous. The two
   runs are not phase-identical, so the difference was dominated by misalignment: it read
   **+4.77 dB where 25 per cent mix must be -12**.
3. **The tail after the note stops.** Correct. Play a short note, release it, and measure
   well after its release: the dry is silent, so what remains IS the block's output. No
   comb, no alignment, nothing to divide out. Then assert the tail RISES with the mix,
   which is a claim about the block's gain rather than about how its wet phases.

And check the measurement repeats before trusting it. Version 3 read the same numbers on
two consecutive runs; version 1 read +2.45 dB and -0.66 dB for the same setting, because
`reset()` does not re-phase a chorus LFO.

---

## 5. Measuring instead of auditioning

Every law above was settled by a number, and most of them had survived a listening pass
first. The habits that did the work:

- **Isolate the block.** A shaper measured through a real voice, filter and envelope gives
  numbers that bounce. The same shaper in twenty lines of Python gives a clean sweep in a
  second, and iterating on constants there costs nothing.
- **Sine in, for anything measuring added harmonics.** Any harmonically rich source
  measures itself.
- **Look at the DELTA per step**, not the endpoint. A control whose deltas collapse to 0.1
  has stopped working even though its value is still rising.
- **Print the raw parameter value next to what the patch asked for.** That one line turned a
  week-old mystery into a one-word answer (2.3).
- **Grid-search the constants against stated targets** once the metrics are right, rather
  than nudging by ear. State the target first: level must rise, even share must land in a
  band, THD must keep climbing.
