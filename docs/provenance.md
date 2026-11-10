# Provenance: who says so

Every law, constant, range and attribution in a synthkit unit carries a tag
saying **where it came from**. Not what it does, a comment already says that, but who is standing behind it.

This is not paperwork. It is the test surface. When a patch sounds wrong, the
first move is to check the provenance of the parameters involved, because an
`[EAR]` constant next to a `[FW]` one is where the bug almost always is. And a
tag is what lets a later reader tell a settled fact from an educated guess made
at two in the morning.

## The tags

| tag | means | example |
|---|---|---|
| `[FW]` | firmware, with an address | `[FW] 0x5770 reads @(0xE8,er6) = body[216]` |
| `[SCH]` | a schematic sheet, with the sheet and part | `[SCH] TB-303 sheet 3, 74LS628` |
| `[MAN]` | the manual, with a page | `[MAN] RX11 owner's manual p28` |
| `[DS]` | a component datasheet | `[DS] SSM2044, fig. 4` |
| `[PUB]` | a published source outside the manual | `[PUB] service bulletin, forum teardown` |
| `[COR]` | the factory corpus, a distribution over real programs | `[COR] body[223] peaks at 56, and 64+56 is 120 BPM` |
| `[REF]` | ordinary DSP or engineering practice, ours and not the machine's | `[REF] a stand-in until the firmware says otherwise` |
| `[EAR]` | fitted by ear | `[EAR] anchor, honestly labelled` |

## The rules that go with them

**Two sources or it is a guess.** One source is a hypothesis. The corpus tells
you the neutral value; the manual tells you the direction; the firmware settles
the law. An attribution confirmed by one of those and contradicted by none is
still `[EAR]` with a lead attached.

**`[EAR]` is legitimate and must be labelled.** Nothing is wrong with fitting a
constant by ear when there is no other source. What is wrong is letting it look
like a measurement. A tagged `[EAR]` is an item on the debt ledger; an untagged
one is a lie the next reader will build on.

**Never ear-calibrate a unit.** The ear accepted a wheel-to-pitch law at 1/4.7 of
its true strength, an envelope maximum at 1/5, an exponential LFO law where the
hardware squares, and three different cutoff laws in one day. Each felt fine in
isolation. Units come from the DSP consumer of the slot, or from an absolute
number in the manual; the ear only ORDERS the remaining debts.

**A wrong law that sounds good is worse than a known gap.** A gap is on the
ledger and someone will close it. A plausible wrong law gets built on.

**A withdrawal needs the same evidence as the claim it replaces.** One machine's
wave-directory index was withdrawn on two voices that looked implausible and
reinstated two days later when the same question was asked of all 1,366 elements
at once. Run a mapping over the whole table before crossing it out.

**Over-correction is a failure mode.** One byte went right  ->  wrong  ->  wrong  -> 
right over four passes because each pass re-read one source. Re-run ALL sources
on every change to an attribution, and log the ping-pong so the next reader sees
why the final answer stands.

**Keep corrections in place, marked withdrawn.** Never delete a finding. The
research file is evidence, and it has already caught wrong conclusions twice.

## Where they live

- `docs/laws.md`, every derived law, formula and constant, with its tag and a
  code reference, so the doc and the code can be diffed.
- `docs/research.md`, the trail: sources, ROM layout, every finding with its
  address and the reasoning.
- `data/params.csv`, a `provenance` column on every row, which the generator
  carries into the C++ so it is visible where the value is used.
- `todo.md`, the debt ledger: every `[EAR]` constant with a firmware lead.
