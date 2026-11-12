# Regression ledger

What each change cost, measured against 1.0.1 and against main, and what was decided about it.
Last full pass: branch `claude/dec64`, 12 November 2026, cloud VM (2 cores, x86-64), one thread, CPU time
(user + system), 81 files / 860 MB: the test corpus plus two holdout sets that were built after the code they test.
Method: size, encode CPU and decode CPU per file for 1.0.1, main and the branch, best of three runs at speed and
balanced, single runs at ratio.
None of this has been confirmed on the M5 yet.

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

## Open, fixable

| Regression | Size of it | Fix | Effort |
|---|---|---|---|
| Marked plans code every block twice | int64 pickle: encode 13 -> 40 ms at balanced (now 24% smaller for it, see Accepted); a file where plain wins every block would pay the same for nothing | Stop trying the plan after the first blocks all lose; needs an order between block jobs to stay deterministic | 1 to 2 days |
| Short probes cannot see long-range repeats | The 32 KiB probes said the integer graph wins on the int64 pickle; whole blocks said plain wins by 41% | Probe with history (code the probe after its preceding 1 MiB), or score plans on two slices | 2 days |
| DEC64 encode cost | 1.7x to 3x encode CPU on decimal files at balanced (still 90 to 230 MB/s) | The trial codes a 1 MiB slice that the block coder codes again; reuse it, and code leaves once instead of trying each leaf codec | 2 days, est. 1.3x to 1.5x back |
| DEC64 decode cost | 1.2x to 1.9x decode CPU at balanced (still 420 to 770 MB/s) | A division per value; batch by segment, drop the per-value tag branch | 1 to 2 days |
| Small transformed blocks at ratio still try two plain codings when block sorting is close | img_chelsea.u8 (0.4 MB): 0.13 -> 0.25 s, same output | Tighter limit for the LZMA2 attempt | hours |
| Text-transform slice limit (25%) set from one file (nci) | unknown | More text files in the corpus before trusting it | corpus work |

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

## M5, 12 November 2026 (commit 67a022f, before this branch)

17 of 19 rivals beaten, 7-Zip, bsc, lzfse and zpaq among them for the first time on the same machine. Open: lz4 -9
(decode 97% of the 98% needed) and paq8px -8. Against paq8px -8 on the 23 files of 11 MB or less: kolmo ultra
33.22 MB, paq8px 34.38 MB (3.4% smaller in total, 97 s against 9,139 s to encode), but paq8px is smaller on 17 of
the 23 files and 5% ahead on the per-file mean; the total is carried by tables and records (sao, CSV). paq8px's
largest leads: small images (17 to 52%), JSON (43%), executables (33%), logs and XML (25%). No setting slower than
1.0.1 on unchanged output, no file larger.

## Not regressions: gaps to rivals that are still open

lz4 -9 decode (97% of the 98% needed, M5); zstd -1/-3 decode on Silesia (75 to 83%); zstd -3 round trip (85 to
93% at 12% smaller); xz -9 decode (45%, and nci now adds to it); snappy encode (91%); PAQ8PX size at ultra.
