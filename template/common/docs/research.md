# Research record

The trail: sources, hardware, ROM layout, and every finding with its address and
the reasoning. Corrections stay in place and are marked withdrawn.

Write findings HERE AS THEY HAPPEN. A summary written at the end loses the
reasoning, and the reasoning is what the next reader needs.

---

## 1. Sources

| what | where | notes |
|---|---|---|
| firmware / OS image | | |
| service manual | | |
| owner's manual | | |
| sysex specification | | |
| factory banks | | |
| schematics | | |

If something is missing, SEARCH FOR IT rather than recording it as unreachable,
and stage what you find in the archive. And read the SCANS, not the OCR: a
manual's tables and schematics are usually images the markdown conversion
dropped, and they are frequently the second source an open item needs. Render at
250-400 dpi and read the picture.

## 2. Hardware

CPU, DSP, converters, filter chips, clock rates, memory map.

## 3. The container format

Sysex type/length table, record layout, checksum, packing.

**On most of these machines the sysex layout IS the RAM layout** — no repacking
on receive, so `body[n]` is a fixed record offset. Check that before inventing a
mapping.

## 4. Corpus profile

Per-byte histograms over the factory bank. This is what tells you a byte's
neutral value (the mode is 64 for a bipolar control, 0 for a send), and it is
what catches a wrong decode: **a decode that produces a suspiciously flat
distribution is the decode.**

## 5. The coefficient builders

The shortcut to functional attribution, and the thing that finds the terms no
patch byte points at. Find the routine that writes each coefficient, note its
address, and read every instruction that reaches its accumulator.

| coefficient | routine | reads | notes |
|---|---|---|---|
| | | | |

**A byte that triggers a recompute is not a plain byte: look for the store that
is followed by a call.**

## 6. Findings

Numbered, dated, each with its evidence and its provenance tag.

*(nothing yet)*

## 7. Withdrawn

*(none yet)*
