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

RESULTS_PLACEHOLDER

## What did not work, and other notes

DIARY_PLACEHOLDER
