# The morph point's two probes

Both are standalone and need no build system:

```
c++ -std=c++17 -O2 -I <synthkit> -I <dspblocks>/include morph_gradient.cpp -o mg && ./mg
c++ -std=c++17 -O2 -I <synthkit> -I <dspblocks>/include morph_render.cpp  -o mr && ./mr <tag> <out-dir>
```

`morph_gradient` prints three maps of the glass: how far the curve moves per
unit of hand travel across, the same up and down, and what the filter IS at each
place. `morph_render` writes one WAV per edge, six seconds of the real
`SuperFilter` with the macro walking that edge.

**Use these rather than a corner-to-corner distance.** The metric they replaced
reported the bottom edge as dead because a highpass at 60 Hz and a notch at
16 kHz happen to look alike at the two ends, and three fixes went after a fault
that was not there. A local gradient cannot be fooled that way, and a rendered
sweep settles it in the only court that counts. The findings are in
`../morph-point.md`.
