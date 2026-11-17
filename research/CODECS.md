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
| 19 | audio | kaud: quantised LPC + partitioned Rice (profile 0); NLMS/sign-sign LMS cascade + binary range coder (profile 1) | own | 40 ESC-50 clips: −4.0% (p0) and −8.2% (p1) vs FLAC -8; p1 −6.7% vs WavPack -hhx6; decodes slower than FLAC | rc3 (format2 lane) |
| 20 | video | kvid: temporal prediction front end, then Kolmo | own | 3 CIF clips, all threads: −14.0% vs FFV1 (10 threads, 16 slices) at 0.94× compress and 2.1× decompress; `ratio` −16% vs xz -9 | rc3 (format2 lane) |
| 21–23 | stacking leaves | per-plane leaves, Huffman→zstd leaf, zero-run leaf | own | ST-1: −0.67% to −2.0% vs production on integer files (fresh data) | rc3 (format2 lane) |
| 24 | own LZMA-class coder | — | own | pending | rc3 (deps lane), only if it gains |
| 25 | CM over parts | parts each coded by their own context-mixing model (codec 7 streams), in parallel for encoding and decoding; parts from the table's columns (transpose plan) or two halves of a non-text stream ≥ 32 MiB; cuts depend on the input only; container version 11 | own | columns: CSV 1 MB −0.5%, encode 2.1× and decode 4.4× faster at 4 threads; halves: +0.14% on 100 MB of binary data, 2× faster both ways; 5 MB halves +0.14% to +2.3% (Silesia osdb), hence from 32 MiB only (`benchmark/rc3/xmode/RESULTS.md`) | rc3 (xmode lane) |

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
| hand-written assembly | — | below a few percent where measured so far (SIMD-ASM.md) | rc3 simd lanes: kept wherever it gains |
| concurrent ultra trials (`core/trials.c`) | `-x` encode | LZMA2 / block sorting, their slice test, plain block sorting and the FASTQ family coder run beside CM when the block has threads to spare; the planner's two CM samples in parallel; frames unchanged | rc3 (xmode) |
| FASTQ: family coder first at `-x` | `-x` encode | CM and the other trials skipped when codec 16 applies (blocks ≥ 64 KiB); frames unchanged; ~20× faster, 1.4–5.4 GiB → 30–150 MiB | rc3 (xmode) |
| CM: prefetch of the next bit's mixer and APM rows | `-x` both ways | 20–25% faster on 1 MB inputs (VM); frames unchanged | rc3 (xmode) |
| CM: AVX2 build of the coding loops, runtime dispatch | `-x` both ways, baseline-ISA builds | 21% faster than the SSE2 build; frames unchanged | rc3 (xmode) |
| CM: hand-written AVX2 mixer dot product and update | `-x` | 2% over the compiler's AVX2 code (noise): the mixer is 11% of the instructions, counter and state-map updates 45% | dropped (xmode) |
| CM: byte-ahead prefetch of hashed buckets (both candidates) | `-x` | no gain | dropped (xmode) |
| CM: block-parallel text (2/4/8 parts) | `-x` | +2.8% / +6.4% / +10.9% on 10 MB text, +8.7% for 4 parts on 100 MB | dropped (xmode) |
| CM: block-parallel binary, 4 or 8 parts | `-x` | +0.90% / +1.3% on 100 MB (3.7× faster) — over the 0.5% budget | dropped (xmode); 2 halves kept |
| family coder: name/quality chunks of block/16 (1–4 MiB) | `-x` FASTQ decode | 10 MB: 1.4× faster decode at 4 threads, +0.57%; 100 MB unchanged (4 MiB); smaller base chunks +13% | rc3 (xmode) |
| CM tables sized to the block (`--x-tables=scaled`, or a cap `16..22`) | `-x` memory | scaled: CSV +0.0–0.2%, text +0.1–5.1%, binary +0.3–2.4%, decode memory 5.4–5.9 GiB → 125–580 MiB (2.0 GiB for 100 MB of text); cap 2^20 on 10 MB text +0.67% at 1.4 GiB | opt-in until the memory model is approved |
