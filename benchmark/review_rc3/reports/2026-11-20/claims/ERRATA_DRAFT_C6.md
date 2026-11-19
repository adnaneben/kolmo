# Errata draft C6: audio against FLAC -8 (leaders report)

Dated 2026-11-19. Draft only; no original file was modified.

## Where

`benchmark/rc3/leaders/RESULTS.md` at `5071f388`, "Key findings", line 37:

> audio: `ratio` is 8.9% smaller than FLAC -8, `balanced` 4.6%, but FLAC decodes 2.3–15× faster;

The same figures sit in the specialist tables at lines 345-349 (1 thread) and 427-431 (10 threads), in the column
"vs smallest". That column means each tool against the smallest result (`kolmo -m ratio`), so it is not "Kolmo vs FLAC".
The rc3 report (`Claude outputs/kolmo-rc3-report.md`) does not repeat these figures.

## Raw rows

These are Kolmo through the Python binding and FLAC through libsndfile, both in process, on the 40 ESC-50 clips (17,641,760 input bytes):

- `Kolmo-Preserved/review-20261119T083123Z.lKHULl/raw/scratch-001/bench/lead/S1.jsonl` (threads 1)
- `.../lead/S10.jsonl` (threads 10)

Library `lib_sha256` 70132325... is the build labelled rc3 at `1536036`, not `5071f388`. Each label has 40 rows, one per clip, with no duplicate clips. Every row has `roundtrip: true`. The totals below are identical at 1 and 10 threads, except `--typed-huf` (see the last section).

| label | total bytes |
|---|---:|
| kolmo -m ratio | 6,537,582 |
| kolmo -x -l 2 | 6,537,582 |
| kolmo -m balanced | 6,840,066 |
| FLAC level 8 | 7,119,853 |
| FLAC level 5 | 7,325,511 |

## Recomputed

The percentages use 100 × (flac − kolmo) / flac, with FLAC -8 as the denominator:

| Kolmo setting | smaller than FLAC -8 | printed | what the printed figure actually is |
|---|---:|---:|---|
| `ratio` (and `-x -l 2`) | **8.1781%** | 8.9% | 100 × (7,119,853 / 6,537,582 − 1) = 8.9065%. This is how much *larger FLAC is than Kolmo*: the denominator is Kolmo, not FLAC. |
| `balanced` | **3.9297%** | 4.6% | 100 × (6,840,066 / 6,537,582 − 1) = 4.6268%. This is `balanced` against `ratio`, read off the "vs smallest" column. It does not compare with FLAC at all. Even "FLAC larger than balanced" would be 4.0904%. |

These match the runbook: 8.1781% and 3.9297%. The claim overstates Kolmo's lead over FLAC -8:

- for `ratio`, by 0.73 points (8.9 printed against 8.18 true);
- for `balanced`, by 0.70 points (4.6 printed against 3.93 true), and that figure came from the wrong comparator.

Cross-checks:

- `research/CODECS.md` (L34, L38) prints −8.2% and −4.0% against FLAC -8 for kaud profiles 1 and 0. That is a different measurement (Mac, 40 clips at 10 processes, per the round-4 report), but it agrees with 8.18% to rounding, not with 8.9%.
- The `-4.0%` there is not this run's 3.93% (that would round to −3.9%).

## Proposed correction (not applied)

Line 37 should read:

> audio: `ratio` is 8.2% smaller than FLAC -8, `balanced` 3.9%, but FLAC decodes 2.3–15× faster;

The specialist tables also need a note that "vs smallest" is relative to the smallest tool, so the FLAC rows (+8.9%, +12.1%) are "FLAC larger than `kolmo -m ratio`".

## Related anomaly in the same tables (not part of C6; recorded as L18 in the ledger)

`kolmo -m balanced --typed-huf` totals 8,821,456 bytes at threads 1 (line 350) but 6,840,066 at threads 10 (line 430), with the same library hash. 39 of the 40 clips differ. At 10 threads the result equals plain `balanced`. This needs an explanation and a retest.

## Reproduce (read-only)

```
python3 -I evidence/claims/audio_totals.py <preserved>/scratch-001/bench/lead/S1.jsonl <preserved>/scratch-001/bench/lead/S10.jsonl
```

The output is saved as `evidence/claims/c6_audio_totals.txt`.
