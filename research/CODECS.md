# Codec and technique ledger

Every codec, coder, transform and speed technique developed or evaluated for Kolmo: the algorithm behind it, its
measured impact, and the verdict. Lanes add or update rows; nothing is removed, so a rejected idea keeps its
reason. Sizes are total bytes against the named baseline (negative = smaller). Measurements are on observed
benchmark data unless marked otherwise. Sources: the rc2 report, round 3 and 4 reports, `research/STACK-AUDIT.md`,
`research/TRACKER.md`, `benchmark/rc2/results/mt/MT.md`, `benchmark/rc2/SIMD-ASM.md`.

Verdicts: **in** (default in the release), **opt-in** (in the release behind an option), **rc3** (being integrated for
rc3), **branch** (exists, not merged), **dropped** (measured and rejected), **research** (kept as a research tool only).

## Block codecs (container codec ids)

| id | codec | backend / algorithm | own or third-party | measured impact | verdict |
|---|---|---|---|---|---|
| 0 | store | copy | own | — | in |
| 1 | LZ4 (`speed`) | LZ4 fast, acceleration 1 | vendored LZ4 1.10.0 | `speed` vs lz4 -1: ±0.1% size, 1.2–1.45× compress, 1.3–2.3× decompress multithreaded | in |
| 2 | LZ4HC | LZ4 high compression | vendored | — | in |
| 3 | zstd (`balanced`, `hc`) | zstd level 3 / 19 / 22 for text | vendored zstd 1.5.7 | balanced vs zstd -3 with all threads: general −8.7%, structured −57.5%, numeric −36.7%, weights −12.8% | in |
| 4 | Brotli (`eco`) | Brotli quality 8, window 2^24 | vendored Brotli 1.1.0 | eco vs brotli -11: −2.5% to −47% at 25–86× compress speed | in |
| 5 | BWT (`ratio` text) | libbsc: LZP + BWT (libsais) + static QLFC | vendored libbsc 3.4.0 (C++) | ratio vs xz -9 general −12.7% | in; replaced by codec 18 in rc3 |
| 6 | LZMA2 (`ratio` binary) | liblzma preset 9e, raw LZMA2 | system / Homebrew liblzma | — | in; vendoring in rc3 (deps lane) |
| 7 | context mixing (`-x`) | orders 1–7, word, column, indirect contexts, bit histories, neural mixer, APMs; squash/stretch and coder rewritten from the mathematics (`1641a08`), exact logistic tables (`d5abd6e`, −0.019% over 82 files, none larger) | own (lpaq1-derived parts rewritten) | `-x` vs zpaq -m5: general −7.4%, structured −45.3%, weights −6.8%, FASTQ −24% | in |
| 8–11 | frontier DNA, DNA prefix, PCM, DNA runs | experimental frontier codecs | own (Codex) | see `research/TRACKER.md` | research (opt-in build) |
| 12 | graph block | integer/decimal graph: predictors, zig-zag residuals in byte planes, per-leaf coder choice | own | the main source of the numeric and table gains | in |
| 13 | Huffman leaf | Huffman only | zstd's HUF | — | in |
| 14 | rANS leaf | rANS order 0 | own | — | in |
| 15 | NPLANE native leaf | plane-context Huffman/rANS for integer residuals | own | −5% to −12% on sensor, ECG, numeric columns; 1.5–2.6× slower decode | opt-in (`--native-leaf`) |
| 16 | FASTQ family | kfam: layout, names, bases, exceptions, qualities, each with its own model; own binary arithmetic coder; parallel chunks | own | 3 FASTQ runs: −25.3% vs rc2 `-x`, −34.3% vs CRAM 3.1; 3.4× CRAM's compress speed, 0.25× its decode | in (at `-x`) |
| 17 | CM image | context mixing with the image model | own | 16-bit CT: −0.9% vs JPEG-LS; MR +5.9% vs JPEG XL e9 | in |
| 18 | BWT in C | libbsc ported to C, libsais subset owned, adaptive QLFC, threaded unBWT | own (Apache-2.0 origins kept) | pending | rc3 (bwt lane) |
| 19 | audio | kaud: quantised LPC + partitioned Rice (profile 0); NLMS/sign-sign LMS cascade + binary range coder (profile 1). Block codec for 16-bit PCM WAV, profile 0 up to `hc`, profile 1 at `max`/`ultra`, kept when smaller than the block coded otherwise | own (`src/codecs/audio.c`, ported from `src/experimental/kaud.c`) | 40 ESC-50 clips: −4.0% (p0) and −8.2% (p1) vs FLAC -8; p1 −6.7% vs WavPack -hhx6. **Against rc2's own modes** (43 WAV files: 40 clips, the clips joined mono and as stereo pairs, an odd-sized WAV): fast −31.1%, eco −23.1%, hc −22.2%, max −23.4%, every file smaller in every mode; decodes slower than rc2 on WAV (profile 1 ~9 MB/s) | **in rc3** (format 2, version 11) |
| 20 | video | kvid: per-strip MED / temporal / temporal+MED prediction, residuals by plane, the layout coded by a nested Kolmo frame (all modes but `rt`) | own (`src/codecs/video.c`) | 3 CIF clips, all threads: −14.0% vs FFV1 (10 threads, 16 slices) at 0.94× compress and 2.1× decompress. **Against rc2's own modes** (akiyo, foreman, mobile): fast −17.4%, eco −12.9%, hc −15.5%, max −11.1%, every clip smaller in every mode; `max` decodes faster | **in rc3** (format 2, version 11) |
| 21 | per-plane leaf | one sub-leaf per byte plane of an INT node's residuals, each with its own codec (block codec, Huffman, rANS, 22, 23, stored), the sparse guard per plane (ST-1, ST-5, ST-8) | own (`src/codecs/planes_leaf.c`) | alone vs rc2 on the 26-file set: 3 files smaller at fast/eco/hc, 1 at max, none larger (−3,230 B at fast); with 22 and 23: log_timestamps −19% to −24%, year_i32 −11% to −34% (`benchmark/rc3/format2/RESULTS.md`) | **in rc3** (graph leaves, version 11) |
| 22 | Huffman then zstd leaf | codec-13 Huffman stream coded again by zstd (S1/S2 H->Z); zstd 19 directly up to 64 KiB, larger streams only when a cheap probe finds repeats | own (`src/codecs/hufzstd_leaf.c`) | alone vs rc2: 4 files smaller at fast (−40,165 B: MiniLM −0.05%, year_i32, OWID float arrays), 2-3 at eco/hc, 1 at max; none larger | **in rc3** (graph leaves, version 11) |
| 23 | zero-run leaf | zero-run lengths and non-zero literals as two sub-streams, each with the best sub-codec (P2-05) | own (`src/codecs/zrun_leaf.c`, `src/algo/zerorun.c`) | alone vs rc2: 6 files smaller at fast/eco/hc, 4 at max, none larger (−2,039 to −4,130 B per mode on the set) | **in rc3** (graph leaves, version 11) |
| 24 | own LZMA-class coder | — | own | pending | rc3 (deps lane), only if it gains |
| 25 | parallel CM | stream- or block-parallel context mixing | own | pending | rc3 (xmode lane) |

## Transforms and graph techniques

| technique | what it does | measured impact | verdict |
|---|---|---|---|
| table transform (`tbl2`) | typed CSV/TSV columns: delta, entity, delta-of-delta, third-order, formula predictors | structured −57.5% vs zstd -3 (with `balanced`) | in |
| DEC64 dictionary (R1) | few-distinct-value decimal columns as indices plus a value list | massdata −5.7% balanced, −6.4% ratio; ozl −1.5% to −4.2% at hc/ratio | in |
| log templates | template mining, typed fields | part of the structured result | in |
| lines, sci (decimal doubles), word codes, JSON split, fixed records, x86 branch transform, tar members, 16-bit images | structure transforms | per-type gains, rc1/rc2 reports | in |
| integer graph fast forward path | one-sweep order choice, no division at factor 1 | faster encode, same bytes | in (rc2) |
| plane-major residuals (S1 joined planes) | residuals as joined byte planes | −16.07% vs interleaved (S1) | in |
| sparse-leaf guard (part of ST-1) | Huffman-only shortcut guard in `kg_serialize` | in `18d7efb` | in |
| plane-aligned Huffman/rANS chunks, singleton rANS chunks (ST-3, P-07/17/18) | aligned entropy chunks for integer leaves | rc3 set, alone (`-DKOLMO_ALIGNED_LEAVES=1`): log_timestamps −17% to −21% in fast/eco/hc/max, no other file changed; on top of codecs 21-23 still 13 B smaller at `hc`, 0 elsewhere | **in rc3**: default at `hc` (no format change); opt-in elsewhere as in rc2 |
| one-symbol rANS chunk as a fill (R4) | decoder shortcut | 0.88–0.91× decode time on the one file with such a chunk | in |
| `--typed-huf` | per-plane Huffman, coded type first; WAV clips with long repeats get zstd-3 leaves | small NPY/WAV under 8 MiB | opt-in |
| Huffman→zstd per plane (S1 H->Z) | nested leaf | as a leaf competing with rc2's leaf rule (codec 22): year_i32 −22% at fast, MiniLM −0.05% at fast (stack audit: the standalone S1 pipeline was larger than `-m hc`) | **in rc3** (codec 22) |
| Huffman→LZ4 (S1 H->L) | nested leaf | LZ4-HC in place of zstd inside codec 22 (encoder-only build), 26-file set, four modes: larger than H->Z on all 9 file/mode pairs where either applies (+0.003% to +23%), smaller on none | not in rc3 (never smaller than codec 22) |
| per-plane residual leaves with the sparse guard (ST-1 = P-01 + P-03, ST-5 at hc, ST-8 small messages) | one leaf per byte plane, own codec each | codec 21 with 22 and 23, against rc2's modes: log_timestamps −19% to −25%, population −0.7% to −2.6%, year_i32 −11% to −34% | **in rc3** (codec 21) |
| ST-2 context gate (decode-cost gate for per-plane leaves) | gate by INT order / width | not needed: a per-plane leaf decodes at its sub-codecs' speed; set decode time 0.48 → 0.41 s at fast, 4.9 → 5.1 s at max (noise) | not in rc3 (no cost to gate) |
| ST-4 per-plane codec choice per file | selector over coders and packing | realised per leaf and per plane by codec 21 (the choice is charged: each plane keeps the smallest coding) | **in rc3** (codec 21) |
| zero-run residual leaf (P2-05) | runs + literals | codec 23 | **in rc3** (codec 23) |
| packed residual representations (P-04, P-05, P-06, P-10, P-11, P-12) | bounded-width / patched bit packing per 128 values | exact packed and patched sizes of all 68 integer-residual leaves of the set in fast/eco/hc/max: never smaller than the leaf rc2 already writes; closest +2.8% (ECG plane at eco) | not in rc3 (measured: no leaf gains) |
| type-first CSV columns, wide-table layout (P2-06, P2-07) | research containers | −25% to −88% vs zstd -3 only; rc2 `eco`/`fast` already smaller than the production-format alternatives (stack audit §3, P2-D1) | not in rc3 (research containers closed; no gain against rc2's modes) |
| order-1 entropy leaf (P2-19) | static order-1 coder for leaves | parity with OpenZL-9 at best on GHCN (its own review); krans order 1: −20–25% vs Huffman leaves at 0.35–0.47× decode, not yet measured against rc2's leaf rule | not in rc3 from this lane (krans is a candidate leaf coder of the simd lanes; no codec id reserved) |
| kfam byte model as a graph leaf (stack audit S-A) | adaptive order 0-3 binary coder on integer residual leaves | −4.1% on that leaf class, −0.034% over 44 files vs `max` leaves; `-x -l 2` CM leaves 8.8% smaller still; decodes at ~1-2 MB/s | not in rc3: no codec id was reserved for it and `-x -l 2` already covers the class; open for a mode between `max` and `ultra` |
| P2-E25 CSV picker | research pipeline | +55–58% vs `-m eco` on 3 of 4 CSVs | dropped |
| `tp_pent` per-plane rANS frame | research container | larger than `-m max` on all 9 typed arrays | dropped |
| S5 per-leaf LZMA2 contexts + block-sorted leaves | stack at ratio | −0.2% to −0.5% on large files at 3× encode time | dropped |
| S13 image model on DEC64 integers | — | no candidate file has integers to give it | dropped |
| integer residual leaves → kfam coder | stack | −4.1% on that class, −0.03% overall; `-x -l 2` CM leaves smaller still | dropped (CM leaves cover it) |
| typed-huf planes → kfam coder | stack | −23.8% on that class but at CM speed | research |

## Research codecs (standalone tools)

| tool | algorithm | best result | verdict |
|---|---|---|---|
| KRC1 (Codex) | residual codec, CPU and Metal GPU | larger than zstd -19 on every group; GPU hybrid no faster than CPU (≤1.16× earlier) | research |
| KBC1 (Codex) | bitwise context mixing on KRC1 | +53% vs zstd -19 on general data | research |
| KBX3 (Codex) | KBC1 with longer hashed contexts | +24% vs zstd -19 general; −10% on numeric | research |
| kolmo-fastq prototype (Codex) | four-line FASTQ split | 58% larger than zstd -3 | dropped (superseded by kfam) |
| kfam CSV and one-stream families | family streams | loses to the table transform on CSV; +9.9% vs `-x` on chr22 FASTA | research |
| kbin | pcodec-style binning, rANS bin indices, offset bits, LPC up to order 8 | beats pcodec 8/12 on all 18 files; ECG −4.4% vs `ratio` at 38× compress speed; equal to `balanced` over all NPY | candidate numeric node (rc3 decision) |
| krans | 32-way interleaved static rANS, order 0/1, AVX2 | order 0: −0.4% to −0.8% vs HUF leaves, 1.19–1.26× faster decode on x86; order 1: −20–25% at 0.35–0.47× decode; NEON slower than scalar on M5 | candidate leaf coder on x86 (simd lanes) |

## Speed techniques

| technique | where | measured impact | verdict |
|---|---|---|---|
| x86 runtime dispatch (AVX2 decimal formatter, SSE4.2 CRC32C, XXH3 AVX2) | decode | numeric CSV decode +19–23% on generic x86 builds | in (rc2) |
| SSE2/AVX2 prefix-sum integer inverse | decode of 32-bit arrays | loop 1.8×, end to end +18% | in (rc2) |
| `-ffp-contract=off` everywhere | build | Brotli encoder output no longer depends on FMA | in (rc2) |
| Metal GPU (KRC hybrid) | KRC | no faster than the CPU path | research |
| hand-written assembly | — | below a few percent where measured so far (SIMD-ASM.md) | rc3 simd lanes: kept wherever it gains |
