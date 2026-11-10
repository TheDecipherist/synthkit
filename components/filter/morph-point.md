# The morph point: what it reaches, and what it does not

Measured 2026-09-28, against `ui/FilterScreen.h` as it stands. The author had asked for
it: *"The filter doesnt move 100% as a user would expect moving it up and down
and side to side. dont get me wrong it sounds fantastic, but you cant really
manipulate the left side alot. Lets see if we can make it even better."*

Nothing here has been changed. This is the survey, with the numbers, and one
candidate at the end that is offered rather than landed.

## How it was measured

Two probes and one renderer, all driving the component's own code:

1. **Local gradient.** At each place on the glass, move the hand two per cent and
   measure how far the drawn curve moves, in dB rms over 256 points. That is what
   "responsive" means to a hand: the change per unit of travel, at the spot the
   hand is on.
2. **Tone travel.** Render six seconds of a seven-saw stack through the real
   `SuperFilter`, with `applyMacro` walking one edge of the glass, then track the
   spectral centroid. How many octaves the TONE moves is the honest answer to
   "can I manipulate this", and it needs the block rather than the picture.
3. **Band reach.** At each x, sweep y and ask how many dB the 20-200 Hz,
   200 Hz-2 kHz and 2-20 kHz bands can be made to cover.

An earlier pass used corner-to-corner distance and it lied. The bottom edge
measured 10 dB of travel end to end only because a highpass at 60 Hz and a notch
at 16 kHz happen to look alike; everything between them is loud. A gradient
cannot be fooled that way, and neither can a rendered sweep. That mistake is the
reason the first three fixes attempted here all made things measurably worse.

## What the glass actually does

Tone travel along each edge, six seconds of drag, centroid range:

| drag | octaves the tone moves |
|---|---|
| top edge, left to right | 6.08 |
| left edge, bottom to top | 5.69 |
| bottom edge, left to right | 1.17 |
| right edge, bottom to top | 1.07 |
| **the diagonal, bottom left to top right** | **0.78** |

The longest gesture on the glass is the one that changes the sound least. That is
the headline, and it is not a rounding difference: the diagonal moves the tone
less than a seventh as far as the top edge.

Local gradient, dB of curve movement per 10 per cent of hand travel, up and down:

```
  y \ x    0.00   0.12   0.25   0.38   0.50   0.62   0.75   0.88   1.00
  1.00      5.0    5.4    5.1    3.8    5.3    8.9    6.0    2.4    1.4
  0.75      3.9    4.0    3.7    6.2    3.5    5.7    3.7    1.5    1.1
  0.50     10.5    8.2    8.1    4.5    3.4    4.0    2.6    1.1    0.9
  0.25     15.0   10.2    6.3    3.6    3.8    3.2    2.2    0.9    0.8
  0.00     12.6    8.5    5.2    3.4    3.3    2.8    1.1    1.1    0.9
```

The right third is dead. 0.8 to 2.4 dB against 12 to 16 in the bottom left, a
spread of fifteen to one across one window.

## Why, and it is one line of arithmetic

`applyMacro` gives the x axis two jobs. It sets the cutoff, and through
`3 * smoothstep(x)` it sets the shape as well. The vertical only reaches the
shape through `lift = (1 - y) * (1 - x)`, and that `(1 - x)` is the whole fault:
it takes the vertical's authority away as the hand moves right, and at x = 1 it
is exactly zero.

What the filter IS at each place says it plainly (cutoff Hz / shape, where
0 is lowpass, 1 bandpass, 2 highpass, 3 notch):

```
  y \ x           0.00          0.25          0.50          0.75          1.00
  1.00         60/0.00      242/0.47      980/1.50     3959/2.53    16000/3.00
  0.50         60/1.00      242/1.22      980/2.00     3959/2.78    16000/3.00
  0.00         60/2.00      242/1.97      980/2.50     3959/3.00    16000/3.00
```

Three consequences fall straight out of that table:

- **The whole right column is a notch at 16 kHz, at every height.** A notch at
  16 kHz removes nothing anybody hears. The right edge of the glass is silent
  territory by construction, which is exactly what the 0.9 dB reads.
- **A bright lowpass cannot be reached at all.** High cutoff and shape 0 is the
  most wanted sound on any synth, and it is the one combination the mapping
  refuses: cutoff high means shape high.
- **The top left is a lowpass at 60 Hz**, which has already taken everything
  away, so more resonance there has little left to act on. That is physics rather
  than a mapping fault, and it is why the left column's upper half reads 3.5 to
  5.0 while its lower half reads 12 to 16.

The low end confirms it from the other side. Sweeping the hand up and down at
x = 0 moves the 20-200 Hz band by 17.6 dB; at x = 0.6 the same gesture moves it
by 70.9 dB. `width = (0.85 - 0.65y)(0.35 + 0.65x)` narrows toward the left, so
the band's lower edge only reaches 40 to 54 Hz there and the bottom of the sound
stays where it is.

## Four experiments, three of which were wrong

Recorded because the next person will think of them too.

| tried | result |
|---|---|
| `kMacroLo` 60 Hz down to 20 Hz | vertical reach 93.9 to 58.3. Worse. Reverted. |
| inverting the width term | worse. Reverted. |
| shape as `2.6t + 0.4x` | horizontal reach 95.4 to 59.9. Worse. Reverted. |
| `macroX()` returning `cutNorm(cutoff)` so the dot sits on the corner | better on the left half (x = 0.2: 60.2 to 77.0) but it makes the dot JUMP under the finger in the left 16 per cent, because the macro's range stops at 60 Hz while the axis starts at 20. Not offered. |

The macro's dot is drawn over 0.04 to 0.96 while the corner it carries only
reaches 0.159 to 0.968 of the display, because the macro's cutoff range
(60 Hz to 16 kHz) is shorter than the axis it is drawn on (20 Hz to 20 kHz).
The dot is an honest inverse of the macro's own range, so it does not jump; it
simply sits left of the corner it commands. Worth knowing before anyone
"corrects" it.

## The candidate

One change, and it applies the rule the code already states rather than
inventing a new one. The author's own words in the header are *"when I pull the matrix
point down I expect the SHAPE to go up"*. Today that rule only operates on the
left, because of the `(1 - x)`. Let it operate everywhere:

```cpp
const float lift = (1.0f - y) * (2.0f - t);
set ("shape", std::clamp (2.0f * t + lift, 0.0f, 3.0f));
```

x now carries the shape as far as highpass, and the last step to notch is the
vertical's, at every x rather than only at the left. Measured against the same
probes:

| | now | candidate |
|---|---|---|
| up and down, right third (dB per 10 per cent) | 0.8 to 2.4 | 4.3 to 18.9 |
| shape's vertical span at x = 1 | 0.00 | 1.00 |
| tone travel, right edge | 1.07 oct | 1.47 oct |
| tone travel, the diagonal | 0.78 oct | 1.04 oct |
| tone travel, top edge | 6.08 oct | 6.31 oct |
| **the left column** | **unchanged, bit for bit** | |

The left edge renders byte-identical, because at x = 0 the two expressions are
the same number. The price is real and small: the bottom left loses some
side-to-side reach (11.7 down to 4.2 dB at the very bottom corner) because the
shape now climbs past highpass sooner along that edge.

**Nothing is stored.** `applyMacro` writes the seven controls and the macro's
x and y are read back from cutoff and resonance, so no patch holds a morph
position and no stored program can move under this change. It is a gesture, not
state, which is why it can be judged purely by ear.

Level-matched renders of both mappings along every edge are in
`solera/listening/morph-point/`.
