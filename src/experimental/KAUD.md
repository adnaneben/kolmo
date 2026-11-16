# KAUD: a lossless coder for 16-bit PCM WAV (research tool)

`build/kaud` codes RIFF/WAVE files with 16-bit integer PCM, mono or stereo, into a `KAU1` stream and back,
byte for byte. It is a standalone research tool: the `kolmo` binary and library do not use it, and nothing in
`src/*.c`, `src/*.inc` or `vendor/` changed. The code is `src/experimental/kaud.c` (one file, C11 + POSIX threads);
the benchmark scripts are in `bench/kaud/`.

It was written for this task from published descriptions of the methods (RFC 9639 for the FLAC ideas of
partitioned Rice coding and of transmitted quantised LPC coefficients; the textbook covariance method of linear
prediction; NLMS and sign-sign LMS adaptive filters and cascades of them, as described for MPEG-4 ALS RLS-LMS and
Monkey's Audio; a carry-propagating binary range coder). No code was copied from FLAC, WavPack, Monkey's Audio,
OptimFROG, TTA or any other project.

```sh
make build/kaud                    # or: make kaud-check  (ASan/UBSan self-test)
./build/kaud    in.wav out.kau     # profile 0 (fast), effort 1
./build/kaud -1 in.wav out.kau     # profile 1 (strong)
./build/kaud -d out.kau back.wav   # decode (any profile); -T n decodes chunks in parallel
./build/kaud -b -1 -T 4 *.wav      # bench: encode, decode, compare; JSON with sizes and MB/s (fastest of -r 3 runs)
```

Options: `-0`/`-1` profile; `-e 0..3` encoder effort (profile 0: 0 = no block splitting, 1 = default, 2-3 = more
predictor candidates, coefficient precisions and an exact stereo choice); `-T n` threads; `-c L` chunk = 2^L samples
(default 18); `-f N` frame length (default 4096 for profile 0, 32768 for profile 1); `-s D` split depth.
Profile 1 tuning knobs are stored in the stream header, so any setting decodes: `--mu a b c`, `--taps a b c`,
`--cross a b`, `--comb s`, `--ss s`, `--lpc1 p`, `--rate r`.

## Design

**Input.** The WAV parser accepts `WAVE_FORMAT_PCM` or `WAVE_FORMAT_EXTENSIBLE` with the PCM sub-format GUID,
16 bits, 1 or 2 channels, block align 2 x channels; everything else is refused with a message (24-bit, float,
8-bit, more than two channels, no `fmt` before `data`, two `fmt` chunks, ...). Every byte before the first sample
(RIFF header, `fmt`, any other chunks) and every byte after the last whole sample frame (an odd byte of the data
chunk, `LIST`, `id3 ` and other trailing chunks, garbage) is stored verbatim, so the decoded file is byte-identical.
A data chunk whose size field runs past the end of the file is coded up to the end of the file.

**Chunks and threads.** The samples are cut into independent chunks of 2^18 sample frames (about 6 s at 44.1 kHz).
Every predictor, filter and probability starts from zero at a chunk start, and the history before a chunk is
zero. Chunks are coded and decoded in parallel by `-T n` worker threads that take the next chunk from a shared
counter. The stream does not depend on `-T`; it only depends on the chunk size. ESC-50 clips (220500 samples) are
one chunk each, so they gain nothing from threads at the default chunk size.

**Integer decoding.** The decoder uses integer arithmetic only. Predictor sums are computed modulo 2^32
(`uint32_t`), and the same wrapping arithmetic is used by the encoder, so a prediction that overflows still
round-trips; the encoder only guarantees that every residual stays below 2^24. A vectorised AVX2 path and the plain
C path of the NLMS update give identical results (checked by encoding with one build and decoding with the other).
The encoder's analysis uses doubles (least squares, bit estimates); that only influences which coefficients are
chosen, never how a stream decodes. Cholesky dot products use an OpenMP SIMD reduction, so the encoder's choices
(not the decoded result) could differ between CPUs with different vector widths.

### Profile 0 (fast)

Frames of 4096 samples. A frame is a binary split tree of blocks (default depth 2, so blocks of 4096, 2048 or
1024; a block splits only if both halves have at least 256 samples). For every block the encoder computes, for
each of the signals L, R, S = L - R and M = (L + R) >> 1, the least-squares covariance matrix up to order 32 and
its Cholesky factorisation, which gives the residual energy of every order at once. From those it estimates the
bits of each block (0.5 n log2(E_p / n) + 13 bits per coefficient + header), picks the stereo pair (L/R, L/S, R/S
or M/S) and decides the splits bottom-up. Covariance rows add up over time, so a parent block's row is the sum of
its children's and only the smallest blocks are scanned; the matrix follows from the row by the diagonal
recurrence C[i+1][j+1] = C[i][j] + x[-1-i] x[-1-j] - x[n-1-i] x[n-1-j]. The products of 17-bit integers are exact
in double precision, so the vectorised scan is exact and order-independent.

For a chosen block and signal: constant blocks are sent as one value; wasted low bits are shifted out; the
best estimated order is quantised (15 significant bits, error feedback), the residual is computed, and a
partitioned Rice code is planned (partition order 0..8, parameter per partition, or raw bits, or zero bits for an
all-zero partition); a verbatim block is used if smaller. Unlike FLAC, there are no warm-up samples: the predictor
reads the previous samples of the chunk, in the block's stereo domain (rebuilt from L and R by the decoder).
Higher efforts evaluate 3 or 5 orders and 3 coefficient precisions with the residual's real cost, and choose the
stereo pair from exact costs.

### Profile 1 (strong)

Profile 0's stereo and LPC stage (least-squares LPC, here order <= 16 on frames of 32768 samples without
splitting, one stereo pair per chunk), then per channel slot a cascade on the LPC residual v:

| stage | filter | input history | adapts on |
|---|---|---|---|
| 0 | NLMS, 256 taps + 32 cross-channel taps, mu = 256/4096 | v of this slot; v of the other slot | e0 = v - p0 |
| 1 | NLMS, 32 taps + 8 cross-channel taps, mu = 512/4096 | e0; v of the other slot | e1 = e0 - p1 |
| 2 | NLMS, 16 taps, mu = 1024/4096 | e1 | e2 = e1 - p2 |
| 3 | sign-sign LMS, 16 taps | e2 | e3 = e2 - p3 |
| combiner | P = sum c_k p_k, c_k start at 1, c_k += 64/16384 sign(r) sign(p_k) | p0..p3 | final r = v - P |

The second slot of a stereo pair sees the first slot's current value v (it is decoded first), the first slot sees
the second's previous values. The NLMS weights are adapted in int32 (Q20) with the normalised step
mu e h / (sum h^2 + eps); the division is a 2048-entry integer reciprocal table, the step is an int16 with a 0..8
bit exponent, and the per-tap update is the rounded high product `(g h + 2^14) >> 15` (one `vpmulhrsw` per 16
taps). Prediction uses an int16 copy of the weights (Q14, saturated), so the dot product is 16 x 16 -> 32-bit
multiply-adds summed modulo 2^32.

**Residual coder.** Per block and slot the encoder runs the cascade first, then chooses a partition order (0..5,
partitions of at least 128 samples) and a Rice-style magnitude parameter k per partition, which are sent. A residual
r is coded with an adaptive binary range coder: the bit length b of |r| is coded as its rank in the order k+1, k,
k+2, k-1, k+3, ... (one adaptive decision per rank, usually one or two); the two bits below the MSB are modelled,
the remaining low bits go to a separate raw bit stream, and the sign is modelled with the previous sign as context.
The context of the rank decisions is (k, where the recent mean |r| (rate 1/16) lies relative to k, in half-octave
steps, 9 levels). Probabilities are 16-bit with a hit count; the adaptation shift grows with log2 of the count up
to 7. Block headers (split flags, stereo pair, LPC coefficients, partition parameters) are bypass bits of the range
coder.

## Stream layout (`KAU1`, version 1)

All multi-byte integers are little endian.

| offset | bytes | field |
|---:|---:|---|
| 0 | 4 | `KAU1` |
| 4 | 1 | version (1) |
| 5 | 1 | profile (0 or 1) |
| 6 | 1 | channels (1 or 2) |
| 7 | 1 | log2 of the chunk length in sample frames (8..30) |
| 8 | 2 | frame length F (16..32768) |
| 10 | 1 | encoder effort (informative) |
| 11 | 1 | profile 1 maximum LPC order |
| 12 | 6 | profile 1 NLMS steps mu0, mu1, mu2 (u16 each, in 1/4096) |
| 18 | 2 | profile 1 sign-sign LMS step (Q20) |
| 20 | 1 | profile 1 probability adaptation cap (1..12) |
| 21 | 1 | split depth D (0..5) |
| 22 | 8 | sample frames N |
| 30 | 2 | profile 1 stage-0 taps |
| 32 | 2 | profile 1 stage-1 and stage-2 taps (u8 each) |
| 34 | 2 | profile 1 cross-channel taps of stages 0 and 1 (u8 each) |
| 36 | 1 | profile 1 combiner step |
| 37 | 4 + H | H, then the H bytes of the file before the first sample |
| | 4 + T | T, then the T bytes after the last coded sample frame |
| | 4 | CRC-32C of the whole original file |
| | 8 per chunk | payload bytes (u32), CRC-32C of the chunk's PCM bytes (u32) |
| | | chunk payloads, in order |

Chunk i holds sample frames [i 2^L, min(N, (i + 1) 2^L)). The decoder checks every chunk CRC and the file CRC.

**Common structure of a chunk.** Frames of F samples (the last may be shorter). A frame is a block of depth 0. A
block of n samples at depth d with d < D and n >= 512 starts with a split bit; if it is 1 the block is two blocks
of n/2 and n - n/2 samples at depth d + 1. Otherwise it is a leaf: for stereo, 2 bits select the slot pair (0: L,R;
1: L,S; 2: R,S; 3: M,S with S = L - R and M = floor((L + R) / 2); L = (2M + (S & 1) + S) >> 1, R = L - S), then one
subframe per slot. A slot's history (32 samples before the block) is the slot signal computed from the decoded L
and R, zero before the chunk start.

**Profile 0 payload** is one MSB-first bit stream, zero-padded to a byte. Rice(k) of an unsigned u: if
q = u >> k is below 32, q zero bits, a one bit, then the k low bits of u; otherwise 32 zero bits and u in 25 bits.
Signed values use zigzag (2v for v >= 0, -2v - 1 for v < 0). A subframe:

| field | bits |
|---|---|
| type: 0 constant, 1 verbatim, 2 LPC | 2 |
| constant: value, two's complement | 17 |
| verbatim: width W, then n zigzag values of W bits | 5 + nW |
| LPC: wasted-bits flag, [wasted bits w] | 1 [+ 4] |
| order p (0..32) | 6 |
| if p > 0: shift s (0..30), coefficient Rice parameter kc, p coefficients Rice(kc) of zigzag(q_j) | 5 + 4 + ... |
| partition order po (0..8) | 4 |
| per partition (2^po of n >> po samples, the last takes the rest): k (0..30), then the residuals Rice(k); or k = 31, width W (0..26), and W-bit zigzag residuals | 5 + ... |

LPC reconstruction in the shifted domain y = x >> w: y[t] = e[t] + (int32(sum_j q_j y[t-1-j] mod 2^32) >> s), with
an arithmetic shift; then x = y << w.

**Profile 1 payload**: u32 length R of the range-coded part, R bytes of range coder output, then an MSB-first raw
bit stream with the low mantissa bits. The range coder (32-bit range, carry through a cached byte, normalisation
below 2^24, 16-bit probabilities of a one bit, bound = (range >> 16) p) carries, in stream order: split bits and
slot pairs as bypass bits; per slot the subframe header as bypass bits (type 0 constant + 17 bits, or type 2 LPC
with the same fields as profile 0, coefficients Rice-coded with bypass bits); per non-constant slot the partition
order (3 bits) and one 5-bit k per partition; then the residuals interleaved sample by sample over the slots. A
constant slot codes no residuals and does not run its cascade (its cross-channel contribution is 0).

## Results

Machine: 4-core Intel Xeon @ 2.10 GHz (AVX-512), Linux 6.18, gcc 13.3 `-O3 -march=native`. Tools: flac 1.4.3
and wavpack 5.6.0 (Ubuntu 24.04 packages; neither version has a thread option); Monkey's Audio and TTA are not
packaged for this system and were not measured; Kolmo 1.2.0-rc2 (`build/kolmo` of this branch). Every tool
encodes each file to a file and decodes it back by separate processes, run one after another with nothing else
running; wall and CPU time (user + system of the child) are summed over the set, the whole set is run three times
and the fastest totals are kept (`bench/kaud/run.py`). Every decoded file was compared with the original (whole
file for kaud, Kolmo and WavPack; samples for FLAC, which writes its own WAV header). MB/s is the original WAV
bytes divided by the time. `flac -8` includes FLAC's default 8 KiB padding block and seek table; `flac-8-nometa`
is `flac -8 --no-padding --no-seektable`, which matters on 5-second clips. The data sets and their licences are
listed in `bench/kaud/fetch_data.sh`.

**Summary.** On the 40 ESC-50 clips, profile 0 (default effort) is 8.2% smaller than `flac -8` (4.0% smaller
than `flac -8` without its padding and seek table) and smaller on all 40 clips; profile 1 is 12.3% smaller
(8.2% without FLAC metadata), smaller than every rival on every clip, and 6.7% smaller than `wavpack -hhx6`.
On the four music tracks, profile 0 is 0.55% smaller than `flac -8` at the same encoding speed, and profile 1 is
5.4% smaller (smaller on all four tracks, 4.0% to 6.4%) and 4.0% smaller than `wavpack -hhx6`. Kolmo's best
setting measured here (`-x -l 2`) is 9.7% (ESC) and 13.4% (music) larger than `flac -8`.

Speeds, single thread, whole process: profile 0 encodes at FLAC -8 speed (44 vs 50 MB/s on the short ESC
files, 32 vs 31 MB/s on music) but **decodes slower than FLAC** (61 vs 88 MB/s on ESC, 80 vs 132 MB/s on music):
its order-32 reconstruction is a sequential dependency, and FLAC's decoder is more tuned. Profile 1 is a symmetric
coder at 6.6-9 MB/s per thread, roughly 15x faster than `wavpack -hhx6` to encode but 4x slower to decode.
With `-T 4` the music set (2^18-sample chunks) codes at 100 / 218 MB/s (profile 0) and 26 / 28 MB/s (profile 1);
the 5-second ESC clips are one chunk each, so threads do not help them. Kolmo `-m ratio -T 4` ran no faster than
`-T 1` on these files.

**Cost of chunk restarts** (in-process, same encoder, sizes in bytes):

| setting | chunk 2^16 | chunk 2^18 (default) | one chunk per file |
|---|---:|---:|---:|
| profile 0, ESC | 6853836 | 6837986 | 6837986 |
| profile 0, music | 76088549 | 76015424 | 75991058 (2^18: +0.03%) |
| profile 1, ESC | 6564482 | 6535502 | 6535502 |
| profile 1, music | 72566885 | 72307114 | 72144570 (2^18: +0.23%) |

Profile 0 effort settings on the full sets (in-process): effort 0 ESC 6885412 / music 76620860 (about 53 MB/s
encode on music), effort 1 6837986 / 76015424, effort 2 6813311 / 75736399 (11 MB/s encode).

The raw JSON with per-file sizes is `bench/kaud/results-20261116.json`; `bench/kaud/report.py` prints the tables.
Kolmo `-x -l 2` on music was run once instead of three times (it takes about 30 minutes per pass).

### esc (40 files, 17641760 bytes)

| tool | bytes | vs flac -8 | ratio | enc MB/s wall | enc MB/s CPU | dec MB/s wall | dec MB/s CPU |
|---|---:|---:|---:|---:|---:|---:|---:|
| flac-5 | 7654198 | +2.76% | 0.4339 | 76.35 | 84.23 | 92.22 | 101.63 |
| flac-8 | 7448556 | +0.00% | 0.4222 | 49.91 | 52.37 | 88.48 | 94.41 |
| flac-8-nometa | 7119836 | -4.41% | 0.4036 | 51.12 | 53.20 | 89.14 | 96.73 |
| wavpack-hh | 7142740 | -4.11% | 0.4049 | 27.85 | 28.71 | 30.15 | 31.67 |
| wavpack-hhx6 | 7005472 | -5.95% | 0.3971 | 0.56 | 0.56 | 31.15 | 33.57 |
| kolmo-balanced-typedhuf-T1 | 8982932 | +20.60% | 0.5092 | 91.93 | 95.74 | 151.29 | 165.29 |
| kolmo-ratio-T1 | 8630460 | +15.87% | 0.4892 | 2.10 | 2.13 | 24.88 | 26.34 |
| kolmo-x-l2-T1 | 8168539 | +9.67% | 0.4630 | 0.15 | 0.16 | 0.17 | 0.18 |
| kolmo-ratio-T4 | 8630460 | +15.87% | 0.4892 | 2.03 | 2.05 | 24.54 | 25.75 |
| kaud-0-T1 | 6837986 | -8.20% | 0.3876 | 43.97 | 46.80 | 60.72 | 63.60 |
| kaud-0-T4 | 6837986 | -8.20% | 0.3876 | 44.54 | 47.36 | 64.82 | 67.97 |
| kaud-1-T1 | 6535502 | -12.26% | 0.3705 | 8.46 | 8.68 | 9.24 | 9.57 |
| kaud-1-T4 | 6535502 | -12.26% | 0.3705 | 8.01 | 8.29 | 9.08 | 9.40 |

| kaud setting | rival | kaud smaller | kaud larger | equal |
|---|---|---:|---:|---:|
| kaud-0-T1 | flac-5 | 40 | 0 | 0 |
| kaud-0-T1 | flac-8 | 40 | 0 | 0 |
| kaud-0-T1 | flac-8-nometa | 40 | 0 | 0 |
| kaud-0-T1 | wavpack-hh | 38 | 2 | 0 |
| kaud-0-T1 | wavpack-hhx6 | 36 | 4 | 0 |
| kaud-0-T1 | kolmo-balanced-typedhuf-T1 | 40 | 0 | 0 |
| kaud-0-T1 | kolmo-ratio-T1 | 40 | 0 | 0 |
| kaud-0-T1 | kolmo-x-l2-T1 | 40 | 0 | 0 |
| kaud-1-T1 | flac-5 | 40 | 0 | 0 |
| kaud-1-T1 | flac-8 | 40 | 0 | 0 |
| kaud-1-T1 | flac-8-nometa | 40 | 0 | 0 |
| kaud-1-T1 | wavpack-hh | 40 | 0 | 0 |
| kaud-1-T1 | wavpack-hhx6 | 40 | 0 | 0 |
| kaud-1-T1 | kolmo-balanced-typedhuf-T1 | 40 | 0 | 0 |
| kaud-1-T1 | kolmo-ratio-T1 | 40 | 0 | 0 |
| kaud-1-T1 | kolmo-x-l2-T1 | 40 | 0 | 0 |

### music (4 files, 146103320 bytes)

| tool | bytes | vs flac -8 | ratio | enc MB/s wall | enc MB/s CPU | dec MB/s wall | dec MB/s CPU |
|---|---:|---:|---:|---:|---:|---:|---:|
| flac-5 | 77088054 | +0.85% | 0.5276 | 75.85 | 77.20 | 142.00 | 144.59 |
| flac-8 | 76437465 | +0.00% | 0.5232 | 31.49 | 31.66 | 131.98 | 134.99 |
| flac-8-nometa | 76403171 | -0.04% | 0.5229 | 33.35 | 33.62 | 131.71 | 135.71 |
| wavpack-hh | 76308678 | -0.17% | 0.5223 | 26.36 | 26.71 | 32.61 | 32.92 |
| wavpack-hhx6 | 75302732 | -1.48% | 0.5154 | 0.46 | 0.46 | 35.38 | 35.76 |
| kolmo-balanced-typedhuf-T1 | 96238179 | +25.90% | 0.6587 | 159.64 | 162.26 | 423.31 | 425.01 |
| kolmo-ratio-T1 | 88516799 | +15.80% | 0.6059 | 1.12 | 1.13 | 36.61 | 37.13 |
| kolmo-x-l2-T1 | 86684475 | +13.41% | 0.5933 | 0.14 | 0.15 | 0.28 | 0.28 |
| kolmo-ratio-T4 | 88516799 | +15.80% | 0.6059 | 1.13 | 1.14 | 37.45 | 37.95 |
| kaud-0-T1 | 76015424 | -0.55% | 0.5203 | 31.96 | 32.13 | 80.26 | 81.31 |
| kaud-0-T4 | 76015424 | -0.55% | 0.5203 | 100.15 | 31.29 | 217.67 | 76.41 |
| kaud-1-T1 | 72307114 | -5.40% | 0.4949 | 6.60 | 6.64 | 7.65 | 7.72 |
| kaud-1-T4 | 72307114 | -5.40% | 0.4949 | 25.70 | 6.94 | 28.03 | 7.67 |

| kaud setting | rival | kaud smaller | kaud larger | equal |
|---|---|---:|---:|---:|
| kaud-0-T1 | flac-5 | 4 | 0 | 0 |
| kaud-0-T1 | flac-8 | 4 | 0 | 0 |
| kaud-0-T1 | flac-8-nometa | 4 | 0 | 0 |
| kaud-0-T1 | wavpack-hh | 2 | 2 | 0 |
| kaud-0-T1 | wavpack-hhx6 | 0 | 4 | 0 |
| kaud-0-T1 | kolmo-balanced-typedhuf-T1 | 4 | 0 | 0 |
| kaud-0-T1 | kolmo-ratio-T1 | 4 | 0 | 0 |
| kaud-0-T1 | kolmo-x-l2-T1 | 4 | 0 | 0 |
| kaud-1-T1 | flac-5 | 4 | 0 | 0 |
| kaud-1-T1 | flac-8 | 4 | 0 | 0 |
| kaud-1-T1 | flac-8-nometa | 4 | 0 | 0 |
| kaud-1-T1 | wavpack-hh | 4 | 0 | 0 |
| kaud-1-T1 | wavpack-hhx6 | 4 | 0 | 0 |
| kaud-1-T1 | kolmo-balanced-typedhuf-T1 | 4 | 0 | 0 |
| kaud-1-T1 | kolmo-ratio-T1 | 4 | 0 | 0 |
| kaud-1-T1 | kolmo-x-l2-T1 | 4 | 0 | 0 |

| file | flac-8 | wavpack-hhx6 | kolmo-ratio-T1 | kolmo-x-l2-T1 | kaud-0-T1 | kaud-1-T1 | kaud -1 vs flac -8 |
|---|---:|---:|---:|---:|---:|---:|---:|
| arrows_voyage_07 | 11779324 | 11685832 | 12870894 | 12433390 | 11742569 | 11307971 | -4.0% |
| dso_shostakovich5_mvt4 | 23402225 | 22937552 | 29527410 | 29527410 | 23361167 | 21912916 | -6.4% |
| nin_ghosts_07 | 10225248 | 10103818 | 12316288 | 12155329 | 10178692 | 9665639 | -5.5% |
| revivalists_amber_live | 31030668 | 30575530 | 33802207 | 32568346 | 30732996 | 29420588 | -5.2% |


## What did not work, and other notes

Sizes in this section are from in-process `-b` runs on two development subsets: "music-30" (the first
10 MB of `nin_ghosts_07`, `dso_shostakovich5_mvt4` and `revivalists_amber_live`) and "esc-10" (every fourth ESC
clip). Lower is better; percentages are relative to the line before unless noted.

**Profile 0.**

- First version (fixed 4096-sample frames, exact stereo choice by coding all four signals, int64 overflow checks):
  ESC 6.88 MB, music 76.59 MB, 32 MB/s encode and 57 MB/s decode. Already 7.6% under `flac -8` on ESC but 0.2% over
  it on music: `nin_ghosts_07` (transients) was 2.8% larger than FLAC. Least squares over a long frame is dominated
  by the loudest stretch; smaller frames fixed it (1024 samples: -2.8% on that track), so frames became split trees.
- The double-precision autocorrelation row was a serial reduction (no `-ffast-math`), and the Cholesky factorisation
  was repeated for each candidate: these, not the LPC filtering, were most of the encoder time. Exact integer
  products in doubles with an OpenMP SIMD reduction, summing children's rows for parents, caching the factorisation
  and computing coefficients only for the candidate orders took effort 1 from 22 to 39 MB/s on music with identical
  output.
- An int64 check that every predictor sum fits in 32 bits cost as much as the residual itself. It is not needed:
  wrapping arithmetic is identical on both sides, so only the residual bound is checked.
- Estimating the stereo pair from a second-difference heuristic (effort 0 of the first version) was about as good
  as the exact choice on ESC and 0.05% worse on music; the final encoder estimates it from the least-squares errors
  it computes anyway.

**Profile 1.**

- First cascade (LPC order 32 on profile 0's split blocks, NLMS 256/32/16 with int64 dot products, sign-sign LMS,
  bias canceller, a 5-level binary tree for the bit length with contexts from recent magnitudes): only 0.5% under
  profile 0 on ESC at 3.7 MB/s. The range coder alone (filters off) was *worse* than profile 0's partitioned Rice
  coding on ESC (1.728 vs 1.721 MB on esc-10): a backward-adaptive model cannot see the onsets that a forward
  Rice parameter sees.
- LPC coefficients were first sent as Elias-gamma bypass bits: 0.29 bits per sample of header on ESC. Rice coding
  them as in profile 0: esc-10 1.728 -> 1.701 MB, music-30 14.135 -> 13.998 MB.
- Sending a per-partition magnitude parameter k and using it as the main context made the coder no worse than Rice;
  ranking the bit length around k (one or two decisions instead of five) made it faster. The coder is now within
  0.7% (orchestra) to 2.7% (NIN) of a non-causal oracle that knows the local mean |r| over 32 samples.
- The bias canceller (a context-dependent running mean subtracted from the residual) made files 0.1% larger:
  removed. A 0.5/0.5 mix of a fast (shift 4) and the slow probability: +0.3%, removed. A third context dimension
  (bit length of the previous residual): no change, removed. Allowing up to 256 partitions per frame instead of 32:
  +0.1%, reverted.
- NLMS step sizes were far too small at first (mu = 8/4096 for 256 taps). Larger steps helped steadily up to about
  256/4096; the int16 step then saturated in quiet passages, which an exponent on the step fixed (esc-10 -0.4%).
- Longer stage-0 filters (512, 1024 taps) were worse at every step size tried, on these 2^18-sample chunks.
- The biggest single gains were structural: profile 0's block splitting *hurts* the cascade (each LPC change
  disturbs the adaptive filters downstream). No splitting (-0.6%), longer LPC frames (8192: -0.4%, 16384: -0.3%,
  32768: -0.1%), and a lower LPC order (16 instead of 32: -0.15%). Per-frame stereo switching hurt for the same
  reason; one stereo pair per chunk (chosen from summed least-squares estimates) beat per-frame choice by 0.1%.
- Cross-channel taps in stage 0 (32, of the other slot's LPC residual, including the current sample for the
  second slot): -0.4% on music-30.
- The 16-tap sign-sign LMS stage as a plain cascade stage gave nothing (0.01%); combining all stage predictions
  with weights adapted by sign-sign LMS gave -0.3%, and then larger NLMS steps another -0.2%.
- Speed: the first cascade spent its time in a serial int64 dot product and update, a 64-bit division per stage
  and sample, a `memmove` per sample in the sign-sign stage and bit-by-bit bypass coding. int16 prediction weights,
  the reciprocal table, sliding buffers, the `vpmulhrsw` update and a separate raw bit stream for the low mantissa
  bits took it from 3.7 to about 9-12 MB/s per thread with the same size.

**Not done.** Monkey's Audio and TTA could not be measured (not packaged here; building them from source was
outside the "packaged" rule). FLAC 1.5's `-j` threads and WavPack 5.7's threads are not in the packaged versions.
Formats other than 16-bit mono/stereo are refused. Profile 0's decoder could gain from order-specialised
reconstruction with two interleaved accumulators; profile 1 is single-path per channel and its speed is bounded
by the 256-tap update. The stream has no per-frame sync or seek table: random access is per chunk.
