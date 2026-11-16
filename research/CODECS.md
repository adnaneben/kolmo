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
| 25 | parallel CM | stream- or block-parallel context mixing | own | pending | rc3 (xmode lane) |

## Transforms and graph techniques

| technique | what it does | measured impact | verdict |
|---|---|---|---|
| table transform (`tbl2`) | typed CSV/TSV columns: delta, entity, delta-of-delta, third-order, formula predictors | structured −57.5% vs zstd -3 (with `balanced`) | in |
| table transform on long-form tables (session `nice-faraday`, ported to rc3) | (1) table blocks may grow by block/8 + 16 KiB before coding (container version 11; was 16 KiB, which turned the transform off on every block ≥ ~400 KiB of short numbers); (2) a group column only has to take more than one value across the sample (long groups were missed) | countries-aggregated.csv (5.5 MB, daily counts per country): `balanced` 4.16× → 15.94×, `hc` 7.26× → 16.83×, `ratio` 5.18× → 17.85×, `-x` 15.68× → 18.87× (xz -9e 8.42×); 50 other file/mode pairs on the Mac unchanged; 407 of 451 unchanged and 44 smaller in the session's own run | rc3 |
| more derived-column formulas (from the 6-bit study's GR6+) | running totals `lag(t)+a`, copy `a`, product `a·b/10^k`, kept when ≥ 50% of sampled rows hit within ±2 in the last digit, used per row where recent cost is lowest | +4.6% / +7.3% on OWID on top of Kolmo in the study; GR6+ is 8% smaller than `-x` on OWID and 3% smaller on covid (after the fix above) | rc3 candidate (not yet built in Kolmo) |
| warm start for block-parallel CM (from the 6-bit study) | every block starts from the model trained on block 0 | 16 blocks: size cost +2% instead of +7–14% | passed to the xmode lane |
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
| krans | 32-way interleaved static rANS, order 0/1, AVX2 | order 0: −0.4% to −0.8% vs HUF leaves, 1.19–1.26× faster decode on x86; order 1: −20–25% at 0.35–0.47× decode; NEON on M5 (simd-arm): order 0 0.43× scalar at 8 ways, 1.2–1.3× at 16–32 ways; order 1 0.38–0.53× | candidate leaf coder on x86 (simd lanes) |
| 6-bit alphabet K6 (fixed, adaptive, token tables; session `dazzling-maxwell`, `research/sixbit-study/`) | text and CSV in 6-bit units with escape codes | 1.23–2.05× alone: beaten by plain Huffman on every file and it hurts any coder that follows | dropped as a storage format |
| GR6 / GR6+ (same study) | DITTO, rank, graph and dictionary ops in 6-bit units, column predictors and formulas, then fixed, Huffman or context-mixing coder | GR6+ CM mode: beats xz -9e, zstd -19, brotli -11 on all 7 files; beats `-x` on OWID (+8%) and covid; loses to `-x` by 4–19% on logs, text and small tables; 1–3 MB/s both ways | research (ideas taken: formulas, warm start, previous-field contexts) |
| bit-level neural coder over 6-bit units (same study) | 6 binary decisions per unit instead of 8 | about 40% faster for 1–3% more size | research |

## Speed techniques

| technique | where | measured impact | verdict |
|---|---|---|---|
| x86 runtime dispatch (AVX2 decimal formatter, SSE4.2 CRC32C, XXH3 AVX2) | decode | numeric CSV decode +19–23% on generic x86 builds | in (rc2) |
| SSE2/AVX2 prefix-sum integer inverse | decode of 32-bit arrays | loop 1.8×, end to end +18% | in (rc2) |
| `-ffp-contract=off` everywhere | build | Brotli encoder output no longer depends on FMA | in (rc2) |
| Metal GPU (KRC hybrid) | KRC | no faster than the CPU path | research |
| hand-written assembly | — | below a few percent where measured so far (SIMD-ASM.md) | rc3 simd lanes: kept wherever it gains |
| NEON word-transform decoder (64-byte classification, token walk by count-trailing-zeros) | decode, word transform (eco, hc) | dickens -m hc decode −36%, -m eco −20% (M5, 1 thread) | in (rc3 simd-arm) |
| NEON byte-plane split/merge with delta (LD2-LD4, UZP/ZIP, prefix sums) | `shuffle` transform | mr -m speed decode −57%, stereo WAV balanced decode −50%, compress −8% | in (rc3 simd-arm) |
| NEON IMG8 median inverse as a 16-row wavefront (skewed 16×16 tiles, transposes) | IMG8 decode | RGB image balanced decode −62%, eco −40%; Y4M decode via IMG8 | in (rc3 simd-arm) |
| NEON IMG8/IMG16 forward residuals, 16-bit SAD in the row-stride search | image compress | 16-bit images balanced compress −41 to −42% (x-ray, img16) | in (rc3 simd-arm) |
| IMG16 median inverse with a 4-op chain (case split on L against min/max(U, UL)), portable C; NEON left predictor | IMG16 decode | mr / 16-bit image balanced decode −24 to −25% | in (rc3 simd-arm) |
| NEON integer-graph inverse, 32-bit, every residual width / order / factor | decode of int32 arrays | balanced −25%, hc −30%, ratio −10% | in (rc3 simd-arm) |
| NEON table-decoder helpers (field mask, number-index left-pack, kind counts) | CSV decode | owid/energy CSV decode −11 to −12% | in (rc3 simd-arm) |
| NEON DEC64 inverse (planes → 64-bit lanes, 2-lane prefix sums, FCVT/FDIV; tagged values in stream order) | decimal-double decode | f64 array balanced −21%, hc −25%, eco −12% | in (rc3 simd-arm) |
| planner image trial: sample loop outside the lag loop (cache) | IMG8 trial in the planner | image compress −3.5% more, y4m −4.5% | in (rc3 simd-arm) |
| kvid NEON strip prediction (encoder costs/residuals, temporal add, intra via the IMG8 wavefront) | kvid (research) | CIF clip -m rt compress −54%, decode −61% | in kvid (rc3 simd-arm) |
| 64-byte function/loop alignment of vendored LZ4 (build flag) | -m speed decode | removes a 21% placement penalty (JSON, logs) seen with a byte-identical lz4.o; at most 5% slower than a fortunate placement on two inputs | in (rc3 simd-arm; integration may revisit) |
| FEAT_CSSC scalar min/max for the IMG16 median chain | IMG16 decode | 7–13% slower than compare-select on M5 | dropped |
| NEON rANS decode for Kolmo's own leaf (4 × 64-bit states) | rANS leaves | not attempted: no 64-bit lane multiply, 4 states only; krans analysis: NEON loses below 16 ways (no gather, lane transfers, horizontal renorm mask in the per-round chain) | stays scalar |
| NEON CM mixer dot product / weight update | `-x` | clang already vectorises them (224 SMLAL in cm_compress_m); mixer ~8% of `-x`, the rest is cache misses in cm_find (48%) | no hand kernel (prefetch: xmode lane) |
| NEON word-transform encoder (run ends by vector compare, memcmp confirm, slot list instead of table scan) | word encode | 5–10% slower (short runs: the vector chain replaces well-predicted branches) | dropped |
