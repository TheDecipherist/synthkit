# The debt ledger

Every `[EAR]` constant, every unattributed byte, every deferred decision — with a
LEAD, so the next session knows where to look rather than starting over.

An item without a lead is a wish. An item with one is work.

---

## Laws still fitted by ear

| law | where | lead |
|---|---|---|
| everything in `src/engine/` | `docs/laws.md` | nothing is this machine's yet; the whole engine is `[REF]` |

## Bytes not yet named

| offset | what is known | lead |
|---|---|---|
| — | the record layout has not been decoded | the sysex spec, then the firmware's coefficient builders |

## Cutoff terms not yet accounted for

`CutoffTerms` in `src/engine/Engine.h` carries an empty field for each term this
build does not model. Every `[TODO/FW]` there is a debt, and a zero in one of
them is a CLAIM that the firmware sums nothing the record does not name — which
was wrong by 2.3 octaves on one machine.

| term | status | lead |
|---|---|---|
| hardwired key follow | unmodelled | read the routine that writes the filter coefficient; the M1's was 71 counts per key |
| velocity into EG intensity | unmodelled | decoded and read by nothing on the M1 while 103 of 126 halves carried one |
| per-zone / per-sample trim | unmodelled | a sample-based machine normalises somewhere |
| controller sweeps | unmodelled | they sum in at zero, so they are invisible at rest |

## Deferred decisions

Write these down with the same weight as laws, so nobody re-opens them by
accident.

*(none yet)*
