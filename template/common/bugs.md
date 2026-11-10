# Bugs

One line per bug, written WHEN IT HAPPENS, not summarised at the end. Keep the
wrong answers in place — a bug that was diagnosed twice and reversed once is
worth more to the next reader than a clean list.

Format: date, what was seen, what it actually was, what fixed it.

---

*(nothing yet)*

<!-- Examples of the shape, from other machines:

2026-09-08  "the delays are hard to hear as delays" -> the tempo never reached
            the engine on any of nine units; the BPM field drove the audition
            source and the bar readout and nothing else. Fixed by
            shell::ShellHost::readTransport and measured in tools/sync_probe.cpp.

2026-09-07  "almost all patches sound the same" -> the voice model read the
            category string and nothing else, so 707 voices rendered as thirteen
            stand-ins. Found by tools/spread_probe.cpp; every other test green.

2026-09-06  "the controls don't do anything" -> bindings.json was compiled into
            the binary and read by no code at all; the real binding was a
            hard-coded if chain. 68 of 189 rows silent. Found by
            tools/controls_probe.cpp.
-->
