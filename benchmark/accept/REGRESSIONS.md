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
| balanced | 172.3 MB | 166.6 MB | 144.7 MB | 5.5 / 4.3 / 5.3 s | 1.62 / 1.30 / 1.43 s |
| ratio | 128.5 MB | 121.9 MB | 112.3 MB | 262 / 262 / 228 s | 11.6 / 13.3 / 11.4 s |

No file is larger than 1.0.1 or than main at any of the three settings. On the 48 files whose output is unchanged
from main, encode CPU is within 3% and decode within 5% (single runs).

## Fixed

| Regression | Where it came from | Status |
|---|---|---|
| pandas pickle of int64 columns + strings 69% larger than 1.0.1 at balanced (219,892 vs 129,908) | balanced quick analysis, on main since 7c6d1c2 | Fixed on the branch (probes away from the trial slice). **Still on main until the branch is merged.** |
| ratio encode 47% slower overall (262 -> 384 s) | coding decimal and transformed blocks both ways | Fixed: a block-sorted slice decides whether the plain attempt is worth making. Now 228 s, faster than main. |
| 2 MB NDJSON at ratio: 6x encode time, same output | plain LZMA2 attempt on every small transformed block | Fixed by the same gate (0.20 s -> 0.22 s). |
| mixed decimal / computed table at balanced: 4% smaller for 1.8x decode time | DEC64 chosen on a thin margin | Fixed: DEC64 must beat the next plan by 7%. |
| digits table 15% larger at ratio | DEC64 scored with zstd, coded with LZMA2 | Fixed before it left the branch (both codings, smaller kept). |
| 16-bit model file 0.84% larger than 1.0.1 at balanced | 256 KiB planning slice | Fixed in 7c6d1c2 (1 MiB slice restored). |

## Open, fixable

| Regression | Size of it | Fix | Effort |
|---|---|---|---|
| Marked plans code every block twice even when plain wins every block | int64 pickle: encode 13 -> 41 ms at balanced, same output | Stop trying the plan after the first blocks all lose; needs shared state between block jobs | 1 day |
| DEC64 encode cost | 1.7x to 3x encode CPU on decimal files at balanced (still 90 to 230 MB/s) | The trial codes a 1 MiB slice that the block coder codes again; reuse it, and code leaves once instead of trying each leaf codec | 2 days, est. 1.3x to 1.5x back |
| DEC64 decode cost | 1.2x to 1.9x decode CPU at balanced (still 420 to 770 MB/s) | A division per value; batch by segment, drop the per-value tag branch | 1 to 2 days |
| Small transformed blocks at ratio still try two plain codings when block sorting is close | img_chelsea.u8 (0.4 MB): 0.13 -> 0.25 s, same output | Tighter limit for the LZMA2 attempt | hours |
| Integer arrays inside containers are not found | int64 pickle: no gain where the float form of the same table is 40% smaller | Extend the array finder to integers | 2 to 3 days |
| Text-transform slice limit (25%) set from one file (nci) | unknown | More text files in the corpus before trusting it | corpus work |

## Accepted: worth the cost

| Cost | What it buys | Why accepted |
|---|---|---|
| Decimal float64 files: encode 1.7x to 3x, decode 1.2x to 1.9x at balanced | 35 to 57% smaller (26 holdout files: 27.4 -> 13.5 MB) | xz -9 needs 18.9 MB for the same files |
| Silesia nci at ratio: decode 120 -> 67 MB/s, encode +1.0 s | 20% smaller (1,533,927 -> 1,225,386) | Ratio mode is about size; the block sorter was already accepted from a 3% gain |
| lfw_subset.npy, skl_digits at ratio: decode about 2x | 16 to 19% smaller | Same rule |
| Mixed-content files at ratio: both codings per block | neg_boost.npy 26% smaller, mixed table 19% smaller | Only files the probes mark; others pay four 32 KiB probes |
| mr, osdb at ratio: decode 1.8x to 2x against 1.0.1 (already on main) | 7% and 20% smaller | Block sorting; accepted when it went to main |
| wide_test.csv at balanced: decode 2.4x against 1.0.1 (already on main) | 43% smaller | Wide-table graph; accepted when it went to main |
| Files using the new graph node or the fixed-width transform need this version | the gains above | Older readers stop with a clear error; no silent misread |
| Speed mode skips the table transform | CSV-like files up to 4x larger than balanced | Measured: the transform costs 2.3x encode and decode and would lose the win over lz4 -1 |

## Not regressions: gaps to rivals that are still open

lz4 -9 decode (97% of the 98% needed, M5); zstd -1/-3 decode on Silesia (75 to 83%); zstd -3 round trip (85 to
93% at 12% smaller); xz -9 decode (45%, and nci now adds to it); snappy encode (91%); PAQ8PX size at ultra.
