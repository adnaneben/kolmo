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

## Container extensions and archive features (rc3, features-b lane)

Measured on a 4-vCPU Intel Xeon (2.10 GHz) Linux VM; details, commands and the full tables in
`benchmark/rc3/features-b/RESULTS.md`. Formats in `SPEC.md` ("Container extensions").

| technique | what it does | measured impact | verdict |
|---|---|---|---|
| KEXT trailer | tagged, CRC-checked sections after the CZN1 frame; readers that do not know it ignore it | 24 B + 16 B per section | rc3 |
| line-aligned blocks (`KOLMO_T_COLUMNS`) | every block of a text plan ends at a line end, so a row never spans two blocks | part of the column-split cost below | rc3 (with `--index`) |
| column-split table blocks (tflag 13) | the `tbl2` stream of a table block split into a header segment and one coded segment per column, each with its own check | against plain frames with the same 4 MiB blocks: balanced OWID +7.4%, sensor CSV +2.0%; hc +9.3% / −0.3%; max +11.2% / −0.0%. Reading one column decodes 5% of the CSV (OWID) and 8% (sensor) | rc3 (with `--index`) |
| per-column graph tables (one graph leaf per column) | the planner forced to the graph route so each column is a leaf | +58% on OWID | dropped |
| IDX1 row index | per-block line counts and line-boundary flags | rows 1,000,000–1,000,100 of a 268 MB log: 4.2 MB decoded, 0.011 s vs 0.232 s for a scan (T1) | rc3 |
| IDX1 grep filter | per-block bitmap of case-folded trigrams (1 bit) and whole alphanumeric tokens of 3+ characters (4 bits), 1.44 bits per set bit | HDFS log: +3.8% size, a block-id grep rules out 60 of 65 blocks (0.040 s vs 0.571 s scan); sensor CSV +2.4%; JSON lines +27.7% (high-entropy tokens); no gain for frequent words | rc3 (`--no-filter` drops it) |
| filter token bits k | bits per token key | HDFS index overhead k=1 +1.1%, 3 +2.9%, 4 +3.8%, 6 +5.6% | k=4 default |
| DDP1 FastCDC dedup | Gear rolling hash, normalised chunking (4 KiB average default; the 16 KiB numbers below with `--chunk 16`), text cuts moved to the next line end, exact dedup by XXH3-64 + memcmp, unique chunks in one Kolmo frame (typed transforms still apply) | daily CSV snapshots 7.45× (4 KiB: 14.1×, hc 36.1×) vs tar\|zstd -19 --long=31 24.1×, borg 2.8×, restic 2.6×; checkpoints 3.50× vs 3.26× and borg 3.15×; growing logs 69.4× vs 55.7×; source trees 40.6× vs 53.7× (tar\|zstd wins); pythia-70m checkpoints 2.20× vs 2.03× | rc3 (`kolmo a` / `kolmo x`) |
