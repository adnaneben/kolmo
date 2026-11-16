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
  the row and column index bits by one each). This alone took the AVX2 order-1 decoder from 330 to 920 MB/s.
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

<<RESULTS>>
