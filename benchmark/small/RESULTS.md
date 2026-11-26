# Small inputs, phase 1: results

Review 40, SYNTHESIS top-10 item 8 (claims #90, #92, #12 and #173). Main `a1d39559` against this branch, on
Apple M5. The method and data are in [README.md](README.md). The format-changing half (compact frame, small-block
entropy stage) is [docs/proposals/small-inputs.md](../../docs/proposals/small-inputs.md).

**Timings are pending.** Per-call compress and decompress times need a quiet machine. The runs made for this PR
were taken at a load average of 120-190 (other lanes): two sequential runs of identical code differed by 2-4x, so no
time from them is reported. Rows files: `rows/*-sizes.tsv` (sizes, heap, hashes; time columns blank). The timing
job is registered for the review workspace's quiet window (`evidence/small-inputs/`, `compare.sh` with main and
this branch interleaved per kind and size), and its tables will be added here. Profiles (sampled shares, not
timings) are quoted below only to explain the changes.

## What changed (encoder only; the format is unchanged)

Every frame is version 12, and 0.0.1 and 0.0.2 decode all of it. Checked here: 1,440 decodes of this branch's frames
of 0 B to 64 KiB, six kinds, levels 3, 4, 5, 8 and 9, with the 0.0.1 build and the 0.0.2 release, all byte-exact.

1. **Empty input: the 32-byte frame** (the header with nblocks 0, as FORMAT.md §4 says), not 52 bytes with a stored
   empty block.
2. **Hash tables sized to the block** (#92, #12). Each table keeps at least four slots per position and at least 2^12
   entries; blocks above 2^(hlog-3) bytes are unchanged.
   - `ko_parse` at levels 5-7 used to fill 1-4 MiB per call whatever the input. Filling it was 17% of a level 7 call
     on a 4 KiB log in a profile, and 89% on a 110-byte line (R21).
   - The lazy parse at level 4 used 1 MiB of head table and a 16 MiB chain. Its chain is now sized to the block
     exactly, which gives the same matches.
   - The fast parse used by KLZH2 levels 1-2 and the planner's size probe took 384-768 KiB per call.
   - Bytes changed in 38 of the 648 Kolmo settings below, by at most 0.009% (all at levels 4-5).
3. **Text planning: field counts.** `tp_fieldcounts` runs for five delimiters on every text input of 4 KiB or more.
   It now counts a line that has no quotes or brackets in one vectorisable pass. The counts are identical (checked on
   2,000,090 cases), so the bytes are identical. It was 42% of a level 3 call on a 16 KiB log chunk in a profile.
4. **Levels 8 and 9, plain blocks under 64 KiB.** These blocks went to BWT unconditionally: at level 8 every one, at
   level 9 text from 4 KiB up. They are now coded with KLZMA and with BWT, and the smaller is kept, KLZMA on a tie. No
   setting is larger than before.
   - Level 8 is 0.8-16% smaller in total at 64 B-16 KiB, and up to 33% smaller on one kind (CSV 1 KiB).
   - Level 9 is 5.0% smaller at 4 KiB and 1.4% at 16 KiB.
   - Where KLZMA is kept, decoding needs no QLFC model set-up. In a profile that set-up was about 400 µs per BWT block
     whatever its size.
   - Compressing runs both coders, so levels 8-9 compress more slowly on these blocks; how much waits for the quiet
     window.

The segment arrays of `compress_impl` (80 KiB per call) were also sized to the input in this branch. That change was
dropped, because the Codex lane makes the same change (`plan_segment_capacity`, PRs #103-#110).

## Not changed, and why

- **A per-thread workspace** (#12 item 5). After the tables are sized to the block, the remaining allocations of a
  small call grow with n (the sequences and stream buffers of KLZH2). The per-call heap at levels 4-7 at 4 KiB and
  below went from 1.3-17.5 MiB to 0.2-0.6 MiB. A workspace would also keep that memory alive between calls, so it is left
  until the quiet-window timings show what the remaining mallocs cost.
- **KLZMA's fixed tables** (two 2^16 hash tables and the encoder state, about 2.5 MiB per call at levels 8-9) and its
  decoder's 2.4 MiB. Their hash widths are part of the bytes it writes. This is a follow-up.
- **QLFC model set-up**: decoder side, and #111 (static QLFC at level 8) covers it.
- **KLZH2 table and stream choices within today's format.** Short code streams already fall back to stored chunks
  when a table costs more than it saves. The remaining loss needs a mode without a transmitted table, which is a
  format change (proposal P2).

## Tables (sizes and heap; one thread unless stated)

### Before and after, per level and input size (one thread, six kinds together)

Size: total output bytes, PR against main. Heap: the largest per-call heap working set over the six kinds, main -> PR.

| level | size | bytes main | bytes PR | change | compress heap KiB | decompress heap KiB |
|---|---|---|---|---|---|---|
| 1 | 64 B | 175617 | 175617 | +0.00% | 88 -> 88 | 0 -> 0 |
| 1 | 256 B | 387617 | 387617 | +0.00% | 88 -> 88 | 0 -> 0 |
| 1 | 1 KiB | 1025190 | 1025190 | +0.00% | 88 -> 88 | 0 -> 0 |
| 1 | 4 KiB | 3120054 | 3120054 | +0.00% | 480 -> 246 | 0 -> 0 |
| 1 | 16 KiB | 9956038 | 9956038 | +0.00% | 530 -> 402 | 0 -> 0 |
| 1 | 64 KiB | 8369172 | 8369172 | +0.00% | 688 -> 688 | 0 -> 0 |
| 2 | 64 B | 177086 | 177086 | +0.00% | 225 -> 225 | 0 -> 0 |
| 2 | 256 B | 409119 | 409119 | +0.00% | 226 -> 226 | 0 -> 0 |
| 2 | 1 KiB | 975536 | 975536 | +0.00% | 231 -> 231 | 1 -> 1 |
| 2 | 4 KiB | 2629431 | 2629431 | +0.00% | 480 -> 252 | 4 -> 4 |
| 2 | 16 KiB | 7864172 | 7864172 | +0.00% | 530 -> 402 | 16 -> 16 |
| 2 | 64 KiB | 6541308 | 6541308 | +0.00% | 688 -> 688 | 65 -> 65 |
| 3 | 64 B | 177070 | 177070 | +0.00% | 225 -> 225 | 0 -> 0 |
| 3 | 256 B | 407036 | 407036 | +0.00% | 227 -> 227 | 0 -> 0 |
| 3 | 1 KiB | 972885 | 972885 | +0.00% | 233 -> 233 | 1 -> 1 |
| 3 | 4 KiB | 2530834 | 2530834 | +0.00% | 510 -> 382 | 24 -> 24 |
| 3 | 16 KiB | 7416194 | 7416194 | +0.00% | 1152 -> 1152 | 49 -> 49 |
| 3 | 64 KiB | 6083250 | 6083250 | +0.00% | 2437 -> 2437 | 869 -> 869 |
| 4 | 64 B | 177059 | 177059 | +0.00% | 17489 -> 225 | 0 -> 0 |
| 4 | 256 B | 405618 | 405618 | +0.00% | 17490 -> 227 | 0 -> 0 |
| 4 | 1 KiB | 963916 | 963923 | +0.00% | 17493 -> 233 | 1 -> 1 |
| 4 | 4 KiB | 2486034 | 2486030 | -0.00% | 17513 -> 382 | 24 -> 24 |
| 4 | 16 KiB | 7180713 | 7180780 | +0.00% | 17797 -> 2150 | 49 -> 49 |
| 4 | 64 KiB | 5810353 | 5810353 | +0.00% | 42288 -> 34304 | 869 -> 869 |
| 5 | 64 B | 177057 | 177057 | +0.00% | 1266 -> 258 | 0 -> 0 |
| 5 | 256 B | 404451 | 404451 | +0.00% | 1268 -> 260 | 0 -> 0 |
| 5 | 1 KiB | 968424 | 968424 | +0.00% | 1278 -> 270 | 1 -> 1 |
| 5 | 4 KiB | 2440216 | 2440218 | +0.00% | 1354 -> 458 | 24 -> 24 |
| 5 | 16 KiB | 6979038 | 6979044 | +0.00% | 2150 -> 2150 | 49 -> 49 |
| 5 | 64 KiB | 5602870 | 5602870 | +0.00% | 26589 -> 34733 | 869 -> 869 |
| 6 | 64 B | 177060 | 177060 | +0.00% | 2354 -> 322 | 0 -> 0 |
| 6 | 256 B | 406293 | 406293 | +0.00% | 2356 -> 324 | 0 -> 0 |
| 6 | 1 KiB | 967886 | 967886 | +0.00% | 2366 -> 334 | 1 -> 1 |
| 6 | 4 KiB | 2414387 | 2414387 | +0.00% | 2442 -> 522 | 24 -> 24 |
| 6 | 16 KiB | 6886000 | 6886000 | +0.00% | 2930 -> 2150 | 49 -> 49 |
| 6 | 64 KiB | 5536765 | 5536765 | +0.00% | 27677 -> 34797 | 869 -> 869 |
| 7 | 64 B | 177060 | 177060 | +0.00% | 4431 -> 351 | 0 -> 0 |
| 7 | 256 B | 406294 | 406294 | +0.00% | 4434 -> 354 | 0 -> 0 |
| 7 | 1 KiB | 967602 | 967602 | +0.00% | 4443 -> 363 | 1 -> 1 |
| 7 | 4 KiB | 2411943 | 2411943 | +0.00% | 4519 -> 551 | 24 -> 24 |
| 7 | 16 KiB | 6877190 | 6877190 | +0.00% | 5007 -> 2150 | 49 -> 49 |
| 7 | 64 KiB | 5522746 | 5522746 | +0.00% | 29754 -> 34826 | 869 -> 869 |
| 8 | 64 B | 177622 | 176220 | -0.79% | 4177 -> 2561 | 4096 -> 12 |
| 8 | 256 B | 425692 | 355922 | -16.39% | 4178 -> 2562 | 4096 -> 2480 |
| 8 | 1 KiB | 993584 | 858241 | -13.62% | 4182 -> 2566 | 4096 -> 2480 |
| 8 | 4 KiB | 2494667 | 2264159 | -9.24% | 4206 -> 2590 | 4096 -> 2480 |
| 8 | 16 KiB | 6947786 | 6620303 | -4.71% | 4430 -> 2869 | 4096 -> 2480 |
| 8 | 64 KiB | 5166824 | 5166824 | +0.00% | 26336 -> 33792 | 4096 -> 2480 |
| 9 | 64 B | 173263 | 173263 | +0.00% | 4177 -> 2561 | 12 -> 12 |
| 9 | 256 B | 344652 | 344652 | +0.00% | 4178 -> 2562 | 4096 -> 2480 |
| 9 | 1 KiB | 807723 | 807723 | +0.00% | 4182 -> 2566 | 2480 -> 2480 |
| 9 | 4 KiB | 2303373 | 2188249 | -5.00% | 4200 -> 2584 | 4096 -> 2480 |
| 9 | 16 KiB | 6519765 | 6428912 | -1.39% | 4272 -> 2656 | 2480 -> 2480 |
| 9 | 64 KiB | 5123127 | 5123127 | +0.00% | 26336 -> 33792 | 4096 -> 2480 |

### Size change per kind (PR against main, one thread)

Only settings whose bytes changed; every other (kind, size, level) wrote identical bytes (same FNV-1a hash).

| kind | size | level | main | PR | change |
|---|---|---|---|---|---|
| json | 256 B | 8 | 74540 | 57886 | -22.342% |
| json | 1 KiB | 8 | 151676 | 129091 | -14.890% |
| json | 4 KiB | 4 | 338756 | 338758 | +0.001% |
| json | 4 KiB | 8 | 349186 | 303926 | -12.962% |
| json | 4 KiB | 9 | 349001 | 300646 | -13.855% |
| json | 16 KiB | 4 | 880991 | 880990 | -0.000% |
| json | 16 KiB | 5 | 868885 | 868887 | +0.000% |
| json | 16 KiB | 8 | 887096 | 827063 | -6.767% |
| json | 16 KiB | 9 | 887096 | 804367 | -9.326% |
| log | 256 B | 8 | 73446 | 53243 | -27.507% |
| log | 1 KiB | 8 | 125686 | 97907 | -22.102% |
| log | 4 KiB | 4 | 246114 | 246115 | +0.000% |
| log | 4 KiB | 5 | 245190 | 245192 | +0.001% |
| log | 4 KiB | 8 | 248870 | 219065 | -11.976% |
| log | 4 KiB | 9 | 248870 | 211633 | -14.962% |
| log | 16 KiB | 4 | 458232 | 458231 | -0.000% |
| csv | 256 B | 8 | 55506 | 41622 | -25.014% |
| csv | 1 KiB | 8 | 76394 | 51212 | -32.963% |
| csv | 4 KiB | 4 | 63533 | 63527 | -0.009% |
| csv | 4 KiB | 8 | 61836 | 44901 | -27.387% |
| csv | 4 KiB | 9 | 61836 | 43433 | -29.761% |
| kv | 1 KiB | 8 | 275446 | 270615 | -1.754% |
| kv | 4 KiB | 4 | 806918 | 806920 | +0.000% |
| kv | 4 KiB | 8 | 845950 | 732109 | -13.457% |
| kv | 16 KiB | 4 | 2394025 | 2394040 | +0.001% |
| kv | 16 KiB | 5 | 2356137 | 2356136 | -0.000% |
| kv | 16 KiB | 8 | 2447402 | 2209871 | -9.705% |
| code | 64 B | 8 | 29512 | 28879 | -2.145% |
| code | 256 B | 8 | 68130 | 54394 | -20.161% |
| code | 1 KiB | 4 | 148513 | 148512 | -0.001% |
| code | 1 KiB | 8 | 144884 | 129818 | -10.399% |
| code | 4 KiB | 4 | 402329 | 402326 | -0.001% |
| code | 4 KiB | 5 | 394854 | 394854 | +0.000% |
| code | 4 KiB | 8 | 387904 | 372892 | -3.870% |
| code | 4 KiB | 9 | 372890 | 361761 | -2.985% |
| code | 16 KiB | 4 | 1260591 | 1260617 | +0.002% |
| code | 16 KiB | 5 | 1228185 | 1228187 | +0.000% |
| code | 16 KiB | 8 | 1182970 | 1171725 | -0.951% |
| code | 16 KiB | 9 | 1152078 | 1143954 | -0.705% |
| binary | 64 B | 8 | 29674 | 28905 | -2.591% |
| binary | 256 B | 8 | 75222 | 69929 | -7.037% |
| binary | 1 KiB | 4 | 220351 | 220359 | +0.004% |
| binary | 1 KiB | 8 | 219498 | 179598 | -18.178% |
| binary | 4 KiB | 4 | 628384 | 628384 | +0.000% |
| binary | 4 KiB | 8 | 600921 | 591266 | -1.607% |
| binary | 16 KiB | 4 | 2151637 | 2151665 | +0.001% |
| binary | 16 KiB | 5 | 2043550 | 2043553 | +0.000% |
| binary | 16 KiB | 8 | 1951812 | 1933138 | -0.957% |

276 settings identical, 48 changed.

### One and ten threads (PR)

| level | settings | same bytes at -T1 and -T10 | compress time T10/T1 (geo. mean) | decompress time T10/T1 |
|---|---|---|---|---|
| 1 | 36 | 36 | pending | pending |
| 2 | 36 | 36 | pending | pending |
| 3 | 36 | 36 | pending | pending |
| 4 | 36 | 36 | pending | pending |
| 5 | 36 | 36 | pending | pending |
| 6 | 36 | 36 | pending | pending |
| 7 | 36 | 36 | pending | pending |
| 8 | 36 | 36 | pending | pending |
| 9 | 36 | 36 | pending | pending |

### Ratio against zstd, Brotli and LZ4 (PR, one thread)

Input bytes / output bytes over all items of the kind and size (higher is better). Kolmo frames carry 52 bytes of container per item (32 header, 16 index, 4 plan), zstd 9-13 (content size, no checksum), the LZ4 frame 7-15 + 4, Brotli none.

| kind | size | K1 | K2 | K3 | K5 | K7 | K9 | lz4 1 | zstd 1 | zstd 3 | zstd 9 | zstd 19 | brotli 5 | brotli 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| json | 64 B | 0.552 | 0.552 | 0.552 | 0.552 | 0.552 | 0.557 | 0.819 | 0.877 | 0.877 | 0.877 | 0.884 | 1.004 | 0.956 |
| json | 256 B | 0.953 | 0.887 | 0.897 | 0.900 | 0.889 | 1.128 | 1.145 | 1.414 | 1.415 | 1.418 | 1.450 | 1.621 | 1.588 |
| json | 1 KiB | 1.526 | 1.733 | 1.730 | 1.700 | 1.651 | 2.028 | 1.678 | 2.223 | 2.245 | 2.268 | 2.266 | 2.476 | 2.616 |
| json | 4 KiB | 2.353 | 2.990 | 3.021 | 3.100 | 3.115 | 3.474 | 2.478 | 3.441 | 3.485 | 3.599 | 3.625 | 3.834 | 4.176 |
| json | 16 KiB | 3.261 | 4.484 | 4.577 | 4.808 | 4.865 | 5.194 | 3.336 | 4.766 | 4.856 | 5.101 | 5.189 | 5.331 | 5.924 |
| json | 64 KiB | 4.124 | 5.797 | 5.970 | 6.379 | 6.475 | 6.687 | 4.096 | 5.925 | 6.117 | 6.482 | 6.697 | 6.693 | 7.619 |
| log | 64 B | 0.552 | 0.552 | 0.552 | 0.552 | 0.552 | 0.552 | 0.810 | 0.877 | 0.877 | 0.877 | 0.893 | 0.989 | 0.957 |
| log | 256 B | 1.123 | 1.038 | 1.040 | 1.042 | 1.039 | 1.226 | 1.329 | 1.600 | 1.603 | 1.604 | 1.642 | 1.794 | 1.759 |
| log | 1 KiB | 2.107 | 2.171 | 2.171 | 2.210 | 2.186 | 2.680 | 2.307 | 2.973 | 3.001 | 3.074 | 3.092 | 3.369 | 3.443 |
| log | 4 KiB | 3.225 | 3.982 | 4.038 | 4.260 | 4.271 | 4.935 | 3.384 | 4.734 | 4.782 | 5.169 | 5.178 | 5.453 | 5.795 |
| log | 16 KiB | 4.317 | 6.078 | 6.396 | 7.065 | 7.258 | 8.128 | 4.412 | 6.632 | 6.777 | 7.683 | 7.830 | 7.761 | 8.587 |
| log | 64 KiB | 5.163 | 7.573 | 8.173 | 9.591 | 10.159 | 11.702 | 5.186 | 7.976 | 8.258 | 9.705 | 10.390 | 9.592 | 11.139 |
| csv | 64 B | 0.552 | 0.552 | 0.552 | 0.552 | 0.552 | 0.573 | 0.812 | 0.961 | 0.877 | 0.877 | 0.986 | 1.117 | 1.048 |
| csv | 256 B | 1.333 | 1.213 | 1.219 | 1.251 | 1.240 | 1.568 | 1.638 | 2.093 | 2.043 | 2.067 | 2.108 | 2.288 | 2.325 |
| csv | 1 KiB | 3.907 | 3.655 | 3.692 | 3.851 | 3.864 | 5.120 | 4.146 | 6.200 | 6.065 | 6.304 | 6.449 | 6.661 | 6.980 |
| csv | 4 KiB | 7.941 | 8.610 | 8.893 | 9.437 | 9.532 | 13.580 | 7.424 | 13.857 | 13.274 | 14.973 | 15.675 | 15.433 | 17.152 |
| csv | 16 KiB | 11.466 | 16.404 | 16.141 | 17.004 | 17.283 | 21.706 | 10.694 | 22.078 | 19.930 | 24.423 | 27.003 | 24.176 | 29.154 |
| csv | 64 KiB | 13.291 | 21.441 | 28.879 | 32.183 | 32.885 | 38.950 | 12.675 | 26.111 | 22.810 | 29.831 | 35.021 | 28.940 | 36.470 |
| kv | 64 B | 0.553 | 0.552 | 0.552 | 0.552 | 0.552 | 0.552 | 0.815 | 0.877 | 0.877 | 0.877 | 0.877 | 0.948 | 0.945 |
| kv | 256 B | 0.845 | 0.832 | 0.832 | 0.832 | 0.832 | 0.888 | 0.967 | 0.978 | 0.984 | 0.986 | 1.003 | 1.055 | 1.012 |
| kv | 1 KiB | 1.002 | 1.010 | 1.009 | 1.004 | 1.007 | 1.176 | 1.054 | 1.163 | 1.171 | 1.179 | 1.197 | 1.210 | 1.256 |
| kv | 4 KiB | 1.134 | 1.266 | 1.290 | 1.321 | 1.333 | 1.435 | 1.174 | 1.366 | 1.379 | 1.393 | 1.425 | 1.407 | 1.486 |
| kv | 16 KiB | 1.440 | 1.694 | 1.701 | 1.780 | 1.791 | 1.907 | 1.479 | 1.765 | 1.772 | 1.811 | 1.842 | 1.829 | 1.948 |
| kv | 64 KiB | 1.940 | 2.333 | 2.280 | 2.448 | 2.437 | 2.649 | 1.924 | 2.309 | 2.408 | 2.474 | 2.489 | 2.506 | 2.679 |
| code | 64 B | 0.577 | 0.560 | 0.560 | 0.560 | 0.560 | 0.585 | 0.864 | 0.950 | 0.952 | 0.955 | 0.987 | 1.105 | 1.079 |
| code | 256 B | 1.050 | 0.998 | 1.001 | 1.006 | 1.002 | 1.206 | 1.258 | 1.530 | 1.538 | 1.551 | 1.587 | 1.734 | 1.777 |
| code | 1 KiB | 1.538 | 1.732 | 1.737 | 1.745 | 1.735 | 2.034 | 1.689 | 2.201 | 2.221 | 2.274 | 2.302 | 2.468 | 2.667 |
| code | 4 KiB | 1.934 | 2.479 | 2.536 | 2.645 | 2.658 | 2.887 | 2.058 | 2.805 | 2.854 | 2.944 | 3.004 | 3.112 | 3.433 |
| code | 16 KiB | 2.285 | 3.069 | 3.195 | 3.402 | 3.422 | 3.652 | 2.365 | 3.246 | 3.299 | 3.483 | 3.584 | 3.586 | 3.988 |
| code | 64 KiB | 2.642 | 3.514 | 3.717 | 4.023 | 4.064 | 4.399 | 2.635 | 3.583 | 3.678 | 3.985 | 4.139 | 4.037 | 4.503 |
| binary | 64 B | 0.565 | 0.556 | 0.556 | 0.556 | 0.556 | 0.579 | 0.844 | 0.919 | 0.923 | 0.924 | 0.928 | 0.993 | 0.984 |
| binary | 256 B | 0.907 | 0.878 | 0.886 | 0.894 | 0.893 | 1.024 | 1.066 | 1.149 | 1.156 | 1.168 | 1.212 | 1.213 | 1.170 |
| binary | 1 KiB | 1.130 | 1.177 | 1.186 | 1.202 | 1.239 | 1.463 | 1.224 | 1.425 | 1.446 | 1.467 | 1.538 | 1.508 | 1.563 |
| binary | 4 KiB | 1.289 | 1.499 | 1.650 | 1.728 | 1.776 | 1.942 | 1.325 | 1.612 | 1.661 | 1.689 | 1.791 | 1.724 | 1.823 |
| binary | 16 KiB | 1.332 | 1.652 | 1.907 | 2.053 | 2.112 | 2.248 | 1.410 | 1.737 | 1.799 | 1.847 | 1.973 | 1.886 | 2.032 |
| binary | 64 KiB | 1.403 | 1.738 | 2.060 | 2.244 | 2.304 | 2.467 | 1.467 | 1.727 | 1.834 | 1.956 | 2.090 | 2.005 | 2.182 |

### Time per call

Pending: the quiet-window run (README.md, `compare.sh`).


### Heap working set of one call, KiB (PR): compress / decompress, largest over the six kinds

| size | K1 | K2 | K3 | K5 | K7 | K9 | lz4 1 | zstd 1 | zstd 3 | zstd 9 | zstd 19 | brotli 5 | brotli 11 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 64 B | 88 / 0 | 225 / 0 | 225 / 0 | 258 / 0 | 351 / 0 | 2561 / 12 | 21 / 145 | 126 / 102 | 126 / 102 | 126 / 102 | 278 / 102 | 1350 / 35 | 760 / 52 |
| 256 B | 88 / 0 | 226 / 0 | 227 / 0 | 260 / 0 | 354 / 0 | 2562 / 2480 | 21 / 145 | 126 / 102 | 126 / 102 | 130 / 102 | 278 / 102 | 1353 / 35 | 765 / 64 |
| 1 KiB | 88 / 0 | 231 / 1 | 233 / 1 | 270 / 1 | 363 / 1 | 2566 / 2480 | 21 / 145 | 134 / 102 | 150 / 102 | 150 / 102 | 294 / 102 | 1372 / 40 | 913 / 58 |
| 4 KiB | 246 / 0 | 252 / 4 | 382 / 24 | 458 / 24 | 551 / 24 | 2584 / 2480 | 21 / 145 | 182 / 102 | 198 / 102 | 214 / 102 | 374 / 102 | 1439 / 44 | 1255 / 82 |
| 16 KiB | 402 / 0 | 402 / 16 | 1152 / 49 | 2150 / 49 | 2150 / 49 | 2656 / 2480 | 21 / 145 | 310 / 102 | 374 / 102 | 374 / 102 | 678 / 102 | 1669 / 63 | 1813 / 100 |
| 64 KiB | 688 / 0 | 688 / 65 | 2437 / 869 | 34733 / 869 | 34826 / 869 | 33792 / 2480 | 21 / 145 | 406 / 102 | 758 / 102 | 1014 / 102 | 1862 / 102 | 10682 / 137 | 5521 / 196 |

### Container share of Kolmo's output (PR, level 3, one thread)

frame_bytes / out_bytes; a one-block compact frame (docs/proposals/small-inputs.md) would replace 52 bytes per item by about 13.

| kind | 64 B | 256 B | 1 KiB | 4 KiB | 16 KiB | 64 KiB |
|---|---|---|---|---|---|---|
| json | 44.8% | 18.2% | 8.8% | 3.8% | 1.5% | 0.5% |
| log | 44.8% | 21.1% | 11.0% | 5.1% | 2.0% | 0.6% |
| csv | 44.8% | 24.8% | 18.7% | 11.3% | 5.1% | 2.3% |
| kv | 44.8% | 16.9% | 5.1% | 1.6% | 0.5% | 0.2% |
| code | 45.5% | 20.3% | 8.8% | 3.2% | 1.0% | 0.3% |
| binary | 45.2% | 18.0% | 6.0% | 2.1% | 0.6% | 0.2% |


## Caveats

- **Heap at 64 KiB, levels 4-9:** the largest per-call heap is JSON's, 26-35 MiB, from the planner's JSON token trial
  (`log_encode_m`). It varies between runs of the same code (26.5 and 34.7 MiB at level 5), so the main -> PR
  figures at 64 KiB are not a change made here; the bytes there are identical.
- Decoder heap figures of 0 are stored or KLZ blocks decoded straight into the caller's buffer.
- Level 9's 2,480 KiB decoder heap at 256 B and up is the KLZMA decoder's tables. Main writes KLZMA at those sizes
  too.

## Findings for later work

- CSV at 16 KiB: level 9 is 1.24x zstd -19, and level 3 is 1.2-1.7x zstd -3 at 256 B-16 KiB (the table-transform
  route on short inputs).
- Level 9 at 64 B-1 KiB already uses KLZMA (main): it is the best Kolmo level for small items, and with a compact
  frame it would be at parity with zstd -19 (proposal, section 2).
