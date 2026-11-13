# Regression ledger

What each change cost, measured against 1.0.1 and against main, and what was decided about it.
Last full pass: branch `claude/dec64`, 12 November 2026, cloud VM (2 cores, x86-64), one thread, CPU time
(user + system), 81 files / 860 MB: the test corpus plus two holdout sets that were built after the code they test.
Method: size, encode CPU and decode CPU per file for 1.0.1, main and the branch, best of three runs at speed and
balanced, single runs at ratio.
The M5 confirmed the branch at 6fa4144 (see below); later commits are not confirmed yet.

## Totals

| Setting | Size 1.0.1 | Size main | Size branch | Encode CPU 1.0.1 / main / branch | Decode CPU 1.0.1 / main / branch |
|---|---|---|---|---|---|
| speed core (`rt`) | 275.8 MB | 275.8 MB | 275.8 MB | 2.3 / 2.0 / 2.0 s | 1.21 / 0.89 / 0.87 s |
| balanced | 172.3 MB | 166.6 MB | 144.6 MB | 5.5 / 4.3 / 5.3 s | 1.62 / 1.30 / 1.43 s |
| ratio | 128.5 MB | 121.9 MB | 112.2 MB | 262 / 262 / 228 s | 11.6 / 13.3 / 11.4 s |

No file is larger than 1.0.1 or than main at any of the three settings. On the 48 files whose output is unchanged
from main, encode CPU is within 3% and decode within 5% (single runs). Times are from the pass before integer
segments were added; that change was re-timed on 16 files (interleaved, best of 5 to 11) with no difference outside
noise. CPU time on this VM moves by up to 25% between passes, so a single-pass slowdown under that is not evidence.

## Fixed

| Regression | Where it came from | Status |
|---|---|---|
| pandas pickle of int64 columns + strings 69% larger than 1.0.1 at balanced (219,892 vs 129,908) | balanced quick analysis, on main since 7c6d1c2 | Fixed on the branch (probes away from the trial slice; a marked plan gives way to integer segments): now 98,236, 24% smaller than 1.0.1. **Still on main until the branch is merged.** |
| ratio encode 47% slower overall (262 -> 384 s) | coding decimal and transformed blocks both ways | Fixed: a block-sorted slice decides whether the plain attempt is worth making. Now 228 s, faster than main. |
| 2 MB NDJSON at ratio: 6x encode time, same output | plain LZMA2 attempt on every small transformed block | Fixed by the same gate (0.20 s -> 0.22 s). |
| mixed decimal / computed table at balanced: 4% smaller for 1.8x decode time | DEC64 chosen on a thin margin | Fixed: DEC64 must beat the next plan by 7%. |
| digits table 15% larger at ratio | DEC64 scored with zstd, coded with LZMA2 | Fixed before it left the branch (both codings, smaller kept). |
| 16-bit model file 0.84% larger than 1.0.1 at balanced | 256 KiB planning slice | Fixed in 7c6d1c2 (1 MiB slice restored). |
| `ultra` larger than `ratio` on images (RGB photograph 404,896 vs 381,565; image array 276,549 vs 245,200) | `ultra` kept the graph or transform chosen by its zstd score | Fixed: image model, and plain coding tried for transformed blocks. Now 323,467 and 203,747. |
| `ultra` 20x slower on model weights for no gain (1.3 -> 26 s on a 3 MB file) | plain context mixing tried for every numeric graph block | Fixed before it left the branch: tried only when plain block sorting is within 2%. |

## Open, fixable

| Regression | Size of it | Fix | Effort |
|---|---|---|---|
| Marked plans code every block twice | int64 pickle: encode 13 -> 40 ms at balanced (now 24% smaller for it, see Accepted); a file where plain wins every block would pay the same for nothing | Stop trying the plan after the first blocks all lose; needs an order between block jobs to stay deterministic | 1 to 2 days |
| Short probes cannot see long-range repeats | The 32 KiB probes said the integer graph wins on the int64 pickle; whole blocks said plain wins by 41% | Probe with history (code the probe after its preceding 1 MiB), or score plans on two slices | 2 days |
| DEC64 encode cost | 1.7x to 3x encode CPU on decimal files at balanced (still 90 to 230 MB/s) | The trial codes a 1 MiB slice that the block coder codes again; reuse it, and code leaves once instead of trying each leaf codec | 2 days, est. 1.3x to 1.5x back |
| DEC64 decode cost | 1.2x to 1.9x decode CPU at balanced (still 420 to 770 MB/s) | A division per value; batch by segment, drop the per-value tag branch | 1 to 2 days |
| Small transformed blocks at ratio still try two plain codings when block sorting is close | img_chelsea.u8 (0.4 MB): 0.13 -> 0.25 s, same output | Tighter limit for the LZMA2 attempt | hours |
| Text-transform slice limit (25%) set from one file (nci) | unknown | More text files in the corpus before trusting it | corpus work |

| `ultra` larger than `ratio` on one CSV | energy_test.csv: 635,318 against 631,255 (0.6%); 1.0.1 has it too | Keep the `ratio` result when it is smaller (costs a `ratio` pass per block) | hours |
| Headerless RGB images are not recognised | img_chelsea.u8: no row length found, so no image graph and no image model; the `.npy` form of the same picture is 11% smaller | Row-length search for interleaved channels | 1 to 2 days |
| No image model for 16-bit images | mr, x-ray: 7% and 1% behind paq8px -8 | 16-bit variant of the model | 2 to 3 days |
## Accepted: worth the cost

| Cost | What it buys | Why accepted |
|---|---|---|
| Decimal float64 files: encode 1.7x to 3x, decode 1.2x to 1.9x at balanced | 35 to 57% smaller (26 holdout files: 27.4 -> 13.5 MB) | xz -9 needs 18.9 MB for the same files |
| Silesia nci at ratio: decode 120 -> 67 MB/s, encode +1.0 s | 20% smaller (1,533,927 -> 1,225,386) | Ratio mode is about size; the block sorter was already accepted from a 3% gain |
| lfw_subset.npy, skl_digits at ratio: decode about 2x | 16 to 19% smaller | Same rule |
| Marked plans at balanced: both codings per block (int64 pickle: encode 13 -> 40 ms, decode 6 -> 8 ms) | 24% smaller than 1.0.1 on that file; 8.5% on a mixed pickle | Only files the probes mark |
| Mixed-content files at ratio: both codings per block | neg_boost.npy 26% smaller, mixed table 19% smaller | Only files the probes mark; others pay four 32 KiB probes |
| mr, osdb at ratio: decode 1.8x to 2x against 1.0.1 (already on main) | 7% and 20% smaller | Block sorting; accepted when it went to main |
| wide_test.csv at balanced: decode 2.4x against 1.0.1 (already on main) | 43% smaller | Wide-table graph; accepted when it went to main |
| `ultra` on 8-bit images: 0.5 s -> 5 s for a 0.8 MB photograph (the graph was coded with LZMA2 leaves, the image model runs the mixer) | 12 to 20% smaller; within 2.5 to 5% of paq8px -8, from 17 to 31% | `ultra` is the smallest-output setting and runs the mixer on everything else |
| `ultra` on text: model 3 is about 35% slower than model 2 (JSON 27 -> 34 s, a 10 MB novel 66 -> 91 s on the VM) and needs about 0.8 GB more memory plus 4 bytes per input byte (5.3 GB against 4.5 GB at the largest table size) | 0.3 to 6.7% smaller on text (JSON 6.7%, CSV 5.4%, XML and logs 4.7%) | `ultra` is the smallest-output setting; binary blocks keep model 2, its speed and its memory |
| Decimal float32 files: the same encode and decode costs as the float64 form | 3 to 58% smaller (four tables: 5.63 -> 2.59 MB at balanced) | Same trade as float64 |
| `ratio` on 8-bit images: the image graph is coded twice (LZMA2 and block-sorted leaves) | 5 to 6% smaller on colour photographs | Small blocks; time not yet measured per file |
| Files using the new graph node or the fixed-width transform need this version | the gains above | Older readers stop with a clear error; no silent misread |
| Speed mode skips the table transform | CSV-like files up to 4x larger than balanced | Measured: the transform costs 2.3x encode and decode and would lose the win over lz4 -1 |

## Decisions to revisit at the end

Each is a choice that could reasonably go the other way. The number is what the choice costs or buys today.

| # | Decision | Taken | The other way | What would change it |
|---|---|---|---|---|
| 1 | Ratio mode may switch a block to block sorting for 3% | Yes (nci decode 2.4x slower for 20% smaller) | Cap the decode slowdown, or require 10% | A decode-speed target for ratio mode (xz -9 and 7-Zip decode faster than kolmo ratio on the M5: 124 against 127 and 182 MB/s) |
| 2 | DEC64 wins a plan by 7% or more | Yes | 0% (4% smaller on mixed tables, decode 1.8x) or 15% | Faster DEC64 decode would justify a lower bar |
| 3 | DEC64 costs 1.7x to 3x encode at balanced | Accepted | Use it only at ratio; balanced stays at main's speed and size | If balanced must hold its encode speed on every file type |
| 4 | Marked plans are coded both ways | Yes | Drop a marked plan outright (cheaper, loses 24% on the int64 pickle) | The deterministic early stop in "Open" |
| 5 | Speed mode skips the table transform | Yes | Apply it (CSV up to 4x smaller, 2.3x slower, loses lz4 -1) | A table check cheap enough to keep the lz4 -1 win |
| 6 | Balanced quick analysis (level-1 scoring, fewer trials) | Yes (encode x1.11 against 1.0.1 on the M5) | 1.0.1's full analysis | More holdout files: the one regression it caused was outside the corpus |
| 7 | New node ids and the fixed-width transform need a new reader | Yes | Gate them behind a flag until 1.1.0 is tagged | Release policy |
| 8 | Text-transform slice limit of 25% at ratio | Set from nci alone | 10% like other transforms (loses nci's 20%) | More text files |
| 9 | Integer probe requires values below 2^32 | Yes | Wider (finds timestamps; misreads short-mantissa doubles, 0.8% larger on two pickles) | A better discriminator |
| 10 | 3% minimum for a plain result at ratio, 10% probe margin, 1.35x slice limit | Chosen by judgement, not tuned | - | A sweep on a larger holdout |
| 11 | `ultra` tries plain context mixing only when plain block sorting is within 2% | Yes | Always try (magika.onnx 1.1% smaller, 20x the time on every model file) | If `ultra` time stops mattering |
| 14 | DEC64 blocks at balanced stay at the normal block size (up to 4 MiB) | Yes | One block of up to 32 MiB: 2.6 to 9.3% smaller on four files, decode CPU about 1.8x, no parallel decode of the file | If size matters more than decode speed at balanced for these files; ratio mode already uses large blocks |
| 13 | Model 3 only for text blocks | Yes | Everywhere (database file 2.2% smaller, executables 1%; three binary files 0.02 to 0.06% larger than 1.0.1, 20% slower) or code both and keep the smaller (2.2x the time) | If the no-file-larger rule is relaxed to a tolerance at `ultra` |
| 12 | Image model is a new block codec (17; 15 and 16 are left for the native leaf of `feat/native-residual-leaf`) | Yes | Fold it into codec 7 behind a stream flag | Release policy, with decision 7 |
| 15 | Budget rule (12 November): a mode that is over its competitor's cost today keeps its output; the work is to make it cheaper | Yes | Apply the rule strictly now: `balanced` would give back 3.4 points of size against zstd -3 on the M5 (0.843 to 0.877) and files would get larger than with 1.0.1 | If being within the competitor's cost on every file matters more than targets 3 and 4 |
| 16 | Archive lifetime for the rule | Both kept at the user's request (13 Nov): 30 days and one year are reported side by side until the end | One year: `ratio` is within OpenZL's cost on 24 of 32 files instead of 3 | What the archive modes are for: at 30 days the rule says the archive default should be near `hc -l 9` |
| 17 | Balanced: a typed plan that saves a fifth skips the record search | Yes (planning -29%; mixed_price_qty_ratio.npy 3.7% smaller and 23% faster to encode with the decimal plan; its decode goes from 15 to 28 ms on 9.6 MB) | Always run the search | A file where the record layout beats a typed plan that already saves a fifth: none in 83 |
| 18 | Balanced: a leaf whose three samples put Huffman a tenth behind zstd does not try Huffman | Yes (with the four-table count: decimal arrays encode 36% faster; no output changed on 83 files) | Always try both | A leaf where Huffman wins the whole although it loses the samples by a tenth; the sparse-leaf case found on 12 November went the other way and has its own guard |
| 19 | Fast settings, marked plan: the plain pass over a graph block runs only when three 64 KiB plain slices come within 35% of the graph's rate | Yes (31 MB decimal table 185 -> 109 ms; no output changed on 83 files at balanced, balanced -q, hc -l 9, speed) | Always code both | A block where plain coding wins the whole although its start, middle and end are each 35% behind the graph |
| 20 | Balanced: the integer trial stops a width when four of sixteen sample runs put it a quarter above plain coding | Yes (no output changed on 83 files) | Always run all sixteen | An integer array whose first, 5th, 10th and last sample runs are unlike the rest |
| 21 | `ratio`: decimal array planes take block sorting, not LZMA2 (budget rule). Both kept at the user's request (13 Nov): `ratio -l 10` codes them with LZMA2 as before | Yes: the table goes from 3.3x OpenZL's cost to 0.8x and stays 23% smaller than OpenZL. Decimal files are 2 to 12.6% larger at `ratio` than at ac6f88b (owid_energy.pkl +12.6%, co2.npy +9.7%, telemetry.npy +2.1%; four small ones 1 to 2% smaller), encode 25 to 30 times faster | LZMA2 planes: 10 to 12% smaller, 8 s per 24 MB | Data kept three years or more (LZMA2 saves 18 KB per CPU second; break-even is 652 KB at 30 days, 54 at one year, 18 at three years), or `ratio` meaning "smallest without context mixing" whatever it costs |
| 22 | `ratio`: float array planes take block sorting | Yes (same size on 6 of 7 files, magika.onnx +0.13% with decode 0.03 -> 0.24 s; encode four to six times faster) | LZMA2 planes | A float file where LZMA2 planes are clearly smaller: none in the set |
| 23 | `hc`: float planes at zstd 9 instead of 19 | Yes: float files keep their size (magika.onnx +0.16%) and encode up to 16 times faster (10 MB safetensors 5.5 -> 0.3 s). Decimal planes keep zstd 19: zstd 12 was measured and rejected (3 to 5% larger on big tables, 12 to 14% on small ones, 36% on telemetry.pkl) | zstd 19 float planes | A float file where zstd 19 planes are clearly smaller: none in the set |
| 24 | Balanced: an 8-bit image layout that saves a fifth ends the float, integer and decimal trials | Yes (0.8 MB photograph: 41% fewer instructions; no output changed on 83 files) | Always run every typed trial | Data that passes the image pre-check and saves a fifth as an image, but is smaller still as integers or decimals: none in 83 |
| 25 | DEC64 dictionary columns: at `balanced` only a dictionary that saves a byte plane and wins by order-0 size; the both-ways trial of one-byte dictionaries runs at `hc` and `ratio` only | Yes: the trial doubled the encode time of a 0.6 MB table at `balanced` for 1 to 2%, and large decimal tables that gain nothing pay 2% more instructions for the candidate test | Trial at `balanced` too (ozl -1.3 to -1.9%, skl_digits -0.8%, one pickle +0.6%) | A cheaper way to see which columns keep their repeats as an index (forcing shows up to -10% on one file) |

### What `ratio` pays for, measured (13 November 2026)

Each expensive choice in `ratio` against its cheaper alternative, over 64 files (`/tmp/cmp/var.py`, CPU time, one
compression and 3.3 decompressions). The cost model breaks even at 652 KB saved per CPU second for data kept 30 days
and 54 KB for one year.

| Choice | Size it buys | CPU it costs | KB per CPU second | Decision |
|---|---|---|---|---|
| LZMA2 planes for decimal arrays (against block sorting) | 1.28% of the set, 10 to 12% of those files | 72 s of 208 | 17.9 | Dropped (decision 21) |
| LZMA2 planes for float arrays | none (one file 0.13%) | 10 s | below 1 | Dropped (decision 22) |
| LZMA2 extreme preset (against preset 9) | 0.46% of the set; samba 0.64%, CSV 1.3 to 1.8%; mozilla is 0.13% larger with it | 75 s of 208 | 6.2 | Kept: without it samba, x-ray and the CSV files would be larger than with 1.0.1 (target 4). Revisit at the end |

### The decisions above under the budget rule (12 November 2026)

| Decision | Under the rule |
|---|---|
| 2: DEC64 must win by 7% (decode is slower) | The margin guards the wrong cost. On the M5 the decimal files decode at 1.01x to 1.02x zstd -3's time and encode at 2.8x to 3.0x. The cost to fix is encode speed (tracker A13); once the planner has a cost table (B12) the margin goes |
| 3: DEC64 costs 1.7x to 3x encode at balanced | Over budget (x1.63 to x1.69 at 1 Gbit/s) for a file 41% smaller. Kept under decision 15; first item of tracker B14 |
| 14: 32 MiB blocks for DEC64 at balanced | Stays rejected: decode would go from 1.02x to about 1.8x zstd -3's time. Open for the archive modes, where it has to fit OpenZL's cost |
| 5: speed mode skips the table transform | Under the rule the natural speed-tier setting is `balanced -Q` (within zstd -3's cost on 35 of 35 files, 4% smaller). `speed` is over on 11 text files because its larger output costs more in transfer than it saves in CPU at 1 Gbit/s; it remains the setting for faster links and for the lz4 comparison |
| 1: ratio may switch a block to block sorting for 3% | Judged by OpenZL's cost: nci at `ratio` is x1.84 at 30 days and x0.61 at one year. The excess is the 5.3 s encode, not the slower decode (0.47 s against 0.09 s). Part of tracker B15 |

## M5, 13 November 2026, runs of ac6f88b, 4e103b0 and ec50fcd

All three: 17 of 19 rivals beaten (open: lz4 -9 on decode at x0.97, paq8px -8), no file larger than 1.0.1, no slowdown
flag on unchanged output. Against 1.0.1 at ec50fcd: `balanced` x1.131 ratio, x1.098 encode, x1.061 decode; `hc` x1.108
ratio, x1.477 encode; `ratio` x1.116 ratio, x1.460 encode, x0.999 decode. Budget rule, speed tier: `balanced` within
zstd -3's cost on 14 files (1079e90), 16 (ac6f88b), 18 of 35 (ec50fcd); the decimal arrays are within it now. Still
over: lfw_subset.npy x1.47, JSON number array x1.36, energy_test.csv x1.27, logs x1.25, nci x1.20, wide table x1.19.

## M5, 12 November 2026, runs of 18d7efb and 3c5f599

Both runs: 17 of 19 rivals beaten (open: lz4 -9 on decode, paq8px -8), every setting at or above 1.0.1 (`balanced`
x1.131 ratio, x1.016 encode, x1.053 decode at 3c5f599), no file larger. Slowdown flags on unchanged output: three at
18d7efb (`hc` mr encode x0.91; `ratio` ld30.txt and webster decode x0.92 and x0.91), none of them at 3c5f599, which has
one other (`ratio` chr22.fa encode x0.92). No flag repeats across the two runs: noise, not counted.

## M5, 12 November 2026 (commit 67a022f, before this branch)

17 of 19 rivals beaten, 7-Zip, bsc, lzfse and zpaq among them for the first time on the same machine. Open: lz4 -9
(decode 97% of the 98% needed) and paq8px -8. Against paq8px -8 on the 23 files of 11 MB or less: kolmo ultra
33.22 MB, paq8px 34.38 MB (3.4% smaller in total, 97 s against 9,139 s to encode), but paq8px is smaller on 17 of
the 23 files and 5% ahead on the per-file mean; the total is carried by tables and records (sao, CSV). paq8px's
largest leads: small images (17 to 52%), JSON (43%), executables (33%), logs and XML (25%). No setting slower than
1.0.1 on unchanged output, no file larger.

## M5, 12 November 2026, second run (commit 6fa4144: this branch up to the image model)

Same 17 of 19 rivals beaten; open: lz4 -9 (decode 96%) and paq8px -8. No unchanged output more than 5% slower than
1.0.1, no file larger, every round trip correct. Against 1.0.1 per setting (ratio, encode, decode): speed core x1.000
x1.055 x1.115; balanced x1.129 x1.031 x1.063; hc x1.107 x1.167 x1.045; ratio x1.122 x0.987 x1.127. Against paq8px -8 on
23 files: 33.05 MB against 34.38 MB (3.9% smaller in total; per-file mean now 3% behind, from 5%). The run of 486655e
before it showed four unchanged outputs 6 to 8% slower to decode and ratio encode at x0.949; the first did not repeat
(noise), the second was the ungated both-ways coding that 93d4866 fixed.

## Process note 3

Twice in this work an unstaged source edit was lost to `git checkout <file>` meant to drop a one-line experiment
(the DEC64 planner wiring on 11 November, the second-difference flag on 12 November). Both were re-applied from the
session and re-verified. Rule since: stage before any experiment, and drop experiments by editing them out.

## Process note 2

A context-mixing run hung for an hour on 12 November: the match chain was reserved from the memory budget after the
model's tables, and when the two together passed the budget the reservation waited for memory that only the same
thread could release. Caught on a 10 MB file before any commit; the reservation is now made once, for both.

## Process note

Two round-trip failures appeared in an `ultra` pass on 12 November (magika.safetensors, mr). Both were the kernel
killing the encoder for memory while a second context-mixing job ran beside it; each file round-trips when run alone.
`ultra` needs about 4.5 GB: one job at a time on the 2-core VM.

The same happened once more on 13 November (loghub_all.log at `ultra`, during the gate, while profiling jobs ran
beside it); alone it gives 185,557 bytes and round-trips. Nothing else may run during a gate.

## Not regressions: gaps to rivals that are still open

lz4 -9 decode (97% of the 98% needed, M5); zstd -1/-3 decode on Silesia (75 to 83%); zstd -3 round trip (85 to
93% at 12% smaller); xz -9 decode (45%, and nci now adds to it); snappy encode (91%); PAQ8PX size at ultra.
