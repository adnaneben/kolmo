# Research tracker

Every takeaway and planned action from the experimentation, with its status and outcome. The research phase is not
closed, and nothing is merged to main, until every row is **Done** or **Dropped** (dropped means: measured, and the
number that justifies dropping it is in the row). Decision of 12 November 2026.

Sources: S = session plan, G = the "Kolmo-inspired codec" plan (experiment ids E00 to E10), M = found by measurement.
Costs and trade-offs of what was done are in `benchmark/accept/REGRESSIONS.md`; this file tracks what was planned.

Status on 12 November 2026 (branch `claude/dec64`, e2bfeef): 60 rows. 14 done, 6 dropped, 9 partly done, 30 not done,
1 deferred (the merge).

## A. Numeric and structured data

| # | Action | Src | Status | Outcome / what is left |
|---|---|---|---|---|
| A1 | Decimal mapping for binary float64 as a graph node, with exceptions and bounds checks | S, G E03 | Done | 26 holdout files 27.4 -> 13.5 MB at balanced; M5 confirmed |
| A2 | Holdout set never tuned on | S, G E09 | Partly | 26 files built; two design changes were made after seeing it, and all tables come from 7 sources. Left: an independent holdout from other publishers |
| A3 | Arrays at any alignment inside containers (pickle, npz, mat, parquet) | M | Done | 12 container files 15.5 -> 7.8 MB at balanced |
| A4 | Integer columns inside containers | M | Done | int64 pickle 129,908 -> 98,236; mixed pickle -8.5% |
| A5 | Doubles interleaved with other fields (rows of structs) | M | Not done | Finder needs contiguous arrays |
| A6 | Integers of 2^32 and above inside containers (timestamps) | M | Not done | Probe limited to 2^32 to keep short-mantissa doubles out (ledger decision 9) |
| A7 | Decimal mapping for float32 | M | Done | Same node, 4-byte words (`.npy` tables and arrays inside containers). Four holdout tables as float32: balanced 5.63 -> 2.59 MB, ratio 3.79 -> 2.11 MB; per file -3% to -58% |
| A8 | Gorilla XOR and bit-plane coding for floats against the byte-plane node | G E03 | Dropped | Six float sets: XOR + planes is 1 to 16% larger than byte planes on tables, noise and weights; 7% smaller on a smooth series, where kolmo's own choice is already smaller still (5.50 against 5.57 MB; a sine: 2.91 against 4.99 MB). XOR bit planes within 1% of XOR byte planes |
| A9 | Residual packing for integers: frame of reference, bit-packing, patched, Rice, against byte planes | G E02 | Dropped | Five integer sets: the best of Rice, bit-packing and patched packing (ideal parameters per 128 values) is 14% to 10x larger than the integer graph at balanced (timestamps 13,106 against 2,628 bytes; decimals as integers 73,910 against 64,840). They are speed encodings, not size ones |
| A10 | Several small entropy contexts per leaf (byte position, sign, exception flag) | G E04 | Partly | Position, sign and exception flag are already separate leaves. Measured an order-1 leaf (previous byte of the same plane) on three tables' planes against the balanced coder: 2,284 against 2,182 KB (worse), 343 to 395 against 460 KB (14 to 25% smaller), 89 to 105 against 111 KB (5 to 20% smaller). Left: build an order-1 rANS leaf and let the serializer pick it per leaf |
| A11 | Cross-field prediction (one column from another) | G | Dropped | Difference from an earlier column: 1 of 144 columns qualifies, 0 bytes. Linear fit on any other column (an optimistic bound, either direction): 0.6%, 0%, 1.0% on three tables, 10.3% on a physics table. Reopen only for tables of physically related quantities |
| A12 | Segmentation: restart or switch models where the data changes | G E07 | Partly | Per-segment exponents and per-block both-ways coding exist; no general change detector |
| A13 | DEC64 speed: reuse the trial slice, code leaves once, batch the decode | M | Not done | Encode is 1.7x to 3x, decode 1.2x to 1.9x on those files |
| A14 | JSON and log structure parsing into streams | G E05 | Partly | Tables, templates and fixed-width lines exist; no JSON key/value split. See D8 for the ultra side |

## B. Planner

| # | Action | Src | Status | Outcome / what is left |
|---|---|---|---|---|
| B1 | Oracle against planner gap | S, G E08 | Partly | Plan against plain coding: 0 of 81 files lose by over 1% at balanced and ratio (was 2 and 2). Left: plan against every other plan |
| B2 | Probes away from the trial slice | M | Done | Fixed the int64 pickle regression (+69% on main) |
| B3 | Marked plans: stop coding both ways once the plan keeps losing | M | Not done | Needs a deterministic order between block jobs |
| B4 | Probes that see long-range repeats | M | Not done | 32 KiB probes said the integer graph wins where whole blocks lost by 41% |
| B5 | Ratio: transformed block against plain | M | Done | Silesia nci -20%; ratio encode over 81 files 262 -> 228 s after gating |
| B6 | Ultra never larger than ratio | M | Partly | Fixed for images (404,896 -> 323,467; 276,549 -> 203,747). Left: one CSV 0.6% larger |
| B7 | Sweep the thresholds chosen by judgement | M | Not done | Ledger decision 10 |
| B8 | Check the 25% text slice limit on more text | M | Not done | Set from nci alone (ledger decision 8) |
| B9 | Planning time at balanced | S | Not done | About 130 ms on the Silesia tar; part of the zstd -3 round-trip gap |

## C. Engine: matching and fast paths

| # | Action | Src | Status | Outcome / what is left |
|---|---|---|---|---|
| C1 | Re-code zstd's literals and sequences with contexts | S | Dropped | Working coder: -3.0% at zstd's level-3 parse, +0.8% at level 2; adaptive version 8x slower to decode |
| C2 | Text: change how matches are found and described | S, G E06 | Not done | Text at zstd -3 is about 90% match descriptions |
| C3 | Own LZ4-class encoder | S | Not done | snappy encode 91%, lz4 -1 margins |
| C4 | LZ4-HC profile for write-once data | S | Not done | lz4 -9 decode at 96 to 98% of the 98% needed |
| C5 | Faster inverse transforms | S | Partly | Fixed-width decode done (67a022f, M5 confirmed). Left: 16-bit image inverse, checksum |
| C6 | LZMA-only ratio profile for decode-heavy use | S | Not done | Ratio decodes at 124 MB/s; xz -9 127, 7-Zip 182 (ledger decision 1) |

## D. Ultra against PAQ8PX

| # | Action | Src | Status | Outcome / what is left |
|---|---|---|---|---|
| D1 | PAQ8PX on the same machine | S | Done | -8 on 23 files: kolmo 33.05 MB, paq8px 34.38 MB; per-file mean 3% behind |
| D2 | 8-bit image model | M | Done | Grey 132,883 -> 116,664 (2.5% behind); RGB 404,896 -> 323,467 (4.8% behind) |
| D3 | 16-bit image model | M | Not done | Reusing the 8-bit model as two channels: x-ray 3,587,117 against 3,561,093 today (worse). mr 6.7% behind, x-ray 0.9% |
| D4 | Recognise headerless RGB images | M | Not done | img_chelsea.u8 gets no image coding; its .npy form is 11% smaller |
| D5 | Text model 3 (higher orders, expected byte) | M | Done | JSON -4.4%, prose -3.4%, logs -2.8%, XML -2.5%; about 20% slower |
| D6 | Match that carries on past an edit | M | Dropped | +0.2% to -0.9% alone; kept inside model 3 at no cost |
| D7 | Mixer and APM selected by the expected byte | M | Dropped | Within 0.1% on three files |
| D8 | JSON and XML structure contexts | M | Partly | Key contexts ("key":, key=, tag after <) added to model 3: api.json -1.1%, xml -0.9%. Still 33% and 19% behind. Left: nesting depth, array position, value type |
| D9 | Log field contexts | M | Partly | Field index and the byte above in the same field added to model 3: logs -2.1%, CSV -4.3%. Logs still 20% behind |
| D10 | Word and text model (stems, word position, more word orders) | M | Not done | reymont 16% behind, dickens 9% |
| D11 | Executable model | S | Not done | ooffice 33% behind; mozilla and samba carry most of the published gap |
| D12 | Two-layer mixing and a chain of APMs | M | Not done | Measured on api.json: 72% of the output is spent on the 7% of bytes where the match model's prediction is wrong (2.7 to 3.6 bits each). Paq8px must be about 30% cheaper on exactly those bytes |
| D20 | Choose the match by longest agreeing context among 16 candidates | M | Done | Offline it looked like half the misses (88% -> 94% right); in the coder, which already keeps a match until it breaks, misses fell 1.4%: api.json -0.8%, xml -1.1%, logs 0. Kept in model 3 |
| D13 | Re-compress embedded deflate streams | S | Not done | Scan: 3.2 MB of deflate data in mozilla, 0.4 MB in samba; about 1.5 to 2 MB recoverable |
| D14 | PAQ8PX with and without zlib on mozilla and samba (Mac) | S | Not done | Tells how much of its lead is deflate |
| D15 | Model 3 on binary blocks | M | Not done | -2.2% on a database file, but three files 0.02 to 0.06% larger than 1.0.1 (ledger decision 13) |
| D16 | Records and tables at ultra | M | Not done | osdb 11% behind |
| D17 | Image arrays inside other formats (stacks of images in .npy) | M | Not done | lfw_subset.npy 12% behind after the plain-coding fix |
| D18 | Tiny files | M | Not done | year.i32: 518 bytes against 248 |
| D19 | PAQ8PX at its top level (-12L) on the same machine | S | Not done | Published sizes only: 36.58 against 27.68 MB on Silesia |

## E. Measurement and process

| # | Action | Src | Status | Outcome / what is left |
|---|---|---|---|---|
| E1 | All rival tools on the Mac | S | Done | 19 rivals measured, bsc and paq8px included |
| E2 | Reuse rival results between Mac runs | M | Done | Six hours -> 27 minutes per commit |
| E3 | Regression ledger with decisions to revisit | S | Done | `benchmark/accept/REGRESSIONS.md`, 13 decisions; kept current |
| E4 | M5 confirmation of each batch | S | Done to 6fa4144 | 17 of 19 rivals, no slowdown, no file larger. e2bfeef pending |
| E5 | A quiet x86 Linux machine | S | Not done | Cloud VM CPU time moves by up to 25% between passes; needs hardware |
| E6 | Dashboard with the 19-rival M5 data and this branch | S | Not done | Shows 7c6d1c2 |
| E7 | Executive deck | S | Not done | "Changes" slide still says speed mode "tighten"; no DEC64, image model or PAQ8PX results |
| E8 | Revisit the 13 ledger decisions | S | Not done | At the end, by agreement |
| E9 | Final validation: freeze, full holdout, damaged-input tests, acceptance twice | G E09 | Not done | Last step before the merge |
| E10 | Merge to main | S | Deferred | Not before every row above is Done or Dropped. Main still has the int64 pickle regression |
| E11 | Tag and publish | S | Not done | From the Mac, after E10 |

## Considered and not planned

Neural plan selection and a new container format (both from G): the graph and the planner cover them; not started on
purpose. Reopen only if B1 shows a planner gap that rules cannot close.
