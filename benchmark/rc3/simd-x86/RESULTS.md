# rc3 simd-x86: SIMD kernels for the loops Kolmo owns (x86-64)

16 November 2026. Branch `claude/rc3-simd-x86-20261116`, from `claude/release-rc3-20261116` at `2207da1`, with
`claude/rc3-structure-20261116` (`9ef225d`) merged. Plan: `research/RC3-PLAN.md` (lane "simd-x86 and simd-arm");
previous study: `benchmark/rc2/SIMD-ASM.md`. Scripts are in `tools/`, raw results in `results/`.

## Summary

- **Seven kernels landed, each with a portable C reference, a test against it at every ISA level, and a measured gain
  above the noise.** They are chosen at run time from CPUID through one table (`src/arch/cpu.c`,
  `src/arch/kernels.h`): C reference, SSE2 (the x86-64 baseline), SSE4.1, AVX2. `make NO_ASM=1` builds the references
  only, and `KOLMO_CPU=c|sse2|sse41|avx2` lowers the level of a build at run time.
- **Output is unchanged.** Frames are byte-identical across the branch point, `NO_ASM=1`, the generic build at every
  level (`KOLMO_CPU=c`, `sse2`, `sse41`, dispatched AVX2), `-msse4.2 -mavx2 -mbmi2 -mfma` and `-march=native`
  (AVX-512 host): 17 inputs x 10 settings (section 4). The determinism check passes.
- **Largest gains** (one thread, fastest of N, generic build with dispatch, against the branch point):

  | where | kernel | end to end |
  |---|---|---|
  | 8-bit images and Y4M video, decode (`balanced`, `hc`) | IMG8 median inverse as a 16-row wavefront | 1.3x (RGB) to 2.4x (CIF video) |
  | kvid (the Y4M node format2 is integrating) | strip prediction, encode and decode | `rt` 4.8x compress, 6.4x decode |
  | 16-bit images, compress (`balanced`) | IMG16 stride search and forward prediction | 1.32x (MR), 1.42x (X-ray) |
  | integer arrays, `-m speed` | byte-plane split and merge | 1.16-1.23x compress, 1.49x decode |
  | `-x` (all data) | CM mixer dot products and updates, context hashes | 1.18-1.25x both ways |
  | Y4M video, `eco` decode | byte planes with delta | 1.64x |
  | numeric CSV, `balanced` compress | inline popcount (no libgcc call) | 1.05x |
  | 8-bit images and video, `balanced` compress | IMG8 forward | 1.06-1.07x |

- **Not landed, with the measurement:** a VEX (AVX2-target) and an AVX-512VL build of the median inverse (slower than
  SSE4.1), a wavefront for the 16-bit median inverse (impossible: its rows are serial), a packed slot table for the
  rANS decoder (no gain), hand-written assembly (no kernel where the intrinsics leave a measurable margin; section 6).

SECTION_E2E

## 1. Setup

| | |
|---|---|
| Machine | KVM guest, Intel Xeon @ 2.30 GHz, 4 vCPUs (SSE4.2, AVX2, BMI2, FMA, AVX-512F/BW/VL/VBMI), 15 GiB, Linux 6.18 |
| Compiler | gcc 13.3.0, `-O3 -fno-plt` (release flags, `make ARCH=`) |
| Builds | `base` = branch point `2207da1`, `ARCH=`; `final` = this branch, `ARCH=` (dispatch picks AVX2 here); `final-sse2` = the same binary with `KOLMO_CPU=sse2` (what a CPU without SSE4.1/AVX2 runs); `base2` = a copy of `base` (A/A control) |
| Timing | whole processes (`tools/ab.py`: `posix_spawn` + `wait4`, wall time), builds interleaved and their order reversed every other round; fastest of the rounds, median in brackets; output deleted before each run |
| Threads | 1 and 4 (all vCPUs), never mixed in a comparison |

### Inputs

`tools/mkcorpus.py DL DATA` builds them from downloads kept in their own directory (Silesia, four Kodak images, five
ESC-50 clips, two xiph CIF sequences) plus seeded generators. SHA-256 (first 16 hex digits):

| file | bytes | SHA-256 | what |
|---|---:|---|---|
| text_dickens.txt | 10,192,446 | `b24c37886142e11d` | Silesia `dickens` (English text) |
| text_webster.txt | 16,777,216 | `6aa3aebb41c861af` | Silesia `webster`, first 16 MiB (dictionary, markup) |
| text_xml.xml | 5,345,280 | `0e82e54e695c1938` | Silesia `xml` |
| table_trades.csv | 13,916,983 | `e28e11e6a4ecdcb5` | generated, 400 k rows x 7 columns (timestamps, prices, quantities) |
| log_access.log | 28,033,058 | `26c6281e1c7dd73f` | generated, 300 k access-log lines |
| table_osdb.bin | 10,085,684 | `60f027179302ca3a` | Silesia `osdb` (fixed records) |
| img8_rgb.npy | 4,718,720 | `61c8f348d7295083` | Kodak 01, 05, 13, 23 stacked, uint8 (2048, 768, 3) |
| img8_grey.npy | 1,572,992 | `da99a77ba3957032` | the same in grey, uint8 (2048, 768) |
| img16_mr.npy | 9,970,692 | `802359a5c6cd5f22` | Silesia `mr` as uint16 (as in rc1) |
| img16_xray.npy | 8,474,368 | `e621a2571d1743c0` | Silesia `x-ray` as uint16 |
| audio_esc.wav | 2,205,044 | `8137fc2bb308de18` | five ESC-50 clips, 16-bit mono 44.1 kHz |
| video_akiyo.y4m | 9,124,244 | `de4b75e65cc9bd6c` | xiph `akiyo_cif`, first 60 frames (4:2:0) |
| video_foreman.y4m | 9,124,244 | `2cae659d72d9ea7f` | xiph `foreman_cif`, first 60 frames |
| num_walk_i32.npy | 16,000,128 | `7b5628f375fc613b` | generated int32 random walk, 4 M values |
| num_f64.npy | 16,000,128 | `7a1ee838d01dfba7` | generated float64 walk rounded to 3 decimals |
| num_sao.bin | 7,251,944 | `c2d0ea2cc59d4c21` | Silesia `sao` (star catalogue records) |
| bin_mozilla | 16,777,216 | `fcef5e122dd6d241` | Silesia `mozilla`, first 16 MiB |

## 2. Profile of the branch point

`perf` is not available on this VM; the `LD_PRELOAD` sampler of rc2 (`benchmark/rc2/simd/sampler.c`, `ITIMER_PROF`) was
used, with `tools/sym.py` (innermost inlined function, `addr2line -i`), `tools/lines.py` (by source line) and
`tools/callers.py` (call site one inline level up). The timer ticks every 4 ms here, so the fast modes were run 12 times
per file and direction (`tools/profrep.sh`). Pooled over the 17 inputs, one thread (`results/profile_base.txt`):

| mode | Kolmo's own code | vendored | system (kernel I/O, memcpy) | Kolmo's largest |
|---|---:|---:|---:|---|
| `speed` compress | 1.9% | 82.6% | 15.5% | `shuffle_fwd` |
| `speed` decode | 11.6% | 45.9% | 42.6% | `shuffle_inv` 11.6% |
| `balanced` compress | 29.1% | 61.4% | 9.5% | `img16_detect` 3.2%, `kg_img8_trial` 2.5%, `kg_hist` 1.9%, `sort_u64` 1.6%, `kg_img8_fwd` + `kg_med` 2.2%, `__popcountdi2` 1.2% |
| `balanced` decode | 46.7% | 32.8% | 20.5% | `kg_img8_back` + `kg_med` + `kg_unzz8` 27.3%, `img16_inv` 5.1%, `tpq_inv_cols` 3.1% |
| `eco` decode | 27.9% | 34.8% | 37.3% | `rans_decompress_all` 6.7%, IMG8 inverse 6.1%, `img16_inv` 3.2% |
| `hc` decode | 37.6% | 35.6% | 26.9% | IMG8 inverse 10.9%, `img16_inv` 5.6%, `tbl2_stream` 2.9%, `tpq_inv_cols` 2.7% |
| `ratio` decode | 4.0% | 54.7% | 41.3% (liblzma) | (libbsc and liblzma: the bwt and deps lanes) |
| `eco`, `hc`, `ratio` compress | 1.7-4.8% | 85-97% | | Brotli, zstd-19/22, liblzma, libbsc |
| `-x` compress / decode | 93.9% / 99.8% | | | `cm_predict` 37-40%, `cm_update` 19-20%, `cm_hmix` 13-14%, `cm_upd_sm` 12-13%, `cm_upd_ctr` 6% |

Per file, the shares are larger: `img16_detect` is 21-37% of `balanced` compression of the 16-bit images, the numeric
parsers 40% of the CSV's (6% in the libgcc popcount), `rans_decompress_all` 23-45% of `eco` decoding of the numeric
arrays and the video. Inside `cm_predict`, line-level samples put 23% on the first touch of the `w4` mixer row (a cache
miss on a 16 MB table) and 13% on the four dot products; in `cm_update`, 41% are the four weight updates; the context
hashes (`cm_hmix` at nibble starts and in the two prefetch loops) are about 7% of all `-x` time.

## 3. Kernels

Every kernel: reference `kolmo_<name>_c` in `src/algo/*.c` (the specification), ISA versions in
`src/arch/x86_64/*.c` (intrinsics with `__attribute__((target))`), entry in the table of `src/arch/cpu.c`, and checks
in `tests/test_kernels.c` (`make check-kernels`, also with ASan/UBSan) that compare each level with the reference on
random and edge-case inputs (saturated values, constant runs, every short width, partial rows, damaged-looking
residuals). A mutation test (one lane mask changed in a copy of the kernel) was caught every time it was tried.

Microbenchmarks (`tools/bench_*.c`, fastest of 15, in process, real data from the corpus). The C reference is compiled
`-O3` for baseline x86-64, so "C" already includes gcc's own SSE2 vectorisation.

KERNEL_TABLE

### 3.1 IMG8 median inverse: a 16-row wavefront (`med8_inv`, SSE2, SSE4.1)

The 8-bit image node predicts each byte from its left, upper and upper-left neighbours (LOCO-I median). The inverse is
a serial chain along a row: each byte needs the one C bytes to its left. Rows, though, form a wavefront: byte x of row
y needs only bytes x and x - C of row y - 1. So 16 rows run in the 16 byte lanes of a vector, lane j skewed C bytes
behind lane j - 1. Lane 0 holds the finished row above the group, lanes 1-15 decode 15 new rows. At each step the left
neighbours are the result of C steps before, the upper neighbours that result shifted up one lane (`pslldq`), the
upper-left ones the result of 2C steps before, shifted. Residuals go in and results come out through 16 x 16 byte
transposes (four rounds of byte unpacks), 16 steps at a time; the two triangles at the ends of the skewed band are
decoded by the scalar code. The arithmetic is the reference's, lane by lane. The median of unsigned bytes needs no
widening: `c >= max(a,b)` gives `min(a,b)`, `c <= min(a,b)` gives `max(a,b)`, otherwise `a + b - c` lies between them
and wraps exactly. `kolmo_med8_rows_*` decodes any number of rows below a finished one (kvid uses it too).

| image | C | C ns/B | SSE2 | SSE4.1 | VEX (AVX2 target) | AVX-512VL |
|---|---:|---:|---:|---:|---:|---:|
| Kodak RGB, W = 2304 | 3 | 1.542 | 0.356 (4.3x) | 0.332 (4.6x) | 0.453 | 0.368 |
| Kodak grey, W = 768 | 1 | 4.247 | 0.560 (7.6x) | 0.503 (8.4x) | 0.568 | 0.557 |
| foreman CIF luma, W = 352 | 1 | 4.216 | 0.601 (7.0x) | 0.539 (7.8x) | 0.610 | |

AVX2 CPUs use the SSE4.1 kernel: the VEX encoding of the same code was 25-35% slower, and the AVX-512VL build (32
vector registers, fewer spills) 10% slower. End to end (`balanced`, one thread, fastest of 11, A/A within 1.4%):
decode of the RGB image 1.35x, the grey image 1.83x, foreman 2.38x faster; `hc` RGB decode 1.30x.

### 3.2 Byte-plane split and merge (`planes_split`, `planes_merge`, SSE2)

The shuffle transform (T_SHUF) cuts w-byte words into w byte planes; `-m speed` uses it on integer arrays (w = 8) and
`eco`/`hc` on video (w = 2 with wrapping differences). Sixteen words are w vectors; writing the vector index and the
byte position as one binary number, one round of unpacks of `v[i]` with `v[i + w/2]` rotates that number left by one
bit, so merging is log2(w) rounds and splitting is 4. With delta, the differences are summed back with an
in-register prefix sum carried across steps. One instance per w (constants after inlining; a first version with a
run-time w kept the vectors in memory and was slower than rc2's loop on w = 8). One channel only; other shapes keep the
reference loop.

| w | delta | split rc2 → kernel (ns/B) | merge rc2 → kernel (ns/B) |
|---:|---:|---|---|
| 2 | no | 3.423 → 0.075 (45x) | 0.940 → 0.089 (10.6x) |
| 2 | yes | 3.822 → 0.094 (40x) | 1.004 → 0.086 (11.6x) |
| 4 | no | 1.826 → 0.071 (25.6x) | 0.629 → 0.068 (9.3x) |
| 8 | no | 0.404 → 0.086 (4.7x) | 0.621 → 0.089 (7.0x) |

16 MiB in process; the kernels run at the VM's memory bandwidth. End to end (fastest of 15): `-m speed` on
`num_walk_i32.npy` 1.23x compress, 1.49x decode; on `img16_mr.npy` 1.16x and 1.49x; `eco` decode of akiyo 1.10x,
of foreman 1.64x.

### 3.3 Context-mixing mixer and context hashes (`cm_dot`, `cm_train`, `cm_hash`; SSE4.1, AVX2)

The `-x` coder mixes 35-63 inputs with 2 or 4 weight sets per bit. Dot products: `vpmuldq` on the even and the odd
32-bit lanes gives exact 64-bit products, and integer sums do not depend on their order. Updates: `w += (x * e) >> 12`
with `vpmulld` and `vpsrad`, the reference's 32-bit product and arithmetic shift (|x| <= 2047 and |e| <= 16380, so the
product fits). Tails use masked loads and stores (AVX2). The context hashes of a nibble (20-29 values of `cm_hmix`) are
computed 8 (AVX2) or 4 (SSE4.1) at a time; the bucket search and the prefetch loops then read them. `cm_t` keeps a
pointer to the table, so a bit costs no `pthread_once`. SSE2 keeps the references (no 32-bit `pmulld`/`pmuldq`).

End to end, `-x` on 2 MB prefixes (fastest of 5 for the mixer, of 3 for mixer + hashes; A/A within 1-4%):

| input | mixer, compress | mixer, decode | + hashes, compress | + hashes, decode |
|---|---:|---:|---:|---:|
| text_dickens (2 MB) | 1.18x (SSE4.1 1.11x) | 1.21x (SSE4.1 1.14x) | 1.22x | 1.18x |
| table_trades (2 MB) | 1.23x (SSE4.1 1.12x) | 1.23x (SSE4.1 1.15x) | | |
| img8_rgb (2 MB, image model) | 1.16x | 1.22x | 1.25x | 1.23x |

The rest of `-x` is cache misses into the hashed tables and the counter and state-map updates
(`cm_upd_sm`/`cm_upd_ctr`, scattered over 20-30 slots per bit): not a SIMD shape.

### 3.4 kvid strips (`vid_strip_enc`, `vid_strip_dec`; SSE2, SSE4.1)

kvid (`src/experimental/kvid.c`, the Y4M front end format2 is turning into the video node) picks per 16-row strip the
best of three predictors: spatial median, previous frame, previous frame plus the median of frame differences. The
encoder now computes all three predictors' residuals and their sums of |residual| in one pass over 16 samples at a
time (`psadbw`), then writes the chosen one's residuals. Predictor 2's median of signed differences is the unsigned
median of the differences biased by 0x80. The decoder runs predictor 1 as a vector add, and predictors 0 and 2 through
the wavefront median inverse (predictor 2 on the biased differences). The reference is kvid's prediction, moved to
`src/algo/video.c`; kvid calls the table. In memory (`kvid -b`, fastest of at least 3), 60 frames of CIF:

| sequence | mode | compress MB/s base → final | decompress MB/s base → final |
|---|---|---|---|
| foreman | rt | 35.5 → 171.9 (4.8x) | 138.9 → 889.0 (6.4x) |
| akiyo | rt | 57.3 → 308.9 (5.4x) | 415.8 → 1484.9 (3.6x) |
| foreman | balanced | 28.9 → 97.7 (3.4x) | 125.6 → 532.1 (4.2x) |
| akiyo | balanced | 43.0 → 110.7 (2.6x) | 353.1 → 892.9 (2.5x) |

Frames identical for rt, balanced, eco, hc and ratio on both sequences, from 7 builds and levels.

### 3.5 IMG16 stride search and forward prediction (`sad16`, `img16_fwd`; SSE2, AVX2)

The 16-bit image detector scores every row stride from 16 to 4096 by the sum of |difference| over 4096 samples, for
each byte order, and runs the forward predictor three times on 512 k samples. `sad16` takes |x - y| of unsigned 16-bit
lanes as the OR of two saturating subtractions and sums them in 32-bit lanes (flushed every 32 k samples).
`img16_fwd` computes the left, average (`(a & b) + ((a ^ b) >> 1)`, no overflow) and median predictors, the zigzag and
the byte-plane split; SSE2 gets unsigned order by flipping the top bit, AVX2 has `vpminuw`/`vpmaxuw`. End to end
(`balanced` compression, fastest of 15): MR 1.32x (SSE2 level 1.21x), X-ray 1.42x (SSE2 level 1.23x). `hc` is
dominated by zstd-19 and does not move.

### 3.6 IMG8 forward (`med8_fwd`, SSE2, AVX2)

Elementwise, so rows vectorize directly. `balanced` compression: foreman 1.06x, RGB 1.07x, grey 1.07x (fastest of 15;
A/A within 1%).

### 3.7 Population counts (portable C)

Baseline x86-64 has no POPCNT, so `__builtin_popcountll` calls libgcc's `__popcountdi2` (a table-driven function): 6%
of `balanced` compression of the CSV, in the SWAR decimal parser and the row and field scanners. The parser's mask has
at most bit 7 of each byte set, so its count is one multiply (`((z >> 7) * 0x0101010101010101) >> 56`); the scanners use
an inline SWAR count (the builtin where the ISA has the instruction, e.g. arm64). CSV `balanced` compression 1.048x
(A/A 1.001x). `tests/test_scan.c` (`make check-scan`) compares both counts with the builtin and `parse_dec_fast` with
`parse_dec` on 3 million fields.

## 4. Same bytes everywhere

FRAMES_SECTION

## 5. Tests

TESTS_SECTION

## 6. What did not land, and what is left

- **VEX and AVX-512 builds of the median inverse**: slower (table in 3.1). gcc keeps the 16 transposed vectors, the
  ring of 2C + 16 results and the transposed outputs partly on the stack (523 stack references in the SSE4.1 loop);
  32 AVX-512 registers did not help, VEX three-operand forms neither.
- **Hand-written assembly**: none. The kernels that matter run at memory bandwidth (byte planes, IMG16 SAD, IMG8
  forward) or are a small share after the change; the one with spills (median inverse, 0.33-0.5 ns/B) is now 20% of
  CIF video decode, so a perfect schedule would gain a few percent end to end. A 32-row AVX2 wavefront (two 128-bit
  halves, a cross-half lane shift per step) is the larger opportunity there, and it can be written with intrinsics.
  The structure lane's `asm.h` pattern was not yet merged when this lane finished.
- **16-bit median inverse**: the IMG16 node treats the samples as one sequence with a row stride; the left neighbour of
  a row's first sample is the previous row's last, so rows are serial and the wavefront does not apply. The loop
  already keeps only the left neighbour on the serial chain.
- **rANS order-0 decoder** (codec 14, 4 interleaved 64-bit states, 32-bit renormalisation from one stream): a 32-bit
  slot table (`f << 16 | slot - c`) loaded beside the symbol instead of after it measured -5% to +1% (noise). The
  format leaves no room for lanes; krans' 32-way AVX2 coder needs a format decision (a new leaf codec), which is the
  format2/integration lanes' call.
- **Histograms** (`kg_hist`, 4 tables), the **table and log decoders** (`tbl2_stream`, `tpq_inv_cols`: already SSE2,
  scatter-bound), the **word decoder** (short tokens, already SSE2) and the **CM counters** (scattered slots): no SIMD
  shape that would gain.
- **zstd, Brotli, liblzma, libbsc**: most of the time in `speed`, `eco`, `hc` and `ratio`; vendored, and owned by the
  deps and bwt lanes.

## Reproduce

```
python3 -I tools/mkcorpus.py DL DATA                      # DL: silesia/, kodim*.png, ESC-50 clips, akiyo/foreman_cif.y4m
make ARCH= && make NO_ASM=1 ARCH=                         # in separate trees; also ARCH="-msse4.2 -mavx2 -mbmi2 -mfma" and ARCH=-march=native
make ARCH= check-kernels check-scan                       # kernels against their references, every level, also with ASan
SAMPLER=sampler.so tools/profrep.sh KOLMO DATA OUT 12 cd "-m speed" "-m balanced"; python3 -I tools/sym.py 30 OUT/*.mbalanced.d.*
python3 -I tools/ab.py 15 WORK DATA/img8_rgb.npy:"-m balanced":1 -- base=OLD base2=OLD-COPY final=NEW final-sse2=NEW@KOLMO_CPU=sse2
AB_JSON=runs.jsonl python3 -I tools/ab.py ...; python3 -I tools/summarize.py runs.jsonl base base2 final final-sse2
python3 -I tools/framecmp.py frames.json WORK DATA base=OLD noasm=... generic-c=NEW@KOLMO_CPU=c generic=NEW avx2=... native=...
gcc -O3 -Isrc -Itools tools/bench_k.c src/arch/cpu.c src/algo/{predict,planes,mixer,video}.c src/arch/x86_64/{med8_sse2,planes_sse2,mixer_avx2,video_sse2,img16_sse2,med8f_avx2}.c -lpthread
```
