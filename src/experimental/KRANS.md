# KRANS: static interleaved rANS for Kolmo's leaves (research)

KRANS is a standalone entropy-coding library and tool written for this project: `src/experimental/krans.c`,
`krans.h`, the CLI `build/krans` and the tests in `tests/kranstest.c`. It is **not** used by the `kolmo` binary
or library, it writes its own frame (`KRN1`), and nothing in `src/*.c`, `src/*.inc` or `vendor/` was changed.
It exists to measure whether a rANS layer of our own can beat the coders behind Kolmo's graph leaves: zstd's
Huffman (HUF, via `huf_compress_all`) and the 4-way, 64-bit-state rANS (`rans_compress_all`) in `src/kolmo.c`.

## Provenance and patents

The coder is Jarek Duda's range variant of asymmetric numeral systems (rANS, [arXiv:1311.2540](https://arxiv.org/abs/1311.2540)).
The interleaved-state layout, the 16-bit renormalisation with one read per symbol and the SIMD renormalisation by
permutation follow Fabian Giesen's published descriptions ("Interleaved entropy coders", [arXiv:1402.3392](https://arxiv.org/abs/1402.3392),
and his rANS notes). The order-1 layout (the block split into one contiguous segment per state, the context being the
previous byte within the segment) follows the *description* of htscodecs' rANS-Nx16 in the CRAM 3.1 codec
specification. No code was copied from any of them, nor from zstd, ryg_rans or htscodecs. The reciprocal
encoder uses the standard exact-division-by-multiplication bound (Granlund and Montgomery), derived in the source.

**Only static coding is implemented**: every block carries its own frequency tables, the encoder counts and
normalises before coding, and no probability changes while a block is coded or decoded. Microsoft holds a US patent
(granted January 2022) on modifications of rANS (adaptive variants). We have not analysed its claims; KRANS
deliberately contains no adaptive coding.
Ideas that would adapt probabilities are listed separately under [needs a patent check](#needs-a-patent-check).

## Coder

* **State**: 32-bit unsigned, kept in [2^15, 2^31) (the lower bound L = 2^15). Renormalisation moves 16-bit words.
  With table totals 2^M, M <= 14, a decoded state is at least 2^(15-M) * f >= 2, so a single 16-bit read always
  brings it back above L, and the encoder emits at most one word per symbol. That *at most one read per symbol*
  property is what lets a SIMD decoder renormalise all lanes with one load and one permute.
* **Encode** (back to front): if x >= f * 2^(31-M), emit the low 16 bits and shift; then
  x' = x + c + q * (2^M - f) with q = floor(x / f) computed as `(x * rcp) >> (31 + l)`, `rcp = ceil(2^(31+l) / f)`,
  2^(l-1) < f <= 2^l. For x < 2^31 this is exact (error term x * (rcp * f - 2^(31+l)) < 2^(31+l)) and needs one
  32x32->64 multiply, also for f = 1 and f = 2^M. The emit is branchless (the word is always stored, the pointer
  only moves when it is kept): the branch is data-dependent and mispredicted about as often as taken.
* **Decode**: slot = x mod 2^M, a table gives (symbol, f, slot - c); x = f * (x >> M) + (slot - c), then one
  conditional 16-bit read. The scalar fast path is branchless too.
* **Interleaving, order 0**: N states (8, 16 or 32); byte i is coded by state i mod N, so the decoder writes N
  consecutive bytes per round. A tail of n mod N bytes uses states 0.. in order.
* **Interleaving, order 1**: the block is split into N contiguous segments of floor(n/N) bytes (the remainder goes
  to the last segment); state j codes segment j with context = previous byte of the same segment (0 at a segment
  start). The N contexts are then independent, which is what allows interleaving at all.
* **One word stream per block**: all states share one stream of 16-bit words; the decoder reads it forward, states
  in order 0..N-1 within a round, which is the exact reverse of the encoder's order. The N final encoder states are
  stored first (4 bytes each). After decoding, every state must be back at L and the stream fully consumed: this is
  a free integrity check that catches nearly every corruption the tables let through.

## Tables

* **Normalisation**: counts are scaled to 2^M with rounding, every present symbol gets at least 1, and the remaining
  difference is moved one unit at a time to the symbol where it costs the fewest coded bits (or, when the largest
  frequency is 16 times the difference, all of it goes there).
* **Order-0 table** = alphabet + frequencies. Alphabet: one byte r (0..16) and r pairs (gap from the previous run's
  end, run length - 1), or 255 and a 32-byte bitmap when shorter. Frequencies: `f - 1` as LEB128 (1 byte below 129,
  2 bytes otherwise) for every present symbol but the last, whose frequency is implied by the total.
* **Order-1 table**: the alphabet of contexts that get their **own** table, a byte saying whether a **shared**
  table follows, the shared table, then the own tables in context order. A context with a single successor costs
  only its alphabet (3 bytes). The encoder decides per context whether its own table pays: its own coded bits plus
  8 x table bytes against its bits under the block's order-0 distribution; the shared table is then rebuilt from
  the counts of the contexts that use it. This is the table-size-aware fallback at context granularity.
* **Order choice per block** (`-o a`, default): estimated size of order 0, order 1 and stored (bits from the
  normalised frequencies plus exact table bytes plus 4N state bytes); the smallest wins. A block of one repeated
  byte is coded as such; a block whose coded size is not smaller than its raw size is stored.
* **Precision**: order 0 tries M = 12 and 14, order 1 M = 10 and 12 (`-p` forces one value, 10..14). The larger M
  is taken only when it is estimated at least 1/256 smaller: 14-bit order-0 tables need two gathers in the AVX2
  decoder, and 12-bit order-1 tables are four times larger to build and keep in cache.

## Frame

All integers are LEB128 (`uv`) unless stated; words and states are little-endian.

| field | size | meaning |
|---|---|---|
| magic | 4 | `KRN1` |
| n | uv | original size |
| block | uv | block size, 1 KiB .. 16 MiB (raw length of every block but the last) |
| blocks | ... | in order, until n bytes are covered |

Block: `mode` byte, `uv` payload length, payload. Mode bits 0-1: 0 stored, 1 one repeated byte, 2 order-0 rANS,
3 order-1 rANS; bits 2-3: ways (0: 8, 1: 16, 2: 32); bits 4-7: precision M - 8. Payload of a rANS block: table(s),
N states of 4 bytes, the 16-bit word stream. The payload length makes blocks independently addressable, which
`-T n` uses to decode blocks on n threads (and the encoder to code them).

## Decoders

* **Portable C** (`scalar`): specialised per N (8/16/32) and table layout, states in locals. For M <= 12 a table
  entry is one 32-bit word, `symbol | (f-1) << 8 | (slot-c) << 20` (f - 1 so that a 4096 frequency of a single-
  successor order-1 context still fits); for M = 13, 14 two arrays (`f | (slot-c) << 16` and the symbols).
* **AVX2** (x86-64, chosen at run time with `__builtin_cpu_supports`): 8 lanes per register, N/8 registers.
  One `vpgatherdd` per register (two at M > 12), `vpmulld`, compare against L, `movemask`, then the next 8 words
  are zero-extended (`vpmovzxwd`) and moved to the lanes that need them with `vpermd` from a 256-entry index table;
  the word pointer advances by 2 x popcount. Order 0 packs the 32 symbols with `packus` + `vpermd` and stores 32
  bytes per round. Order 1 gathers the next context's table base from a 256-entry map.
* **Order-1 output staging**: the 32 segments are n/N apart, a power of two for usual block sizes, so the 32 byte
  stores of a round all fall into the same L1 set. Both order-1 decoders (and the order-1 encoder, for its input)
  stage 32 rounds and move them with an SSE2 16x16 byte transpose (four rounds of `punpck{l,h}bw`, which rotate
  the row and column index bits by one each). This alone took the AVX2 order-1 decoder from 330 to 920 MB/s (gcc, dickens).
* **NEON** (arm64): 4 lanes per register; arm64 has no gather, so table entries are loaded per lane and the
  arithmetic, the compare and the renormalisation (`tbl` with a 16-entry shuffle table) are vector. Correctness is
  tested under qemu-aarch64 (with UBSan); its **speed is not measured** (no arm64 hardware here).
* **Safety**: every table is validated (alphabet canonical, frequencies sum to exactly 2^M, every present symbol
  >= 1), states must start in [L, 2^31), SIMD loops run only while their whole round's input is available and the
  scalar tail checks every read; contexts without a table map to an existing table, so a corrupt stream decodes
  garbage in bounds and then fails the final-state check.

## Build and test

```sh
make build/krans krans-check            # round trips and corrupted frames, optimised and with ASan + UBSan
make krans-check-arm64                  # NEON under qemu (apt: gcc-aarch64-linux-gnu qemu-user)
./build/krans -o a -w 32 in out.krn     # compress (verified before writing); -d to decompress
./build/krans -b -r 5 -T 4 files...     # JSON per file: size, encode/decode MB/s per decoder, threads
```

Options: `-o 0|1|a` order, `-w 8|16|32` ways, `-p 0|10..14` precision, `-B KiB` block (default 256), `-T n`
threads, `-D auto|scalar|avx2|neon`, `-r runs` (fastest of at least 3). `-P file prefix` writes the two byte planes
used below. The library is compiled for the baseline ISA (`KRANS_CFLAGS ?= -O3 -fno-plt`) with the AVX2 code in
`target("avx2")` functions: with `-march=native` on this AVX-512 machine, gcc 13 emits EVEX-encoded gathers inside
those functions and the 32-way decoder ran at 1330 instead of 1690 MB/s.

## Results

**Setup.** 4-vCPU Intel Xeon VM at 2.3 GHz (AVX2, BMI2 and AVX-512 present, AVX2 used), 15 GB RAM, Linux 6.18.
Main build: gcc 13.3 `-O3 -fno-plt` (baseline ISA, AVX2 selected at run time) for KRANS and for every rival. zstd
1.5.6 was downloaded and built separately, outside this repository (`bench/krans/build_zstd.sh`), so that HUF and
FSE could be called directly; lz4 1.9.4 is the system library. A second build with clang 18.1 covers the main
configurations. Each figure is the fastest of 5 runs on one thread unless marked T4; MB/s means 10^6 input bytes
per second. A stream type's speed is the 12 files' total bytes over their total time. Every run was decoded and
compared byte for byte (1,260 runs, all verified). Raw data and full tables are in `bench/krans/results/`.

**Inputs** (`bench/krans/streams.sh`, hashes in `bench/krans/silesia.sha256`): the 12 Silesia files (`raw`,
211.9 MB) and, standing in for Kolmo's graph leaves, the byte planes of 16-bit integer residuals: d[i] = x[i] -
x[i-1] over each file read as 16-bit little-endian words, split into `lo` and `hi` planes (106.0 MB each). The
plane streams are much closer to Kolmo's leaves than the raw files are, but they are not Kolmo's leaves.

**Rivals.** `huf`: zstd's `HUF_compress4X_repeat` and `HUF_decompress4X_hufOnly_wksp` on 128 KiB chunks (HUF's
maximum block), with `HUF_flags_bmi2` (the CPU has BMI2). `huf-f0`: the same frames decoded with flags 0, as
`src/kolmo.c` calls it. `fse`: zstd's tANS with tableLog <= 12 on 128 KiB chunks. Both get a 3-byte header per chunk,
as a zstd literals header would. `lz4`, `zstd1` and `zstd3` code each whole stream (LZ, given for context: they
are not entropy coders).

### Main comparison (gcc)

Sizes relative to `huf`. KRANS configurations: `o0`/`o1`/`auto` order, `w` ways, `p` precision, `B` block in KiB
(default 256).

| stream | coder | size vs huf | enc MB/s | dec MB/s (AVX2) | dec MB/s (scalar C) |
|---|---|---:|---:|---:|---:|
| raw | huf (128 KiB) | 62.25% of input | 1255 | 1605 | |
| raw | fse (128 KiB) | -0.50% | 505 | 503 | |
| raw | krans o0 w32 p12 B128 | -0.40% | 427 | **1606** | 517 |
| raw | krans o0 w32 p12 B64 | -0.93% | 422 | 1575 | 510 |
| raw | krans o1 w32 p10 | -24.65% | 145 | 748 | 355 |
| raw | krans auto w32 | -24.88% | 130 | 557 | 291 |
| raw | krans auto w32 B1024 | -25.08% | 151 | 675 | 297 |
| raw | lz4 / zstd -1 / zstd -3 | -23.5% / -44.4% / -49.6% | 662 / 526 / 292 | 3564 / 1470 / 1348 | |
| lo | huf (128 KiB) | 79.60% of input | 1355 | 1295 | |
| lo | fse (128 KiB) | -0.38% | 532 | 513 | |
| lo | krans o0 w32 p12 B128 | -0.38% | 477 | **1627** | 523 |
| lo | krans o0 w32 p12 B64 | -0.68% | 444 | 1433 | 516 |
| lo | krans o1 w32 p10 | -20.82% | 119 | 567 | 310 |
| lo | krans auto w32 | -20.92% | 97 | 543 | 289 |
| lo | krans auto w32 B1024 | -22.84% | 137 | 532 | 248 |
| lo | lz4 / zstd -1 / zstd -3 | -19.4% / -31.4% / -37.8% | 813 / 574 / 308 | 4444 / 1270 / 1337 | |
| hi | huf (128 KiB) | 74.41% of input | 1179 | 1318 | |
| hi | fse (128 KiB) | -0.83% | 519 | 502 | |
| hi | krans o0 w32 p12 B128 | -0.82% | 464 | **1567** | 521 |
| hi | krans o0 w32 p12 B64 | -1.20% | 430 | 1580 | 518 |
| hi | krans o1 w32 p10 | -24.98% | 126 | 564 | 301 |
| hi | krans auto w32 | -25.01% | 108 | 555 | 296 |
| hi | krans auto w32 B1024 | -27.16% | 140 | 500 | 191 |
| hi | lz4 / zstd -1 / zstd -3 | -21.1% / -35.8% / -42.4% | 717 / 574 / 308 | 3457 / 1177 / 1234 | |

Reading it:

* **Order 0, same block size as HUF (128 KiB), 32 ways, AVX2**: 0.4-0.8% smaller than HUF and as fast to decode
  on raw text (1606 vs 1605 MB/s). It is **1.19-1.26x faster** on the residual planes (1567-1627 vs 1295-1318
  MB/s), which are the leaf-like streams. Encoding runs at a third of HUF's speed (430-480 vs 1180-1360 MB/s).
  FSE reaches the same sizes but decodes at 500 MB/s.
* **Order 1 is the large win in size**: 20.8-25.0% smaller than HUF on every stream type (per file, `auto`
  against order 0 at 128 KiB: raw 10.3-44.7%, median 22.0%; lo 0.1-48.8%, median 19.7%; hi 0.4-47.6%, median
  20.6%). That is above the 5.9-19.6% published for real (post-LZ) streams; plain byte planes keep far more
  previous-byte structure. On the planes order 1 gets close to lz4's size or beats it (hi: -25.0% against lz4's
  -21.1%). It costs decode speed: forced order 1 decodes at 564-748 MB/s (0.43-0.47x HUF), `auto` at 543-557 MB/s
  (0.35-0.42x; it more often picks 12-bit tables).
* **The automatic choice works**: `auto` coded 1,606 of 1,637 blocks with order 1 and kept order 0 where the
  order-1 tables do not pay (all 17 blocks of x-ray's lo plane, 10 of 17 on its hi plane). On no file is it larger
  than the better of forced order 0 and forced order 1.
* **Kolmo's own HUF call** (`huf-f0`, flags 0) decodes 20-24% slower than with `HUF_flags_bmi2` *in this
  baseline-ISA build* (1213 vs 1605 MB/s raw). Built with `-march=native`, as Kolmo builds itself, zstd compiles
  BMI2 in statically and flags 0 was no slower (2063 MB/s with flags 0, 1895 with the BMI2 flag on dickens: noise). The flag only
  matters for portable builds (packages, wheels). It is an easy fix, but outside this change.

### Interleave width (order 0, 12-bit, 256 KiB blocks, decode MB/s, gcc)

| ways | raw AVX2 | raw scalar | lo AVX2 | lo scalar | hi AVX2 | hi scalar | size vs 32 ways |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 8 | 508 | 531 | 511 | 535 | 514 | 543 | -0.04% |
| 16 | 972 | 513 | 974 | 516 | 965 | 507 | -0.02% |
| **32** | **1595** | 515 | **1514** | 513 | **1541** | 510 | 0 |

**32 ways is the best width** for the AVX2 decoder: 3.0-3.1x the 8-way AVX2 decoder and 2.8-3.0x the best scalar
decoder, in line with the 2.9x published for 32-way interleaving, at a size cost of 4 bytes per extra state per
block (+0.04%). The AVX2 decoder is latency-bound (gather, `vpmulld` and the renormalisation chain), so fewer
independent registers run proportionally slower: 8-way AVX2 is *slower* than scalar C. Scalar C barely depends on
the width (510-540 MB/s). For order 1 the picture is the same: 32 ways 564-748 MB/s against 8 ways 275-314 MB/s
with AVX2, and 300-370 MB/s scalar at any width.

Precision: 14-bit order-0 tables save 0.01-0.03% and decode at 941-972 MB/s instead of 1514-1595 MB/s (two gathers
per register). 12-bit order-1 tables save 0.28% on raw, 0.05% on lo, lose 0.10% on hi, and decode at 303-482 MB/s
instead of 564-748. The automatic choice therefore takes the higher precision only when it saves at least 1/256.

### Block size (32 ways)

| block | o0 raw | o0 lo | o0 hi | o0 dec AVX2 (raw) | auto raw | auto lo | auto hi | auto dec AVX2 (raw) |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 64 KiB | -0.93% | -0.68% | -1.20% | 1575 | -22.97% | -17.19% | -20.15% | 532 |
| 128 KiB | -0.40% | -0.38% | -0.82% | 1606 | | | | |
| 256 KiB | +0.30% | -0.00% | -0.36% | 1595 | -24.88% | -20.92% | -25.01% | 557 |
| 1 MiB | +1.74% | +0.67% | +0.47% | 1691 | -25.08% | -22.84% | -27.16% | 675 |

(sizes vs huf at 128 KiB). The two orders pull in opposite directions. Order-0 tables are cheap (175-235 bytes per
block), so smaller blocks track local statistics and win. Order-1 tables cost 6.7-13.8 KB per 256 KiB block (2.7-5.1
KB at 64 KiB), so larger blocks amortise them; at 64 KiB up to a fifth of order 1's gain goes into tables. Decode speed hardly depends on the block size,
except that order-1 table construction (up to 257 x 2^M entries per block) shows at 64 KiB.

### Multithreaded, independent blocks (auto w32, 256 KiB blocks, 4 vCPUs)

| stream | enc 1T | enc 4T | dec 1T | dec 4T |
|---|---:|---:|---:|---:|
| raw | 128 | 477 | 635 | 2290 |
| lo | 98 | 356 | 542 | 1950 |
| hi | 107 | 388 | 535 | 1907 |

Scaling is 3.6-3.7x on 4 vCPUs. With 4 threads the order-1 decoder reaches 1.4-1.5x HUF's single-thread speed.

### clang 18 (same flags, main configurations; rivals rebuilt with clang)

| stream | o0 w32 p12 dec AVX2 / scalar | o1 w32 p10 dec AVX2 / scalar | o0 / o1 enc | huf dec / enc | huf-f0 dec |
|---|---:|---:|---:|---:|---:|
| raw | 1668 / 698 | 810 / 454 | 503 / 198 | 1581 / 1168 | 1338 |
| lo | 1650 / 709 | 617 / 379 | 547 / 160 | 1288 / 1322 | 1163 |
| hi | 1617 / 681 | 577 / 363 | 522 / 168 | 1337 / 1321 | 1157 |

clang generates better code for the scalar decoders (+28-38% over gcc) and the encoders (+16-37%). The AVX2
decoders differ by 2-9%. KRANS order 0 at 32 ways is 1.06x HUF on raw and 1.21-1.28x on the planes with clang.

### Against the published claims

* *2.9x decode speed for 32-way interleaved rANS at the same size*: reproduced against an 8-way rANS (3.0-3.1x with
  AVX2, 2.8-3.0x against the best scalar decoder, sizes within 0.04%), **not** against zstd's HUF, which is itself 4-stream interleaved with a hand-written x86-64
  asm loop. Against HUF the gain is 1.0x on text and 1.2-1.3x on residual planes.
* *Static order-1 rANS 5.9-19.6% smaller than order 0*: on these streams the median gain is 20-22% (range 0-49%).
  Order 1 helps most where previous-byte structure survives (text, nci, xml), and least where the bytes are close to
  independent noise (x-ray, sao), where `auto` falls back to order 0.

### What this means for Kolmo

* An order-0 KRANS leaf coder (32 ways, 128 KiB blocks) could replace HUF leaves at equal or 0.4-0.8% smaller size,
  with 1.2-1.3x faster leaf decoding on plane-like data. Leaves are only part of Kolmo's decode time, so on its
  own this cannot close the 0.82-0.85x gap to zstd -3. It is a local gain, measured on stand-in streams, and needs a
  measurement inside Kolmo before any format change.
* Against the existing 4-way 64-bit rANS leaf coder (`rans_compress_all`, 2^16 totals), the 32-way layout should give
  a similar interleaving gain (the 8-way vs 32-way numbers above); that coder was not measured here, since it is
  internal to `src/kolmo.c`. Its 16-bit precision is not needed: 12 bits lost
  at most 0.03% here.
* Order 1 is the bigger lever for leaves that are still byte-correlated: 20-25% smaller here, at 0.35-0.47x HUF's
  decode speed on one thread (1.4-1.5x with 4 threads over blocks). It suits a ratio-oriented mode, not the default speed
  mode.

## What did not work (or did not pay)

* **Direct order-1 output stores.** The 32 segment starts are n/32 apart, a power of two for 64 KiB-1 MiB blocks,
  so the 32 byte stores of a round all map to one L1 set. The first AVX2 order-1 decoder ran at 330 MB/s (scalar
  32-way at 150). Staging 32 rounds and transposing raised it to 920-1000 MB/s on dickens (the same fix sped up the
  order-1 encoder's reads).
* **Branchy renormalisation.** The "need a word?" test is data-dependent. With branches the encoder ran at 240 MB/s
  and the scalar decoder at 250 MB/s (gcc, dickens); branchless, 440-540 MB/s and 570-710 MB/s.
* **`-march=native` with gcc.** On this AVX-512 machine gcc 13 used EVEX encodings inside the `target("avx2")`
  functions, and the 32-way decoder dropped from 1690 to 1330 MB/s. Hence the baseline-ISA build.
* **14-bit tables** (two gathers) and **12-bit order-1 tables**: almost no size gain and 35-47% slower decoding
  (see above).
* **8-way and 16-way AVX2**: latency-bound; 8-way AVX2 is slower than scalar C. NEON (4 lanes, no gather) is likely
  to follow the same rule, but it has not been timed on hardware.
* **Exact greedy normalisation for order 1.** Moving one unit at a time to the cheapest symbol (`log2` per symbol
  per unit) took 65% of the order-1 encode time; a `qsort`-based version still took about 40%. The final version (floor,
  largest remainders by 256 buckets, proportional cut, at most 8 greedy units; plans cached so each context is
  normalised once per precision) doubled order-1 encoding speed on mozilla and costs about 0.1% in size on order-1
  streams (mozilla raw 28,202,398 -> 28,231,166 bytes). Order-1 encoding is still the slowest part (100-150 MB/s with
  gcc): building and costing 256 tables per block at two precisions dominates.
* **Order-1 table build at small blocks**: up to 257 x 2^M decode-table entries per block show at 64 KiB
  (530 MB/s against 675 MB/s at 1 MiB).

## Needs a patent check

Not implemented, deliberately: each of these changes probabilities during coding or derives them from data
already coded, which is where the January 2022 Microsoft patent on rANS modifications may apply.

* Adaptive order-0/order-1 frequencies (counts updated as symbols are coded, periodic renormalisation of the
  decoding table), which would remove the 2.7-13.8 KB order-1 table cost per block.
* Mixing or switching between order-0 and order-1 models per symbol or per segment from coded statistics.
* Predicting a block's tables from the previous block's decoded data (rather than transmitting them). Transmitting
  only a delta to explicitly coded previous tables is still static coding, but it should be checked too.

Static ideas that remain open (no patent concern identified, not done for lack of time): a 64-bit-state AVX-512
or 16-lane variant, an AVX2 encoder, a cheaper order-1 table header (a shared alphabet, frequencies coded with
fewer bits), and timing NEON on real arm64 hardware.
