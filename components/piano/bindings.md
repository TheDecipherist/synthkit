# Wiring the PIANO component

Work down it. Every row is something that was once missed.

| # | row | done when |
|---|---|---|
| 1 | The folder exists and `loadFromFolder` returned true | `loaded()` is the file count and `missing()` is 0 |
| 2 | Every one of the nine controls reaches the audio | a control-sweep gate reports 0 dead. **Eight of the nine were declared and read by NO line of DSP for weeks**, and no gate could see it because none of them was in one |
| 3 | DAMPING and TWO STAGE are measured with the key RELEASED | they act only after note-off; held, they are correctly inaudible and a gate that holds the note calls them dead |
| 4 | MODEL changes the sound | it selects a different recording set, so it must |
| 5 | Every key sounds, on every model, at every velocity | 88 x 3 x models, through the real processor |
| 6 | No key is a hole against its neighbours | a bad take reads as a note that did not play; "it made a sound" cannot see it |
| 7 | Only the selected model is resident | three sets at eight seconds a note measured **1.01 GB**. Load the others on demand, off the audio thread |
| 8 | A retired take is reported, not hidden | a gate whose bar is "every key plays its own recording" fails on a deliberate retirement; give it `retired()` |
| 9 | Every recording STARTS when the key does | `--pianoonset`-style check: the time from the first sample a voice reads to half the note's own peak, on every key and every dynamic. **A soft note is where this breaks and a loud one is where it cannot**, because the room is the same in both and only the note changes - so a latency check at one velocity proves nothing about the others. The first version of this ran at velocity 112 and read 3.3 ms while the real answer at velocity 30 was 425 |
| 10 | And no attack is CLIPPED by fixing row 9 | the peak of the first millisecond, in dB under the note's peak. Both directions or neither: overcorrecting an onset opens the sample inside its own transient and the piano sounds clicky, which no late-note check can see. Measure 1 ms and a PEAK - 5 ms of RMS cannot tell a kept run-up from a clipped attack, because a hard strike is at full level within two |
