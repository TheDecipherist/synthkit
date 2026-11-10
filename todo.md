FIXED 2026-09-23: the 4 viewport test failures (ExampleSynth/ExampleFx
panel_scrolls_and_still_hits + their selftests). They were never the band
gesture and never the example panels. NativePanel::scale() caps at
sharpCeiling(), the export scale over the display's, and a 2x export on a 2x
screen puts that cap at 1.0; the probe asserted the uncapped
width/designWidth, 1.11290324 for its 1380 px component against a 1240 px
design. So it failed on every Retina machine and passed on a 1x one, which is
why it stayed "pre-existing" and unreproducible. The cap also leaves the plate
smaller than its window, so the panel centres it and preViewportBounds() had no
centring term, reading every control as moved. The two selftests were a
cascade: checkFits runs BEFORE the defect is injected, so its failure made the
selftest report a probe that could not see a fault it had never been given.
Fixed in both probe templates; sharpCeiling() is now public and the scale
policy in NativePanel.h has a fourth rule saying the cap exists.

OPEN: synthkit.panel_tab_and_path_seams_hold fails - "a band past the area's
bottom was drawn" and "placements did not track the bands". It is PRE-EXISTING
AT HEAD and belongs to neither the viewport fix nor the uncommitted EchoFold
work. An earlier version of this note blamed the EchoFold changes; that was
wrong. Checked both ways on 2026-09-23: reverting the viewport fix alone with
the EchoFold work still in the tree, it failed; and building the committed tree
in a clean worktree with NO EchoFold work at all, it failed with the same two
assertions. So the bands were already broken before either. LEAD: the two
assertions are about a band drawn past the area's bottom and about placements
not tracking shown/hidden members, so start at the band reflow and what it does
to the placement list, not at the painter.

FIXED 2026-09-23: sharpCeiling() now has a test, check 9 in
panel/tests/panel_probe.cpp. The cap's answer depends on the display, so a naive
test would only hold on the machine that wrote it; check 9 uses the cap's own
definition to stay display-independent. A 1x export gives 1/dev, at most 1 for
any dev >= 1, so the jmax floor decides and the cap is EXACTLY 1.0 everywhere.
An 8x export gives at least 2.6 even on a 3x display, comfortably above the
1.113 fit ratio used, so the cap cannot bind and rule 1 shows through untouched.
One case pins the cap, the other proves it gets out of the way. Shown to catch
its defect: removing the jmax floor from sharpCeiling() fires three of its four
assertions.

OPEN 2026-10-07: `components/envelope` is HALF a component - `ui/EnvScreen.h`
and nothing else. No `component.json`, so nothing declares its params or its
local ids; no `README.md`, so nothing says where its law came from; no probe.
Two synths link it (megasynth, solera) and both reach straight for the header,
which works and is exactly how the filter looked before it became a component.
It was absent from the registry table for that reason, and absence read as
"not a component" rather than "not finished" - so it is now a row that says
PARTIAL. Finishing it is the same three files the `piano` got on 2026-10-07:
declare the params with provenance, write what it is and where it came from,
and give it a probe with a `--selftest`. It also needs its row in
`Siblings.cmake`'s component list, which it does not have - it is reached today
through the include path another component's target supplies, so it works and
nothing declares it. Solera's `Source/SoleraEditor.h` is the only include of it;
an earlier note here said megasynth used it too and that was wrong.
