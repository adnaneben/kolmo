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
| 5 | BWT (`ratio` text, rc2) | libbsc block: LZP 15/72 + BWT (libsais) + static QLFC | own C port of libbsc 3.4.0 (rc3; byte-identical to libbsc, 2,826 comparisons) | ratio vs xz -9 general −12.7% (rc2) | read only from rc3 (rc2 frames decode); written by codec 18 instead |
| 6 | LZMA2 (`ratio` binary) | liblzma preset 9e, raw LZMA2 | system / Homebrew liblzma | — | in; vendoring in rc3 (deps lane) |
| 7 | context mixing (`-x`) | orders 1–7, word, column, indirect contexts, bit histories, neural mixer, APMs; squash/stretch and coder rewritten from the mathematics (`1641a08`), exact logistic tables (`d5abd6e`, −0.019% over 82 files, none larger) | own (lpaq1-derived parts rewritten) | `-x` vs zpaq -m5: general −7.4%, structured −45.3%, weights −6.8%, FASTQ −24% | in |
| 8–11 | frontier DNA, DNA prefix, PCM, DNA runs | experimental frontier codecs | own (Codex) | see `research/TRACKER.md` | research (opt-in build) |
| 12 | graph block | integer/decimal graph: predictors, zig-zag residuals in byte planes, per-leaf coder choice | own | the main source of the numeric and table gains | in |
| 13 | Huffman leaf | Huffman only | zstd's HUF | — | in |
| 14 | rANS leaf | rANS order 0 | own | — | in |
| 15 | NPLANE native leaf | plane-context Huffman/rANS for integer residuals | own | −5% to −12% on sensor, ECG, numeric columns; 1.5–2.6× slower decode | opt-in (`--native-leaf`) |
| 16 | FASTQ family | kfam: layout, names, bases, exceptions, qualities, each with its own model; own binary arithmetic coder; parallel chunks | own | 3 FASTQ runs: −25.3% vs rc2 `-x`, −34.3% vs CRAM 3.1; 3.4× CRAM's compress speed, 0.25× its decode | in (at `-x`) |
| 17 | CM image | context mixing with the image model | own | 16-bit CT: −0.9% vs JPEG-LS; MR +5.9% vs JPEG XL e9 | in |
| 18 | BWT (`ratio` text, `ultra` alternative) | LZP 17/96 + BWT (libsais subset, pthread crew) + adaptive QLFC, coder blocks ≥ 4 MiB, no libbsc header or adler32s; LZP, sort, QLFC and inverse threaded within a block | own (libbsc and libsais origins, Apache-2.0, kept) | BWT stage alone on 451 MB: −1.29% vs codec 5 (adaptive −1.11%, LZP −0.05%, coder blocks −0.13%); decode about 1.5× slower per thread, faster with the block's threads; `ratio` details in `benchmark/rc3/bwt/RESULTS.md` | rc3 (bwt lane) |
| 19 | audio | kaud: quantised LPC + partitioned Rice (profile 0); NLMS/sign-sign LMS cascade + binary range coder (profile 1) | own | 40 ESC-50 clips: −4.0% (p0) and −8.2% (p1) vs FLAC -8; p1 −6.7% vs WavPack -hhx6; decodes slower than FLAC | rc3 (format2 lane) |
| 20 | video | kvid: temporal prediction front end, then Kolmo | own | 3 CIF clips, all threads: −14.0% vs FFV1 (10 threads, 16 slices) at 0.94× compress and 2.1× decompress; `ratio` −16% vs xz -9 | rc3 (format2 lane) |
| 21–23 | stacking leaves | per-plane leaves, Huffman→zstd leaf, zero-run leaf | own | ST-1: −0.67% to −2.0% vs production on integer files (fresh data) | rc3 (format2 lane) |
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
| plane-aligned Huffman/rANS chunks, singleton rANS chunks (ST-3, P-07/17/18) | aligned entropy chunks for integer leaves | 0% on the benchmark files against rc2's own leaf competition (gains were against weaker baselines) | opt-in (rc2); format2 lane re-measures under format 2 |
| one-symbol rANS chunk as a fill (R4) | decoder shortcut | 0.88–0.91× decode time on the one file with such a chunk | in |
| `--typed-huf` | per-plane Huffman, coded type first; WAV clips with long repeats get zstd-3 leaves | small NPY/WAV under 8 MiB | opt-in |
| Huffman→zstd per plane (S1 H->Z) | nested leaf | larger than `-m hc` on every typed array (stack audit) | format2 lane re-measures |
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
| libsais OpenMP regions on a pthread crew (spin then sleep) | BWT encode and inverse inside a block | enwik8 32 MB, 4 threads: sort 27 → 58 MB/s, inverse 86 → 355 MB/s (rc2 used one thread per block); equal to OpenMP's active wait, far better under load | rc3 (bwt lane) |
| block-parallel LZP and QLFC with serial-capacity replay | codec 5 and 18 encode and decode | output independent of threads, byte-identical to libbsc's serial result | rc3 (bwt lane) |
| `ratio`: slice test skips the full LZMA2 pass when BWT is ≤ 90% on the slice | max/ratio binary blocks over 8 MiB | Silesia osdb: compress 6.8 s → 0.8 s, −3.1% | rc3 (bwt lane) |
| AVX2 copy of libsais with runtime dispatch | BWT encode | baseline SSE2 build ~7% slower forward, equal inverse (edge of VM noise) | not done; candidate for the SIMD lane |
| hand-written assembly | — | below a few percent where measured so far (SIMD-ASM.md) | rc3 simd lanes: kept wherever it gains |
