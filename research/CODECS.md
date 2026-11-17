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
| 6 | LZMA2 (`ratio` binary) | liblzma preset 9e, raw LZMA2 | vendored liblzma subset, XZ Utils 5.8.1 (rc3; was the system / Homebrew liblzma) | rc3 vendoring: same bytes, LZMA2 decode 1.43× faster than the system 5.4.5 (44.7–45.5 → 63.8–64.9 MB/s), Silesia `ratio` decode −8%; no system library needed (`docs/DEPENDENCIES.md`) | in |
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
| 24 | own LZMA-class coder | — | own | not written: liblzma 5.8.1 is already the faster decoder; no LZMA2 parse setting gives 9e's size for less time (depth 64 +0.22% at 1.41×, non-extreme +1.19% at 2.3× on structured data); an infinitely fast decoder could save at most the 24% (Silesia) to 46% (structured) of `ratio` decode left in LZMA2 | dropped (deps lane, rc3) |
| 19 | audio | kaud: quantised LPC + partitioned Rice (profile 0); NLMS/sign-sign LMS cascade + binary range coder (profile 1). Block codec for 16-bit PCM WAV, profile 0 up to `hc`, profile 1 at `max`/`ultra`, kept when smaller than the block coded otherwise | own (`src/codecs/audio.c`, ported from `src/experimental/kaud.c`) | 40 ESC-50 clips: −4.0% (p0) and −8.2% (p1) vs FLAC -8; p1 −6.7% vs WavPack -hhx6. **Against rc2's own modes** (43 WAV files: 40 clips, the clips joined mono and as stereo pairs, an odd-sized WAV): fast −31.1%, eco −23.1%, hc −22.2%, max −23.4%, every file smaller in every mode; decodes slower than rc2 on WAV (profile 1 ~9 MB/s) | **in rc3** (format 2, version 11) |
| 20 | video | kvid: per-strip MED / temporal / temporal+MED prediction, residuals by plane, the layout coded by a nested Kolmo frame (all modes but `rt`) | own (`src/codecs/video.c`) | 3 CIF clips, all threads: −14.0% vs FFV1 (10 threads, 16 slices) at 0.94× compress and 2.1× decompress. **Against rc2's own modes** (akiyo, foreman, mobile): fast −17.4%, eco −12.9%, hc −15.5%, max −11.1%, every clip smaller in every mode; `max` decodes faster | **in rc3** (format 2, version 11) |
| 21 | per-plane leaf | one sub-leaf per byte plane of an INT node's residuals, each with its own codec (block codec, Huffman, rANS, 22, 23, stored), the sparse guard per plane (ST-1, ST-5, ST-8) | own (`src/codecs/planes_leaf.c`) | alone vs rc2 on the 26-file set: 3 files smaller at fast/eco/hc, 1 at max, none larger (−3,230 B at fast); with 22 and 23: log_timestamps −19% to −24%, year_i32 −11% to −34% (`benchmark/rc3/format2/RESULTS.md`) | **in rc3** (graph leaves, version 11) |
| 22 | Huffman then zstd leaf | codec-13 Huffman stream coded again by zstd (S1/S2 H->Z); zstd 19 directly up to 64 KiB, larger streams only when a cheap probe finds repeats | own (`src/codecs/hufzstd_leaf.c`) | alone vs rc2: 4 files smaller at fast (−40,165 B: MiniLM −0.05%, year_i32, OWID float arrays), 2-3 at eco/hc, 1 at max; none larger | **in rc3** (graph leaves, version 11) |
| 23 | zero-run leaf | zero-run lengths and non-zero literals as two sub-streams, each with the best sub-codec (P2-05) | own (`src/codecs/zrun_leaf.c`, `src/algo/zerorun.c`) | alone vs rc2: 6 files smaller at fast/eco/hc, 4 at max, none larger (−2,039 to −4,130 B per mode on the set) | **in rc3** (graph leaves, version 11) |
| 24 | own LZMA-class coder | — | own | pending | rc3 (deps lane), only if it gains |
| 25 | parallel CM | stream- or block-parallel context mixing | own | pending | rc3 (xmode lane) |
| 26 | GR6+ (`-x`, line-structured text) | fields as decisions (repeat, column and graph move-to-front ranks, dictionaries, column predictors with derived-column formulas, shared-prefix text), values as 6-bit units, integer context mixing (8 models, two mixers, APM) with the CM arithmetic coder; tried per ultra text block, kept only when smaller | own (from branch `claude/dazzling-maxwell-p8y4u0`, commit `12f0c0e`) | M5, 1 thread, on top of rc3 `-x`: covid 18.87× → 19.48× (−3.1%), owid_test 12.23× → 12.70× (−3.7%), OWID CO2 12.84× → 12.93× (−0.7%), decode 5–11× faster on those; energy, wide, loghub, dickens, api.json unchanged (not chosen); compression +3% to +30%, +126% on a 130-column table (formula search); other modes byte-identical (`benchmark/rc3/gr6/RESULTS.md`) | rc3 |

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

## Trained profiles and targets (features-a lane, rc3)

Measured on held-out files of 10 dataset families, one thread, against zstd at the same level with a dictionary from
`zstd --train -B16K` on the same sample (`benchmark/rc3/features-a/RESULTS.md`).

| technique | what it does | measured impact | verdict |
|---|---|---|---|
| trained profile (`kolmo train`, `-P`, `KPF1`) | setting, pinned plans, zstd and raw dictionaries, held-out choices; frames carry the profile id (version 11) | `balanced`: messages −2.4% to −11% vs zstd + dict and −64% to −75% vs untrained; Hadoop logs −25% vs zstd + dict, −39% vs untrained; large CSV/JSON/numeric ±0.2% vs untrained | rc3 |
| compact profile frame (`CZC1`) | one-block profile frames ≤ 1 MiB with 14–21 bytes of header instead of 52 | part of the message result (GeoJSON features: 2.95 MB untrained → 0.76 MB with magicless frames) | rc3 |
| raw zstd blocks for profile leaves ≤ 64 KiB | no zstd frame or block header (5–6 bytes) inside the container | GeoJSON features −4.0%, SSH lines −7.3% | rc3 (uses zstd's deprecated block API) |
| dictionary leaf limit (`DLIM`) chosen on held-out samples | dictionaries only on leaves up to 64 KiB … unlimited | a dictionary on every leaf cost +0.7% on CSV; with the limit −0.1% | rc3 |
| pinned plans | the planner's choice per file shape and size band, used when the field count fits | Hadoop logs, no dictionary: −1.2% vs untrained (+13% with a majority vote and no band); OpenStack 256 KiB files compress 2.8× faster | rc3 |
| COVER dictionary trainer (chosen on held-out samples against fastCover) | zstd's COVER with a 2-step parameter search, one thread | SSH lines −7.2%, Hadoop logs −7.0%; rejected on OpenStack lines (+23%) | rc3 (`--quick` skips it) |
| direct path for profile inputs < 4 KiB | skips block planning and scheduling; same bytes | messages 1.6–2.1× faster to compress | rc3 |
| `--target` (floors, `cost:`), per-chunk setting from trial compressions and a calibrated speed table | chooses among 9 settings per 16 MiB chunk | median choice regret 0% vs the best fixed setting; floors met with 0–5% size regret in most cases; cost targets pay 11–76% (median) for their trials | rc3 (trial cost to be reduced) |
| `kolmo calibrate` | measures setup + rate per setting and class | built-in table from the reference VM | rc3 |

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
| GR6 / GR6+ (same study) | DITTO, rank, graph and dictionary ops in 6-bit units, column predictors and formulas, then fixed, Huffman or context-mixing coder | GR6+ CM mode: beats xz -9e, zstd -19, brotli -11 on all 7 files; beats `-x` on OWID (+8%) and covid; loses to `-x` by 4–19% on logs, text and small tables; 1–3 MB/s both ways | research (ideas taken: formulas, warm start, previous-field contexts); GR6+ itself ported as block codec 26 |
| bit-level neural coder over 6-bit units (same study) | 6 binary decisions per unit instead of 8 | about 40% faster for 1–3% more size | research |

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
| vendored liblzma 5.8.1 instead of the system 5.4.5 (deps lane) | LZMA2 decode | 1.43× (coder), `ratio` decode −8% on Silesia; same bytes; encode within ±4% | in (rc3) |
| Huffman leaves pass `HUF_flags_bmi2` (deps lane) | codec 13 decode | 1.15–1.21× leaf decode on baseline x86-64 builds (Kolmo's own leaf chunks); below noise end to end (leaves ≈ 2% of decode); same bytes | in (rc3) |
| function-section GC of vendored code (deps lane) | build | binary code 3.13 → 2.78 MB (371 KiB of zstd, LZ4, libbsc, xxHash, Brotli, liblzma never reached); same bytes; speed within noise | in (rc3) |
| zstd x86-64 Huffman assembly (vendored) | `balanced` decode | 2% end to end (0.480 → 0.469 s, Silesia, baseline build); no AArch64 equivalent in zstd | in; AArch64 port left to simd-arm, low priority |
| LZMA2 parse lighter than 9e in `ratio` (deps lane) | `ratio` encode | depth auto +0.065% at 1.11×, depth 64 +0.22% at 1.41×, nice 128 +0.48% at 1.52×, non-extreme +1.19% at 2.3× (structured); Silesia non-extreme +0.018% at 1.2× | dropped: 9e kept |
| LZMA2 decoded straight into the output block (deps lane) | `ratio` decode | ≤ 3% of LZMA2 decode (dictionary mapping + copy, 22 ms per 47.5 MB) | dropped (not worth a fork of liblzma) |
| Huffman leaf format variants: table log 8–12, optimal depth, previous-table reuse (deps lane) | codec 13 size | −0.09% to +1.95%; reuse never chosen | dropped |
| zstd leaves without the frame (magicless, no content size) (deps lane) | leaf size | < 0.03% (26 frames in a 3.2 MB structured output) | dropped |
| kernel dispatch table (`src/arch/cpu.c`, `arch/kernels.h`) with C references in `src/algo` | all | `KOLMO_CPU=c\|sse2\|sse41\|avx2` lowers the level; `make NO_ASM=1` = references only; `check-kernels` compares every level | in (rc3 simd-x86) |
| IMG8 median inverse as a 16-row wavefront (rows in byte lanes, skewed C bytes, 16×16 transposes), SSE2/SSE4.1 | decode of 8-bit images, Y4M in the generic modes | loop 4.3× (RGB) to 8.4× (grey); `balanced` decode 1.35× (RGB), 1.83× (grey), 2.38× (CIF video) | in (rc3 simd-x86) |
| same kernel, VEX (AVX2 target) or AVX-512VL build | — | 25–35% (VEX) and 10% (AVX-512VL) slower than the SSE4.1 build | out |
| IMG8 forward (median residuals) SSE2/AVX2 | compress of 8-bit images, Y4M | loop 5.4× / 8×; `balanced` compress +6–7% | in (rc3 simd-x86) |
| byte-plane split/merge by unpack rounds (T_SHUF, one channel; w = 2 also with delta) | `speed` integer arrays; eco/hc video | loops 4.7–45×; `speed` compress 1.16–1.23×, decode 1.49×; eco video decode 1.10–1.64× | in (rc3 simd-x86) |
| CM mixer dot products (vpmuldq) and weight updates (vpmulld), AVX2/SSE4.1 | `-x` | kernels 3.1× / 2.2×; `-x` 1.15–1.23× faster both ways | in (rc3 simd-x86) |
| CM context hashes 8 at a time (vpmulld) | `-x` | kernel 1.8×; with the mixer, `-x` text 1.22× compress / 1.18× decode | in (rc3 simd-x86) |
| kvid strip prediction: 3 predictors' costs in one vector pass; decode by vector add or the wavefront median inverse | kvid (Y4M node) | `rt` 4.8× compress, 6.4× decode; `balanced` 2.6–3.4× / 2.5–4.2× | in (rc3 simd-x86) |
| IMG16 stride search (Σ\|Δ\| in 32-bit lanes) and forward prediction, SSE2/AVX2 | compress of 16-bit images | kernels 9.9× / 19.5× (AVX2); `balanced` compress 1.32–1.42× | in (rc3 simd-x86) |
| inline popcount (no libgcc call on baseline x86-64) | table scanners, decimal parser | numeric CSV `balanced` compress +4.8% | in (rc3 simd-x86) |
| IMG16 median inverse as a wavefront | — | not possible: a row's first sample predicts from the previous row's last one (rows are serial) | out |
| rANS order-0 decode with a packed slot table | eco/hc leaves | −5% to +1% (noise) | out |

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
