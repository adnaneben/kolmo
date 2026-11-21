# Release targets

A version is released only when `accept.py` prints ACCEPTED on a results file from the Apple M5 benchmark
(`~/Downloads/kolmo-bench`, written by `night.sh` for every candidate branch).

1. **Every rival is beaten in its own game.** For each rival setting there is one kolmo setting that, across the
   whole file set, has at least the rival's ratio, at least 98% of its encode and decode speed, and is better on at
   least one of the three.
2. **Unchanged output never gets slower.** A file whose compressed size equals the baseline's may not encode or
   decode more than 5% slower.
3. **No setting moves backwards.** Against the baseline (1.0.1), per setting: ratio not lower, encode and decode not
   below 97%.
4. **No file gets larger.** At the same setting, no file may compress to more bytes than with the baseline.
5. **One thread means one thread.** Every tool is asked for one thread. A run whose CPU time exceeds its elapsed time
   by more than 3% used helper threads and is charged its CPU time. (lz4 1.10 with `-T1` does this: on a 20 MB file it
   takes 11.0 ms elapsed and 14.1 ms of CPU; kolmo takes 12.2 ms elapsed and 11.4 ms of CPU.)

Rivals measured today: lz4, zstd (-1, -3, -9, -19), brotli, gzip, xz, bzip3. To be added as they are installed:
OpenZL, zxc, JPEG XL (images), Pcodec (numeric), CLP (logs).

## Status

| Build | Verdict | Open |
|---|---|---|
| main (rename commit 87dfc86) | not accepted | lz4 -1, zstd -1, zstd -3 on encode speed; 19 unchanged-output slowdowns; `fast` encode 0.963x and `max` encode 0.865x of 1.0.1 |
| claude/next 6aeebcb (11 Nov) | not accepted: targets 2, 3 and 4 pass; lz4 misses target 1 | see below |
| claude/next 3f6c7d5 (11 Nov) | **ACCEPTED** (first time), with target 5 in force | none; needs a second accepted run before a merge to main |
| claude/next c591613 (11 Nov) | not accepted: one file larger than 1.0.1 at balanced (llm_bf16_47mb, +0.8%) | fixed in 7c6d1c2 (1 MiB planning slice) |
| claude/next 7c6d1c2 (11 Nov), merged to main | the ten original rivals beaten, plus zstd -22, brotli -11 and xz -9; no slowdown, no setting behind 1.0.1, no file larger | lz4 -9, added to the rival list the same day: decode at 97% of it (needs 98%) |

claude/next 6aeebcb on the M5 (six runs of the branch agree within a point on the lz4 figure):
- Target 1: nine of ten rivals beaten. zstd -1 (3.25x, 690, 1113 MB/s) by `fast -l 1 -Q` (3.34x, 722, 1247); zstd -3 by
  `fast -l 1 -q` (4.12x, 510, 1201); zstd -9 by `fast`; zstd -19, brotli and gzip by `hc -l 9`; xz -6 by `max`; bzip3 by
  `max -l 1`. Open: lz4 -1 (2.28x, 952, 1497) against `speed` (2.32x, 928, 1926): encode 97.5% of lz4, target 98%.
  kolmo is 11 to 29% faster than lz4 on incompressible input and 3 to 14% slower on compressible input; the per-file
  gaps fit a fixed cost of under a millisecond per run plus about 0.05 ms per MB.
- Target 2: no confirmed slowdown at 6aeebcb; the next run (e3e6b08, same code plus a static liblzma on macOS) confirms
  one, api.json at `max` (encode x0.92 to x0.94), and leaves lz4 at 97.7%. The Mac script now also records where a
  speed-mode run spends its time (start-up, planning, blocks) against lz4.
  A further run (339589d, same code) shows no confirmed slowdown and lz4 at 97.6%: lz4 is the only open item.
- Target 3: every setting at or above 1.0.1: rt x1.000 / x1.029 / x1.078 (ratio / encode / decode); fast x1.095 / x1.015 /
  x1.051; hc -l 9 x1.080 / x1.046 / x1.027; hc x1.075 / x1.192 / x1.040; max x1.085 / x1.037 / x1.091.
- Target 4: no file larger than with 1.0.1 (Mac set, and 264 file/setting pairs on the cloud).
- The cloud gate passes: 132 ladder outputs identical to the record build, 90 floors, 51 references, all test suites.

claude/next 3f6c7d5 on the M5, with runs that used helper threads charged their CPU time (target 5): all ten rivals
beaten. lz4 -1 (2.28x, 858, 1313 MB/s) by `speed` (2.32x, 907, 1844); zstd -1 (3.25x, 656, 1034) by `fast -l 1 -Q`
(3.34x, 712, 1212); zstd -3 (3.46x, 475, 964) by `fast -l 1 -q` (4.12x, 502, 1164); zstd -9 by `fast`; zstd -19 by `hc`;
brotli and gzip by `hc -l 9`; xz -6 by `max`; bzip3 by `max -l 1`. No confirmed slowdown on unchanged output, every
setting at or above 1.0.1, no file larger. lz4 on elapsed time alone is still 2 to 3% ahead on encode.

## The budget rule (12 November 2026)

This replaces fixed percentage thresholds as the test for whether a change is an improvement.

> Inside each mode, kolmo picks the smallest option whose measured cost stays within the cost of that mode's
> competitor on the same input. A change is an improvement when it makes a mode's output smaller and the mode stays
> within that cost, or when it lowers the cost of a mode that is over it without making the output larger.

| Tier | Modes | Competitor | Cost |
|---|---|---|---|
| Speed | `speed`, `balanced` | zstd -3 | compress time + transfer at a 1 Gbit/s link + decompress time, one thread (model 2 of `benchmark/TRADEOFFS.md`) |
| Archive | `hc`, `ratio` | OpenZL 0.3.0, its cheapest setting for the file (levels 6 and 9, every profile, trained where a trainer exists) | CPU dollars for one compression and 3.3 decompressions + storage dollars for the lifetime (model 3) |

`-x` (`ultra`) is outside the rule: it is the explicit "smallest of all" setting and is judged against PAQ8PX.
The planner types the data first (container header, number width and kind, table columns), then tries only the
options that type allows, then picks by the rule.

`python3 benchmark/accept/budget.py speed <M5 results>` and `... archive <OpenZL results>` print, for every setting,
on how many files it is within the budget, and the "oracle": the smallest kolmo result within the budget per file.

Three points of this definition are a reading of the rule, not something it states; each changes the verdicts:
1. **Speed-tier cost includes the link.** With compress and decompress time alone (no link), `balanced` is within zstd
   -3's cost on 10 of 35 files instead of 14, because nothing pays for a smaller output.
2. **Archive lifetime.** `TRADEOFFS.md` says "30 days or more". At 30 days CPU decides and `ratio` is within OpenZL's
   cost on 3 of 32 files; at one year storage decides and it is within on 24 of 32.
3. **Existing output is not made larger to meet the budget.** Where a mode is over the budget today, the work is to
   make it cheaper. Targets 3 and 4 (no setting behind 1.0.1, no file larger) still hold.

### Status against the rule

Speed tier, Apple M5, 35 files, commit 1079e90 (`budget.py speed`):

| Setting | Files within zstd -3's cost | Total size against zstd -3 |
|---|---|---|
| `balanced` | 14 of 35 | 0.843 |
| `balanced -q` | 22 of 35 | 0.861 |
| `balanced -Q` | 35 of 35 | 0.957 |
| `speed` | 24 of 35 | 1.467 |
| smallest within the budget, per file | 35 of 35 | 0.877 |

`balanced` is over the budget mostly on encode time: decimal arrays (cost x1.63 to x1.69: encode 2.8x to 3.0x zstd
-3, decode 1.01x), wide and typed tables (x1.30 to x1.46), a JSON number array (x1.41), 8-bit images (x1.30 to x1.36), logs (x1.31), and
three files where planning or a transform gains nothing and still costs 10% (mozilla, osdb, chr22.fa). `speed` is over on text, where its
larger output costs more in transfer than it saves in CPU at 1 Gbit/s.

Archive tier, cloud VM, 32 files, OpenZL timed in the same session. First at commit 3c5f599
(`benchmark/openzl/results-3c5f599.json`), then at 4e103b0 after `ratio` and `hc` stopped paying for float and
decimal planes that do not earn their CPU (`results-4e103b0.json`):

| Setting | Within OpenZL's cost, 30 days | Size against OpenZL | Within, one year |
|---|---|---|---|
| `balanced` | 27 -> 25 of 32 | 0.948 -> 0.950 | 21 -> 20 of 32 |
| `hc -l 9` | 27 of 32 | 0.863 -> 0.865 | 28 -> 27 of 32 |
| `eco` | 17 of 32 | 0.841 | 28 of 32 |
| `hc` | 4 -> 9 of 32 | 0.803 -> 0.804 | 25 -> 27 of 32 |
| `ratio -l 1` | 6 -> 13 of 32 | 0.748 -> 0.758 | 23 -> 25 of 32 |
| `ratio` | 3 -> 9 of 32 | 0.711 -> 0.721 | 24 -> 25 of 32 |
| smallest within the budget, per file | 29 -> 30 of 32 | 0.800 -> 0.775 | 28 of 32 (0.712 -> 0.719) |

Counts within a file or two move between sessions on files whose cost is within 3% of OpenZL's (the three magika
files, x-ray).

No kolmo setting is within OpenZL's cost on: lfw_subset.npy (kolmo is 9% larger at `ratio`, 43% at `balanced`),
magika.onnx (1 to 2% larger), xml at 30 days (the settings that are 12 to 30% smaller cost 7 to 16% more); at one year also magika.gguf and
magika.safetensors (equal size within 0.2%, 1 to 2% more CPU).

## A separate goal: balanced against zstd -3 on Silesia, round trip

Not part of the acceptance rule, tracked here. Silesia tar, in memory, one thread, same process, cloud VM, best of
seven: zstd -3 is 31.20% at 189 MB/s round trip (encode 230, decode 1063). `balanced` was 27.49% at 124 MB/s (66%) and
is 27.48% at 176 MB/s (93%; encode 221, decode 867). `fast -l 2` is 28.48% at 200 MB/s: 8.7% smaller than zstd -3 and
6% faster. Open for `balanced`: about 80 ms on 212 MB. Planning is 130 ms (record search on sao, 16-bit image
detection on mr and x-ray, the trials on mozilla, osdb and samba) and decode is 45 ms behind (image and line-class
inverses). The LZ pass is zstd level 3 in both, so passing zstd -3 at this ratio needs planning near zero or an LZ
stage cheaper than zstd's at the same ratio.

7c6d1c2 on the M5, with four rivals added (zstd -22 --ultra, brotli -11, xz -9, lz4 -9): thirteen of fourteen beaten.
lz4 -9 is 2.88x at 72 MB/s encode and 1285 MB/s decode; the closest kolmo setting, `fast -l 1 -Q`, is 3.34x at
724 MB/s encode and 1240 MB/s decode: 16% smaller and ten times faster to encode, 3% slower to decode. Merged to
main on the user's instruction after the regression gate (0 failures, 91 floors), the test suites, the 264-pair
comparison with 1.0.1 (0 larger) and targets 2 to 4 on the M5 all cleared.
