# kbin: a numeric-array coder (frame KBN1)

`build/kbin` is a standalone research tool for raw little-endian arrays and `.npy` files. It is not
used by the Kolmo library or the `kolmo` binary and does not change the Kolmo format. It re-implements
the method published for pcodec (Loncaric et al., arXiv 2502.06112) from the paper, without pcodec's
code, and adds two things of ours: an integer linear predictor and an optional context extension for
bin indices (`-C`). Provenance: `PROVENANCE.md`.

```sh
make ARCH= build/kbin build/kbin-asan
./build/kbin -t i16 in.i16 out.kbn          # raw array: the type is required
./build/kbin data.npy out.kbn               # .npy: type and shape from the header
./build/kbin -d out.kbn restored            # byte-exact
./build/kbin -b -r 5 -T 10 -C data.npy      # bench: JSON with size and the fastest of 5 runs
./build/kbin-asan --selftest                # round trips and corrupted frames under ASan and UBSan
```

Options: `-T n` threads (chunks code in parallel, encode and decode); `-l 1..12` level (log2 of the
starting bin count, default 8 as pcodec); `-c n` chunk length 2^n values (default 18); `-s n` interleaved
channels of a raw array (default: detected from the data); `-C` context extension; `-n` no linear
predictor (pcodec's delta set only); `-F`, `-M`, `-L` switch off the float divisor mode, the integer
multiplier mode and the 2-D `.npy` layout.

## Method

1. **Order-preserving ordinals.** Each value becomes an unsigned integer of its width that keeps its
   order: unsigned as is; signed with the sign bit flipped; floats with all bits flipped when negative
   and the sign bit set when positive. NaN payloads, infinities and -0.0 are just ordinals.
2. **Layout and chunks.** A C-order 2-D `.npy` with at most 16 columns is coded column by column
   (`ecg_2ch`); a Fortran-order 2-D `.npy` is coded per contiguous column (`owid_energy`, 128 columns).
   For raw arrays an interleave of 1 to 8 channels is detected by comparing first-difference costs on a
   sample (it finds the 2 channels of `mitdb-100.i16`). Each column is cut into chunks of 2^18 values;
   chunks are independent and code in parallel with a pthread pool.
3. **Mode per chunk** (one or two latent streams):
   * classic: the ordinals;
   * integer multiplier: when the gcd `m` of all differences is at least 2, `q = o / m` and `r = o mod m`
     (`r` is then constant and costs a few bytes; timestamps in ms that are whole seconds);
   * float divisor (our form of the paper's float multiplier): `k = round(x * D)`, with `D` the smallest
     power of ten for which 90% of a sample is exact, else the smallest integer up to 4096 for which
     90% are within float32 rounding of a multiple; streams `k` and the ordinal adjustment
     `ord(x) - ord(k / D)`, where `k / D` is one IEEE division. Decimal data parsed from text gives an
     adjustment of 0; NaN and infinities reuse the previous `k`, so runs of them cost almost nothing;
   * float quantisation: `hi = round(o / 2^q)` and the signed remainder, for `q` from the trailing-zero
     count and 29, 26, 20 (f64) or 13, 10 (f32). Rounding instead of the paper's floor keeps values just
     below a float32 boundary small (`lfw_subset`, chosen `q` = 26).
   The mode is chosen by a cost proxy (entropy of sign and bit length plus the bits under the leading
   one) on each candidate's streams.
4. **Delta per latent stream**, chosen on a sample of 8 slices of 2048 values: consecutive differences of
   order 0 to 7 (stopping after two orders that do not improve); lookback (as in pcodec: each value is
   coded against one earlier value at an explicit lag within a window of 2^8, 2^12 or 2^15; candidates
   are the previous lag, the most recent exact match and the most recent values sharing a bit prefix of
   three lengths, and lag 1; the smallest residual wins); and our integer linear predictor of order up
   to 8 (Levinson-Durbin on a sample, coefficients quantised to 16 bits, prediction in wrapping 32-bit
   arithmetic, so the integer path stays exact and encoder and decoder agree bit for bit). A predictor
   or lookback must beat the best consecutive order by 1.5% or 3%, since it decodes more slowly.
   Residuals are centred by adding 2^63.
5. **Binning** (the paper's core). The residuals are sorted (11-bit radix sort) and cut into 2^level
   quantile groups; a run of equal values that crosses a group boundary becomes its own group. A dynamic
   program over consecutive groups picks the bins that minimise
   `count * offset_bits + count * log2(n / count) + per-bin header`, with
   `offset_bits = bit length of (high - lower)`. Each value is then its bin index plus
   `value - lower` written raw in `offset_bits` bits.
6. **Bin indices: static rANS, our own.** 32-bit state, 16-bit renormalisation, table of 2^S slots
   (S up to 12, more only when there are many bins), weights quantised per chunk. Streams of 65,536 or
   more symbols use 4 interleaved states, each with its own word stream, so the lanes decode
   independently; shorter streams use one state to save header bytes. The encoder uses an exact
   reciprocal (`mulhi(x, ceil(2^64 / f))`) instead of a division.
7. **Context extension (`-C`, not in the paper).** The previous bin indices of the same lane (the
   symbol 4 positions earlier in long streams, 1 in short ones) are clustered greedily into up to 8
   contexts, merging the pair that costs least until the next merge costs more than a weight table
   (streams with at most 64 bins and at least 2048 values); each context has its own static table.
   Using the lane's own previous symbol keeps the 4 lanes independent: with the immediately previous
   symbol, sizes were 0.5% smaller overall but decoding was 2 to 3 times slower.

Decoding is integer arithmetic only, except one IEEE division per value in the float divisor mode
(built with `-ffp-contract=off`). The decoder rejects frames that are inconsistent (weight sums, lane
lengths, final rANS states, offset bit counts, lags, sizes) and never reads outside its input.

## Frame layout (KBN1)

All integers are LEB128 varints unless a width is given; fixed-width fields are little endian.

```
frame    := "KBN1" u8:version=1 u8:type header nvalues ncols u8:layout chunk_log tail_len tail[tail_len]
            size[njobs] chunk[njobs]
type     := 0 i16 | 1 u16 | 2 i32 | 3 u32 | 4 i64 | 5 u64 | 6 f32 | 7 f64
header   := u8:0                                     raw input
          | u8:1 len bytes[len]                      .npy header, verbatim
          | u8:2 u8:align64 u8:fortran u8:ndim dim[ndim]   .npy header exactly as numpy writes it
                                                     (regenerated byte for byte; align 16 or 64)
layout   := 0: value (row r, column c) is element r*ncols + c;  1: element c*nrows + r
njobs    := ncols * ceil(nrows / 2^chunk_log), column-major; nrows = nvalues / ncols
chunk    := u8:mode params latent [latent]
mode     := 0 classic (1 latent) | 1 integer multiplier, varint m (2) | 2 float divisor, varint D (2)
          | 3 float quantisation, u8 q (2)
latent   := u8:delta [params] warm[w] [residuals(n-1) of lag-1] residuals(n-w)
delta    := 0..7 consecutive order k (w = k) | 8 linear predictor: u8 p (1..8) u8 shift zigzag coef[p]
            (w = p) | 9 lookback: u8 window_log (w = 1, lag stream first)
residuals(n) := nbins (lower_delta u8:offset_bits)[nbins]
            [if nbins > 1: u8:S u8:C cmap[nbins] if C > 1, weights[C][nbins] (C = 1: weight - 1),
             total_words lane_words[L-1] u32:state[L] u16:words...]  offset_bytes offset_bits[...]
```

Lanes: `L = 4` when `n >= 65536`, else 1; symbol `i` belongs to lane `i mod L`; its table is
`cmap[bin of symbol i - L]` (context 0 for the first `L`). Offsets are packed LSB first. The input's
bytes after the last whole value (or after the `.npy` data) are the tail, stored verbatim.

## Results

Apple M5, 10 cores, macOS, 16 November 2026. **All timings are provisional**: the machine ran other
heavy jobs throughout (load average 14 to 16), and rerunning the same command moved speeds by 20 to
50%. Sizes are exact. kbin and pcodec 1.0.4 (Python, `standalone.simple_compress` with
`ChunkConfig(compression_level=8 or 12)`, defaults otherwise, given the array in file order) run in
process, fastest of 5 runs; Kolmo is the CLI from this branch (`make ARCH= all`), whole process and file
to file, fastest of 3 runs (`-x`: 1 run). MB/s is input bytes / 10^6 per second; aggregate speeds are
total bytes over total time. pcodec's size excludes the 128-byte `.npy` header that kbin and Kolmo
store. kbin rows are at level 8 and one thread unless marked. The typed `.npy` files and the raw arrays
overlap (the same ECG channels appear four times).

### Summary

| Set | pcodec 8 | pcodec 12 | kolmo balanced | kolmo ratio | kolmo -x | kbin | kbin -l 12 | kbin -C |
|---|---|---|---|---|---|---|---|---|
| All 18 files, bytes | 12,306,307 | 12,119,104 | 11,668,107 | 10,465,314 | 9,952,930 | 11,935,283 | 11,824,623 | 11,296,425 |
| against pcodec 8 | | -1.5% | -5.2% | -15.0% | -19.1% | -3.0% | -3.9% | -8.2% |
| 16 integer files, bytes | 9,837,019 | 9,796,331 | 9,770,149 | 8,816,800 | 8,490,071 | 9,650,293 | 9,616,139 | 9,245,383 |
| against pcodec 8 | | -0.4% | -0.7% | -10.4% | -13.7% | -1.9% | -2.2% | -6.0% |
| 8 ECG files, bytes | 3,106,556 | 3,095,184 | 3,709,372 | 3,104,694 | 2,907,209 | 3,096,031 | 3,089,141 | 2,968,839 |
| against pcodec 8 | | -0.4% | +19.4% | -0.1% | -6.4% | -0.3% | -0.6% | -4.4% |
| 7 raw arrays, bytes | 1,593,865 | 1,588,007 | 1,897,737 | 1,592,530 | 1,493,719 | 1,588,241 | 1,584,573 | 1,523,558 |
| against pcodec 8 | | -0.4% | +19.1% | -0.1% | -6.3% | -0.4% | -0.6% | -4.4% |
| 2 float files, bytes | 2,469,288 | 2,322,773 | 1,897,958 | 1,648,514 | 1,462,859 | 2,284,990 | 2,208,484 | 2,051,042 |
| against pcodec 8 | | -5.9% | -23.1% | -33.2% | -40.8% | -7.5% | -10.6% | -16.9% |

kbin at level 8 is smaller than pcodec level 8 on 18 of 18 files; pcodec level 12 is smaller than it
on 9 files, by 0.2% or less on the ECG channels and by 2% on `year` (568 against 581 bytes). Level
against level, `kbin -l 12` is smaller than pcodec level 12 on 18 of 18. `kbin -C` is smaller than
pcodec level 12 on 15 of 18 (not on `xray` and the two `year` files). On the signals where `kolmo -m balanced` is 19% larger
than pcodec (ECG, raw arrays), kbin with the paper's method is level with pcodec, and `-C` is 4.4%
smaller than pcodec and than `kolmo -m ratio`. Kolmo's slow modes stay smaller on 2-D images (MR,
x-ray), on the decimal table (`owid`) and on `lfw`, and `kolmo -x` on ECG.

Speed, one thread and 10 threads (MB/s, compress / decompress; provisional):

| Set | pcodec 8 | pcodec 12 | kolmo balanced | kolmo ratio | kbin | kbin -C | kbin -T 10 | kbin -C -T 10 |
|---|---|---|---|---|---|---|---|---|
| All 18 files | 207 / 1491 | 148 / 1421 | 63 / 110 | 3.5 / 37 | 97 / 662 | 90 / 624 | 411 / 2185 | 408 / 1963 |
| 16 integer files | 158 / 1188 | 129 / 1149 | 45 / 78 | 2.2 / 26 | 161 / 590 | 147 / 565 | 415 / 1669 | 416 / 1519 |
| 8 ECG files | 132 / 1143 | 132 / 1089 | 39 / 72 | 2.2 / 27 | 218 / 850 | 218 / 809 | 356 / 1449 | 345 / 1219 |

On one thread kbin compresses integers about as fast as pcodec and decompresses at about half its
speed; with 10 threads it is faster than single-threaded pcodec in both directions on the large files
(the small ones have one chunk). `kbin -l 12` compresses at 104 MB/s on the integer files (39 MB/s
overall, the float search is costlier) for 0.4% (1% overall) smaller output.

### Per file, bytes

| File | Input bytes | pcodec 8 | pcodec 12 | kolmo balanced | kolmo ratio | kolmo -x | kbin | kbin -l 12 | kbin -C |
|---|---|---|---|---|---|---|---|---|---|
| ecg_2ch_i16.npy | 2,600,128 | 621,315 | 618,684 | 864,881 | 641,932 | 587,080 | 617,512 | 616,108 | 594,221 |
| ecg_ch0_i16.npy | 1,300,128 | 313,437 | 312,434 | 339,974 | 305,172 | 285,265 | 312,979 | 312,342 | 295,965 |
| ecg_ch0_i32.npy | 2,600,128 | 313,586 | 312,599 | 316,774 | 305,172 | 294,786 | 312,985 | 312,348 | 295,971 |
| ecg_ch1_i16.npy | 1,300,128 | 304,940 | 303,875 | 333,319 | 300,286 | 286,614 | 304,552 | 303,785 | 298,275 |
| log_timestamps_ms_i64.npy | 64,472 | 2,549 | 2,530 | 2,717 | 2,464 | 2,464 | 2,468 | 2,441 | 2,372 |
| mr_u16.npy | 9,970,692 | 2,898,096 | 2,885,679 | 2,316,287 | 2,014,294 | 1,940,820 | 2,726,104 | 2,713,755 | 2,454,339 |
| population_i64.npy | 151,280 | 37,440 | 37,317 | 39,873 | 37,458 | 37,458 | 37,206 | 37,044 | 36,215 |
| xray_u16.npy | 8,474,368 | 3,751,193 | 3,734,638 | 3,657,595 | 3,616,738 | 3,561,247 | 3,747,665 | 3,733,196 | 3,743,886 |
| year_i32.npy | 93,636 | 598 | 568 | 992 | 754 | 618 | 581 | 547 | 581 |
| mitdb-100-ch0.i16 | 1,300,000 | 313,437 | 312,434 | 339,821 | 305,080 | 285,193 | 312,973 | 312,336 | 295,959 |
| mitdb-100-ch0.i32 | 2,600,000 | 313,586 | 312,599 | 316,683 | 305,080 | 294,711 | 312,979 | 312,342 | 295,965 |
| mitdb-100-ch1.i16 | 1,300,000 | 304,940 | 303,875 | 333,162 | 300,194 | 286,545 | 304,546 | 303,779 | 298,269 |
| mitdb-100.i16 | 2,600,000 | 621,315 | 618,684 | 864,758 | 641,778 | 587,015 | 617,505 | 616,101 | 594,214 |
| year.i32 | 93,508 | 598 | 568 | 902 | 661 | 518 | 575 | 541 | 575 |
| population.i64 | 151,152 | 37,440 | 37,317 | 39,783 | 37,365 | 37,365 | 37,200 | 37,038 | 36,209 |
| log_timestamps_ms.i64 | 64,344 | 2,549 | 2,530 | 2,628 | 2,372 | 2,372 | 2,463 | 2,436 | 2,367 |
| owid_energy_f64.npy | 23,938,176 | 2,188,878 | 2,044,600 | 1,576,921 | 1,403,314 | 1,259,112 | 2,009,632 | 1,944,081 | 1,778,847 |
| lfw_subset.npy | 1,000,080 | 280,410 | 278,173 | 321,037 | 245,200 | 203,747 | 275,358 | 264,403 | 272,195 |

`kbin -n` (pcodec's delta set, no linear predictor) differs from `kbin` only on `mr_u16.npy`
(2,901,983 bytes: the predictor saves 6.1%) and on `owid` (111 bytes smaller without it).

### Per file, speed (MB/s, compress / decompress; provisional)

| File | pcodec 8 | pcodec 12 | kolmo balanced | kolmo ratio | kolmo -x | kbin | kbin -C | kbin -T 10 | kbin -C -T 10 |
|---|---|---|---|---|---|---|---|---|---|
| ecg_2ch_i16.npy | 58 / 652 | 61 / 622 | 50 / 144 | 1.4 / 17 | 0.01 / 0.02 | 180 / 691 | 197 / 652 | 519 / 1745 | 445 / 1468 |
| ecg_ch0_i16.npy | 274 / 1428 | 248 / 1452 | 18 / 66 | 3.0 / 57 | 0.08 / 0.06 | 197 / 694 | 192 / 672 | 285 / 996 | 262 / 826 |
| ecg_ch0_i32.npy | 522 / 2676 | 495 / 2576 | 65 / 76 | 6.3 / 112 | 0.1 / 0.1 | 394 / 1351 | 381 / 1366 | 550 / 2119 | 521 / 1699 |
| ecg_ch1_i16.npy | 263 / 1426 | 251 / 1404 | 52 / 94 | 2.8 / 20 | 0.05 / 0.05 | 198 / 740 | 196 / 723 | 280 / 994 | 269 / 847 |
| log_timestamps_ms_i64.npy | 552 / 3350 | 471 / 2544 | 6.9 / 9.5 | 2.3 / 4.9 | 0.3 / 7.3 | 99 / 883 | 95 / 786 | 99 / 883 | 96 / 796 |
| mr_u16.npy | 174 / 1052 | 141 / 1053 | 113 / 507 | 1.7 / 33 | 0.03 / 0.6 | 132 / 386 | 118 / 375 | 495 / 1770 | 573 / 1516 |
| population_i64.npy | 117 / 3452 | 51 / 1669 | 8.8 / 26 | 3.4 / 2.8 | 0.4 / 36 | 135 / 1845 | 98 / 1441 | 135 / 1868 | 96 / 1498 |
| xray_u16.npy | 207 / 1454 | 115 / 1398 | 90 / 87 | 3.0 / 33 | 0.5 / 0.5 | 135 / 602 | 118 / 570 | 620 / 2225 | 636 / 3035 |
| year_i32.npy | 562 / 2411 | 494 / 1976 | 2.2 / 6.8 | 1.7 / 7.6 | 0.3 / 0.5 | 138 / 946 | 88 / 520 | 93 / 517 | 90 / 520 |
| mitdb-100-ch0.i16 | 268 / 1424 | 245 / 1395 | 35 / 16 | 1.9 / 21 | 0.5 / 0.5 | 185 / 659 | 173 / 680 | 216 / 969 | 232 / 817 |
| mitdb-100-ch0.i32 | 513 / 2670 | 489 / 1557 | 30 / 183 | 5.3 / 78 | 0.7 / 0.8 | 371 / 1497 | 358 / 1336 | 426 / 2038 | 408 / 1674 |
| mitdb-100-ch1.i16 | 266 / 1417 | 254 / 1404 | 44 / 89 | 2.5 / 16 | 0.8 / 0.8 | 185 / 739 | 178 / 706 | 223 / 1050 | 213 / 854 |
| mitdb-100.i16 | 59 / 639 | 59 / 687 | 48 / 128 | 1.2 / 19 | 0.4 / 0.6 | 161 / 730 | 161 / 658 | 373 / 1644 | 402 / 1511 |
| year.i32 | 274 / 1486 | 240 / 1212 | 2.2 / 4.6 | 1.0 / 3.8 | 0.4 / 0.7 | 81 / 899 | 78 / 874 | 80 / 917 | 61 / 917 |
| population.i64 | 279 / 3452 | 52 / 3093 | 2.8 / 3.4 | 1.3 / 4.7 | 0.6 / 38 | 106 / 1799 | 87 / 1512 | 107 / 1778 | 87 / 1482 |
| log_timestamps_ms.i64 | 575 / 3470 | 492 / 2527 | 1.9 / 3.4 | 1.1 / 1.6 | 0.8 / 22 | 133 / 1609 | 126 / 1287 | 134 / 1693 | 75 / 775 |
| owid_energy_f64.npy | 380 / 2342 | 206 / 2142 | 182 / 326 | 83 / 98 | 6.3 / 447 | 62 / 786 | 58 / 720 | 482 / 4781 | 473 / 3561 |
| lfw_subset.npy | 175 / 1755 | 57 / 1655 | 33 / 41 | 1.1 / 17 | 0.4 / 0.7 | 88 / 1126 | 77 / 1156 | 84 / 666 | 81 / 1217 |

Kolmo's figures include process start and file I/O, which dominate on the small files; `kolmo -x`
ran while other `-x` jobs shared the machine and is far below its usual 0.5 to 1 MB/s.

## What failed or did not pay

* **Quantile groups split by runs.** The first optimiser extended a group over a run of equal values
  that started inside it, so a group could span -44 to +1 with 99% of its values at +1; `year` coded to
  17,780 bytes instead of 565. Runs that cross a boundary now form their own group.
* **Exact-match lookback.** Lookback with only exact matches (and a 2^12 window) left `lfw` at 552 KB of
  residuals; the prefix candidates brought classic + lookback to 344 KB, and float quantisation with
  rounding to 275 KB. A float multiplier with base 1/765 (the image averages) needed 30 bits of
  adjustment per value and lost.
* **Linear prediction on ECG.** Least-squares predictors of order 2 to 32 have higher residual entropy
  than the first difference on MIT-BIH record 100 (3.93 to 4.09 against 3.84 bits; the QRS spikes
  dominate the squared error), so the encoder never picks one there. It pays on `mr_u16` only (6.1%).
* **High predictor orders.** Orders up to 32 were chosen on MR and were 1.1% larger than orders up to 8
  (the Levinson error estimate overfits). The order did not set the decode speed: the reconstruction
  loop was bound by its loop-carried chain through memory. The limit is now 8, with the history in
  registers and the terms that do not need the newest value summed one step ahead, which made the
  reconstruction three times faster (MR decodes at about 355 instead of 185 MB/s, CPU time).
* **More rANS states.** Eight interleaved states with one shared word stream were no faster than four,
  and separate word streams per lane did not speed up the order-0 path measurably either. The order-0
  decoder still runs at about half of pcodec's single-thread speed; the cause is not pinned down (the
  offset-bit reads cost little; the rANS step itself measures about 2.8 ns per symbol under load).
* **Level 12.** 0.9% smaller overall at less than half the compression speed. With `-C` it gains
  only 0.2% (`-C -l 12`: 11,275,352 bytes against 11,296,425), because more bins exceed the 64-bin
  limit of the context extension, which then switches off for those streams; with the earlier
  previous-symbol contexts it was even slightly larger than `-C` at level 8.
* **Not reached.** No 2-D prediction for images (Kolmo's ratio mode is 18 to 26% smaller on MR), no
  dictionary mode, and the decimal table `owid` stays 13% behind `kolmo -m balanced`.

## Tests

`make kbin-check` (and `make experimental-check`) runs `kbin --selftest` with ASan and UBSan and
without: every type with 0, 1, 2, 3, 5, 17, 300, 1025 and 5000 values; constant, zero, random,
extreme values (minimum and maximum alternating), ramps with noise, multiples, outliers, decimals and
float specials (NaN with payloads, signalling NaN, infinities, -0.0, subnormals); with and without a
trailing partial value; detected and forced interleave; one and three threads; with and without the
context extension; streams of 140,000 values (four lanes); canonical and non-canonical `.npy`
headers; and 24 truncated or bit-flipped copies of every frame, which must be rejected or decoded
without a sanitizer report. Mutation of real frames (`year`, timestamps, population, with and without
`-C`; 1,000 sanitizer decodes on the final code, about 2,300 during development) found nothing. The
sanitizer build also round-trips `mr_u16` (predictor, 3 threads), `lfw` and `owid` (`-C`, 4 threads)
and `mitdb-100.i16` (`-C`); the bench mode checks every round trip byte for byte.
