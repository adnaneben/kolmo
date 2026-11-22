# Routing map: input type × level

> **Since 0.0.1 (container version 12)** Kolmo has no library coders. This document was written before that, and its
> coder names map as follows in 0.0.x (`src/codecs/native.c`): LZ4 → KLZ level 1 and LZ4-HC → KLZ level 3 (28); zstd
> level 1-2 / 3-5 / 6-9 / 10-15 / 16-18 / 19+ → KLZH2 (32) level 1 / 3 / 5 / 6 / 8 / 9; Brotli → KLZH2 level 7; LZMA2 →
> KLZMA (24); zstd's Huffman (HUF) → khuf (27); XXH3/XXH64 → `kolmo_khash64`. Dictionaries
> and trained profiles are removed. Figures below were measured with the library coders.

Status: rc3 wave 2, router lane, 17 November 2026 (branch `claude/rc3w2-router-20261117`). What Kolmo detects, which
transform and codec each level then uses, how that compares with the best tool for the type, and where Kolmo is not
the best. The ordered list of what to build next is `docs/ROUTING-PLAN.md`.

**Levels.** `speed` (internal `rt`): LZ4. `balanced` (`fast`, the default): zstd 3. `eco`: Brotli 8, window 2^24.
`hc`: zstd 19 (22 for word-transformed text). `ratio` (`max`): LZMA2 9e, block sorting (codec 18) for text and where it
wins on binary data. `-x` (`ultra`): context mixing (codec 7) with LZMA2 and block-sorting trials, plus codec 25
(columns / halves), GR6+ (codec 26) on line-structured text, the FASTQ family coder (16) and the CM image model (17).
Transforms (`-n` turns them off) are chosen by the planner per file or archive member (`src/plan/planner.c`), and
format-2 codecs (audio 19, video 20) per file.

**Measurements.** Unless a row names another source, every number comes from `benchmark/rc3/router/RESULTS.md`:
cloud VM (x86-64, 4 vCPU, 15.7 GiB), every tool on one thread (`kolmo -T 1`, `zstd -T1`, `xz -T1`), four tools at a
time (two for `-x`), so MB/s are per core on a loaded machine; every output decoded and compared. The corpus is
public data (Silesia, enwik8, OWID, Loghub, GH Archive, Kodak, ESC-50, xiph CIF, ENA SRR062634, UCSC chr22,
MiniLM-L6) cut to at most 16–25 MB per file; `-x` rows are this branch (router fix R0; times from the rc3 run, whose frames are identical except on two files), the rest are rc3 at `1536036`.
"Best general" is the smallest of zstd -3/-19/-22, xz -9e, Brotli 11, bzip2 -9 and LZ4 on the same files;
"specialist" is FLAC -8 (audio), FFV1 (video, one thread, 4 slices), JPEG XL e9 / JPEG-LS (8-bit images). Ratio =
input ÷ output. "Route" is the transform+codec covering most input bytes (`frameinfo.py`).

## How the router decides (detection order)

| step | signal | where | route |
|---|---|---|---|
| 1 | `YUV4MPEG2 ` header, 8-bit, ≥ 4 KiB | `core/compress.c` | the whole file to kvid (codec 20), every level but `speed` |
| 2 | 16-bit PCM WAV (`ka_wav_parse`) | `core/compress.c` | every block holding samples is also coded by kaud (codec 19, profile 0 up to `hc`, profile 1 at `ratio`/`-x`), the smaller kept; not at `speed` |
| 3 | tar archive | `transforms/tar.c` `segments()` | each member planned on its own (step 5 onwards) |
| 4 | trained profile (`-P`) | `plan/profile.c` | pinned plan for the member's shape and size band |
| 5 | text: ≥ 95% printable bytes in the first 64 KiB | `plan/planner.c` `make_plan` | 5a–5g; `speed` and `-Q` code text as it is |
| 5a | delimited, > 256 columns with a constant count, ≥ 64 KiB | same | wide-table graph (codec 12) when 8% below zstd -1 (and 3% below the level's coder from `eco` up) |
| 5b | minified JSON (no newline, ≥ 16 `},{`), few key layouts | same | split into records, log tokens (tflag 10) |
| 5c | JSON lines / pretty JSON (lines start with `{ [ " } ]`) | same | JSON or log tokens when 8% smaller on a zstd -1 trial; pretty JSON not at `ratio`/`-x` |
| 5d | ≥ 50 lines, 2–256 fields per line, ≥ 95% of lines alike (`, \t ; \| space`) | same | typed table (`tbl2`, formulas) when 3% smaller on zstd -1; at `balanced`, a first line of more than 256 fields repeated over 9 lines: transpose |
| 5e | log lines (template trial 8% smaller) | same | log templates (tflag 5); `balanced`: fixed-length line classes when smaller |
| 5f | other text at `eco` and up | same | word dictionary when 1% smaller on zstd -3 |
| 5g | `ratio`: block sorting for text blocks; `-x`: CM (transform kept only if CM on a 1 MiB transformed sample wins; tables also tried as codec 25 columns) | `core/blocks.c`, `plan/parallel.c` | |
| 6 | binary: a byte-plane width whose planes are more predictable than the bytes | `make_plan` | shuffle when 5% below plain on a zstd -1 trial (32 KiB) |
| 7 | binary ≥ 256 KiB: candidates on a 1 MiB middle slice, scored by zstd: x86 E8/E9 density ≥ 1/1000, 16-bit image, fixed records, float32/16 (exponent predictable, mantissa random), integer arrays, DEC64 (decimal doubles), 8-bit image (row lag) | same | the cheapest wins if 3% below plain (DEC64: 7% below the others); four 32 KiB probes elsewhere mark mixed files (`verify`) |
| 8 | `.npy` header | same | dtype and shape fix width, header and row length |
| 9 | FASTQ records (`@`, `+`, equal lengths), block ≥ 64 KiB | `core/blocks.c` | `-x` only: family coder (codec 16) first, CM skipped |
| 10 | incompressible | `core/blocks.c` | stored when the coder does not shrink the block; `speed`/`balanced` find it fast, slower levels spend their full coder time first (plan item 6) |

## The map

Each type: what it is in the corpus, the detection signal and route, the measured table, and the comparison with the leader. Negative = Kolmo smaller.

### CSV, long (few columns, many rows)

**Corpus:** long tables: covid daily counts per country (5 columns), OWID CO2 melted to 4 columns. **Route:** typed table `tbl2` with formulas at every level but `speed`; `-x`: GR6+ (covid) or codec 25 columns.

2 files, 7.8 MB: covid-countries.csv, owid-co2-long.csv. Best general tool: xz-9e (8.37×, 1.2 / 69.8 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 2.56 | 251.3 / 558.2 | +0.0% (lz4-1) | +227.2% | — |
| balanced | tbl+zstd/tf4 100% | 13.23 | 142.4 / 272.3 | -68.1% (zstd-3) | -36.7% | — |
| eco | tbl+brotli/tf4 100% | 13.77 | 49.7 / 158.8 | -39.7% (brotli-11) | -39.2% | — |
| hc | tbl+zstd/tf4 100% | 14.06 | 17.8 / 258.8 | -51.9% (zstd-19) | -40.5% | — |
| ratio | tbl+lzma2/tf4 100% | 14.85 | 4.4 / 108.2 | -43.6% (xz-9e) | -43.6% | — |
| -x | gr6 71%, tp+cmpar/tf2 29% | 18.03 | 0.1 / 0.7 | -53.6% (xz-9e) | -53.6% | — |

**Against the leaders:** Far ahead of every general tool (xz −44% at `ratio`). The 6-bit study's GR6+ is 8–17% below `ratio` on these (plan item 3).

### CSV, wide

**Corpus:** wide tables: OWID CO2 (79 columns), OWID energy (130 columns). **Route:** `tbl2` with derived-column formulas; `-x`: GR6+ on CO2, table + LZMA2 on energy (R0: was table + CM, 1% larger than `ratio`).

2 files, 24.5 MB: owid-co2.csv, owid-energy.csv. Best general tool: brotli-11 (4.68×, 0.4 / 196.6 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 2.02 | 303.2 / 1028.2 | +0.0% (lz4-1) | +132.4% | — |
| balanced | tbl+zstd/tf4 100% | 10.46 | 104.9 / 396.0 | -68.3% (zstd-3) | -55.2% | — |
| eco | tbl+brotli/tf4 100% | 11.20 | 30.8 / 206.5 | -58.2% (brotli-11) | -58.2% | — |
| hc | tbl+zstd/tf4 100% | 11.35 | 11.6 / 326.7 | -62.8% (zstd-19) | -58.7% | — |
| ratio | tbl+lzma2/tf4 100% | 11.95 | 2.9 / 134.2 | -61.0% (xz-9e) | -60.8% | — |
| -x | gr6 62%, tbl+lzma2/tf4 38% | 12.54 | 0.2 / 4.3 | -62.8% (xz-9e) | -62.6% | — |

**Against the leaders:** −55% to −63% against the best general tool, −58% to −68% against each level's rival. The GR6+ trial costs +126% compress time on energy for nothing (plan item 7).

### TSV

**Corpus:** OWID CO2 as TSV. **Route:** same route as CSV (tab is a delimiter candidate).

1 files, 15.2 MB: owid-co2.tsv. Best general tool: brotli-11 (4.85×, 0.4 / 211.1 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 2.02 | 308.6 / 952.7 | +0.0% (lz4-1) | +139.9% | — |
| balanced | tbl+zstd/tf4 100% | 10.94 | 131.0 / 422.2 | -69.9% (zstd-3) | -55.7% | — |
| eco | tbl+brotli/tf4 100% | 11.64 | 33.4 / 213.2 | -58.4% (brotli-11) | -58.4% | — |
| hc | tbl+zstd/tf4 100% | 11.80 | 13.4 / 309.8 | -64.2% (zstd-19) | -58.9% | — |
| ratio | tbl+lzma2/tf4 100% | 12.45 | 2.9 / 127.8 | -61.1% (xz-9e) | -61.1% | — |
| -x | gr6 100% | 13.51 | 0.3 / 2.6 | -64.2% (xz-9e) | -64.1% | — |

**Against the leaders:** Same as csv_wide.

### CSV, sparse integer matrix

**Corpus:** sparse integer matrix CSV (HDFS event counts, 32 columns, mostly 0/1). **Route:** `balanced` to `hc`: typed transpose; `ratio`: plain block sorting (the transpose lost); `-x`: CM.

1 files, 16.0 MB: hdfs_events16.csv. Best general tool: bzip2-9 (9.70×, 6.0 / 31.1 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 3.94 | 744.2 / 1666.7 | +0.0% (lz4-1) | +146.0% | — |
| balanced | tp+zstd/tf2 100% | 7.80 | 249.6 / 334.7 | -14.0% (zstd-3) | +24.4% | — |
| eco | tp+brotli/tf2 100% | 8.28 | 16.9 / 146.5 | +10.1% (brotli-11) | +17.1% | — |
| hc | tp+zstd/tf2 100% | 9.63 | 4.6 / 172.0 | -4.1% (zstd-19) | +0.7% | — |
| ratio | bwt 100% | 9.96 | 2.3 / 33.2 | -6.9% (xz-9e) | -2.6% | — |
| -x | cm 100% | 11.02 | 0.2 / 0.2 | -15.9% (xz-9e) | -12.0% | — |

**Against the leaders:** Ahead of the level rival at `balanced` (−14%), `hc` (−4%), `ratio` (−7% vs xz) and `-x` (−16%); `eco` is +10% vs Brotli 11; `ratio` is only 2.6% below bzip2 -9 (the transpose loses to plain block sorting there). A sparse-count model (zero runs per column) is not in the table transform.

### JSON documents (pretty and minified)

**Corpus:** GH Archive events: pretty-printed (indent 2) and minified array. **Route:** pretty: JSON/log tokens (`balanced`–`hc`), log tokens + LZMA2 (`ratio`), CM (`-x`); minified: plain (`balanced`), word dictionary (`eco`–`ratio`), CM.

2 files, 21.3 MB: gharchive_min.json, gharchive_pretty.json. Best general tool: brotli-11 (14.10×, 0.7 / 510.1 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 4.70 | 482.3 / 1563.9 | +0.3% (lz4-1) | +200.2% | — |
| balanced | log/json1+zstd/tf5 56%, zstd 44% | 9.59 | 277.7 / 924.7 | -4.0% (zstd-3) | +47.0% | — |
| eco | log/json1+brotli/tf5 56%, word+brotli/tf9 44% | 12.82 | 32.5 / 274.4 | +10.0% (brotli-11) | +10.0% | — |
| hc | log/json1+zstd/tf5 56%, word+zstd/tf9 44% | 13.47 | 4.4 / 339.2 | -6.0% (zstd-19) | +4.7% | — |
| ratio | log+lzma2/tf5 56%, word+bwt/tf0 44% | 14.30 | 6.6 / 67.5 | -7.0% (xz-9e) | -1.4% | — |
| -x | cm 56%, word+cm/tf0 44% | 21.01 | 0.1 / 0.2 | -36.7% (xz-9e) | -32.9% | — |

**Against the leaders:** Close to Brotli 11 at `ratio` (−1.4%); `-x` −33%. The minified-JSON split (step 5b) was not chosen (route: plain zstd at `balanced`).

### JSON lines

**Corpus:** GH Archive JSON lines (one event per line, nested, heterogeneous). **Route:** `balanced`: plain zstd (the token trial does not gain 8%); `eco`–`ratio`: word dictionary; `-x`: CM.

1 files, 16.8 MB: gharchive16.jsonl. Best general tool: brotli-11 (13.08×, 0.7 / 524.3 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 4.51 | 458.4 / 1290.5 | +0.1% (lz4-1) | +189.9% | — |
| balanced | zstd 100% | 8.21 | 319.6 / 729.4 | +4.9% (zstd-3) | +59.3% | — |
| eco | word+brotli/tf9 100% | 11.57 | 21.2 / 251.5 | +13.0% (brotli-11) | +13.0% | — |
| hc | word+zstd/tf9 100% | 12.02 | 3.3 / 289.3 | -2.9% (zstd-19) | +8.8% | — |
| ratio | word+bwt/tf0 100% | 13.15 | 21.0 / 40.6 | -5.4% (xz-9e) | -0.5% | — |
| -x | cm 100% | 19.17 | 0.2 / 0.2 | -35.1% (xz-9e) | -31.8% | — |

**Against the leaders:** `balanced` is +4.9% against single-stream zstd -3: eight independent 2 MiB blocks (plan item 5). `ratio` ≈ Brotli 11 (−0.5%); `-x` −32%.

### Logs

**Corpus:** Loghub Apache, Linux, SSH (16 MB), HDFS (16 MB). **Route:** log templates with typed fields at every level but `speed` (`ratio`: + LZMA2 or block sorting); `-x`: CM over the plain lines.

4 files, 41.0 MB: apache.log, hdfs16.log, linux.log, ssh16.log. Best general tool: xz-9e (24.83×, 1.2 / 245.5 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 7.50 | 712.5 / 1243.6 | +0.1% (lz4-1) | +231.2% | — |
| balanced | log+zstd/tf5 87%, log/json1+zstd/tf5 13% | 30.43 | 210.1 / 540.7 | -57.0% (zstd-3) | -18.4% | — |
| eco | log+brotli/tf5 87%, log/json1+brotli/tf5 13% | 35.74 | 72.3 / 319.4 | -36.5% (brotli-11) | -30.5% | — |
| hc | log+zstd/tf5 87%, log/json1+zstd/tf5 13% | 38.67 | 13.8 / 368.4 | -45.1% (zstd-19) | -35.8% | — |
| ratio | log+lzma2/tf5 87%, log/json1+bwt/tf5 13% | 40.93 | 10.9 / 231.1 | -39.4% (xz-9e) | -39.4% | — |
| -x | cm 100% | 57.33 | 0.2 / 0.2 | -56.7% (xz-9e) | -56.7% | — |

**Against the leaders:** −36% to −57% against each level's rival (−18% to −57% against the best general tool). GR6+ with `lzp` phrases would be 7–30% below `ratio` on Apache, Linux, HDFS (plan item 3); `-x` stays 19–31% below that.

### Prose

**Corpus:** Silesia dickens (English), reymont (Polish), Webster dictionary (16 MB). **Route:** word dictionary at `eco`/`hc` where it gains (dickens); `ratio`: block sorting; `-x`: CM.

3 files, 33.6 MB: dickens, reymont, webster16. Best general tool: bzip2-9 (4.45×, 11.9 / 27.0 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 1.89 | 293.7 / 1036.9 | +0.1% (lz4-1) | +135.7% | — |
| balanced | zstd 100% | 3.16 | 174.7 / 644.9 | +1.0% (zstd-3) | +41.0% | — |
| eco | brotli 70%, word+brotli/tf9 30% | 3.99 | 9.0 / 219.6 | +9.9% (brotli-11) | +11.6% | — |
| hc | zstd 70%, word+zstd/tf9 30% | 4.39 | 1.9 / 381.3 | -1.6% (zstd-19) | +1.4% | — |
| ratio | bwt 70%, word+bwt/tf0 30% | 5.64 | 14.1 / 20.8 | -22.4% (xz-9e) | -21.1% | — |
| -x | cm 100% | 6.47 | 0.2 / 0.2 | -32.3% (xz-9e) | -31.2% | — |

**Against the leaders:** `ratio` −21% vs bzip2 at 14 MB/s; `-x` −31%. `balanced` +1.0% vs zstd -3 and `eco` +9.9% vs Brotli 11 (Brotli 8 by design).

### Source code

**Corpus:** Silesia samba (tar of C sources), Kolmo `src/` as a tar. **Route:** plain coders (tar members are text; the word transform does not gain on code); `ratio`: LZMA2 or block sorting.

2 files, 19.6 MB: kolmo_src.tar, samba16. Best general tool: xz-9e (5.44×, 1.8 / 80.6 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 2.68 | 408.4 / 677.4 | +0.1% (lz4-1) | +103.2% | — |
| balanced | zstd 100% | 4.05 | 200.2 / 722.2 | +2.8% (zstd-3) | +34.5% | — |
| eco | brotli 100% | 4.88 | 13.4 / 230.8 | +11.1% (brotli-11) | +11.7% | — |
| hc | zstd 100% | 5.23 | 2.8 / 324.2 | -0.2% (zstd-19) | +4.0% | — |
| ratio | lzma2 85%, bwt 15% | 5.47 | 1.6 / 62.7 | -0.4% (xz-9e) | -0.4% | — |
| -x | cm 100% | 6.89 | 0.2 / 0.3 | -21.0% (xz-9e) | -21.0% | — |

**Against the leaders:** `ratio` = xz (−0.4%); `-x` −21%. No code-specific model (identifier dictionaries, indentation); `balanced` +2.8% vs zstd -3 (block split).

### XML / HTML

**Corpus:** enwik8 (16 MB), Silesia xml, three Wikipedia HTML pages. **Route:** word dictionary at `eco`–`ratio` (enwik8, xml); `ratio`: block sorting; `-x`: CM.

3 files, 23.3 MB: enwik8_16, wiki3.html, xml. Best general tool: brotli-11 (4.64×, 0.5 / 226.8 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 2.07 | 345.9 / 829.7 | +0.1% (lz4-1) | +124.6% | — |
| balanced | zstd 100% | 3.39 | 196.9 / 660.5 | +1.0% (zstd-3) | +37.1% | — |
| eco | word+brotli/tf9 95%, brotli 5% | 4.27 | 8.8 / 181.9 | +8.7% (brotli-11) | +8.7% | — |
| hc | word+zstd/tf9 95%, zstd 5% | 4.64 | 2.4 / 257.0 | -3.7% (zstd-19) | +0.1% | — |
| ratio | word+bwt/tf0 95%, bwt 5% | 5.40 | 12.3 / 19.4 | -14.5% (xz-9e) | -14.0% | — |
| -x | cm 100% | 6.30 | 0.1 / 0.2 | -26.6% (xz-9e) | -26.3% | — |

**Against the leaders:** `ratio` −14% vs Brotli 11; `-x` −26%. No markup-aware transform (tag/attribute dictionaries); `balanced` +1.0% vs zstd -3.

### NPY arrays

**Corpus:** .npy arrays derived from OWID CO2: float64 decimal, float32 (computed, not decimal), int64, int32. **Route:** graph per dtype: DEC64, integer (float32 coded as int4 here), with per-plane leaves (codecs 21–23).

4 files, 1.4 MB: co2_f64.npy, gdp_per_cap_f32.npy, population_i64.npy, year_i32.npy. Best general tool: xz-9e (5.52×, 2.2 / 34.4 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 67%, shuf+lz4/tf1 33% | 4.31 | 120.6 / 168.9 | -26.3% (lz4-1) | +28.3% | — |
| balanced | kg:int4+graph/tf0 33%, kg:dec64+graph/tf0 33%, kg:int8+graph/tf0 33% | 7.92 | 9.5 / 120.6 | -42.8% (zstd-3) | -30.2% | — |
| eco | kg:int4+graph/tf0 33%, kg:dec64+graph/tf0 33%, kg:int8+graph/tf0 33% | 8.03 | 5.6 / 86.6 | -32.0% (brotli-11) | -31.2% | — |
| hc | kg:int4+graph/tf0 33%, kg:dec64+graph/tf0 33%, kg:int8+graph/tf0 33% | 8.17 | 1.8 / 70.0 | -40.4% (zstd-19) | -32.4% | — |
| ratio | kg:int4+graph/tf0 33%, kg:dec64+graph/tf0 33%, kg:int8+graph/tf0 33% | 8.26 | 1.1 / 43.4 | -33.1% (xz-9e) | -33.1% | — |
| -x | kg:dec64+graph/tf0 33%, kg:int8+graph/tf0 33%, kg:int4+graph/tf0 17% | 8.34 | 0.3 / 1.1 | -33.8% (xz-9e) | -33.8% | — |

**Against the leaders:** −30% to −43% against the general tools. Mac: kbin (research) equals `balanced` over 11 NPY files.

### Raw integer array

**Corpus:** raw int64 (population counts, no header). **Route:** integer graph, width found by trial.

1 files, 0.5 MB: population.i64. Best general tool: xz-9e (3.87×, 1.6 / 32.4 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | shuf+lz4/tf1 100% | 4.00 | 95.8 / 160.8 | -50.1% (lz4-1) | -3.4% | — |
| balanced | kg:int8+graph/tf0 100% | 5.89 | 9.8 / 115.4 | -46.0% (zstd-3) | -34.3% | — |
| eco | kg:int8+graph/tf0 100% | 6.00 | 5.9 / 93.8 | -37.0% (brotli-11) | -35.5% | — |
| hc | kg:int8+graph/tf0 100% | 6.21 | 1.9 / 45.9 | -45.2% (zstd-19) | -37.7% | — |
| ratio | kg:int8+graph/tf0 100% | 6.35 | 1.2 / 76.3 | -39.1% (xz-9e) | -39.1% | — |
| -x | kg:int8+graph/tf0 100% | 6.35 | 1.0 / 67.2 | -39.1% (xz-9e) | -39.1% | — |

**Against the leaders:** −34% to −46% against the general tools.

### Raw float array (decimal values)

**Corpus:** raw float64 (decimal values, no header). **Route:** DEC64 graph found by the decimal probe.

1 files, 0.5 MB: co2.f64. Best general tool: bzip2-9 (6.37×, 3.8 / 22.2 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 3.40 | 125.0 / 187.6 | +0.0% (lz4-1) | +87.1% | — |
| balanced | kg:dec64+graph/tf0 100% | 9.65 | 11.0 / 121.7 | -52.2% (zstd-3) | -34.0% | — |
| eco | kg:dec64+graph/tf0 100% | 9.70 | 5.9 / 71.5 | -37.2% (brotli-11) | -34.4% | — |
| hc | kg:dec64+graph/tf0 100% | 9.79 | 2.1 / 86.6 | -47.3% (zstd-19) | -35.0% | — |
| ratio | kg:dec64+graph/tf0 100% | 9.67 | 3.9 / 17.2 | -34.3% (xz-9e) | -34.1% | — |
| -x | kg:dec64+graph/tf0 100% | 10.01 | 1.5 / 83.4 | -36.6% (xz-9e) | -36.4% | — |

**Against the leaders:** −34% vs bzip2; `ratio` is 1.3% larger than `hc` here (BWT on decimal planes; plan item 10).

### Binary records with floats

**Corpus:** Silesia sao: star catalog, 28-byte records of mixed float/int fields. **Route:** fixed-record transform (record period found by lag search) at every level but `speed`.

1 files, 7.3 MB: sao. Best general tool: xz-9e (1.64×, 2.1 / 23.5 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 1.07 | 259.9 / 619.8 | +0.0% (lz4-1) | +53.5% | — |
| balanced | rec+zstd/tf7 100% | 3.15 | 111.1 / 521.7 | -58.5% (zstd-3) | -48.0% | — |
| eco | rec+brotli/tf7 100% | 3.28 | 16.9 / 225.9 | -51.7% (brotli-11) | -50.0% | — |
| hc | rec+zstd/tf7 100% | 3.28 | 7.9 / 294.8 | -55.7% (zstd-19) | -50.0% | — |
| ratio | rec+lzma2/tf7 100% | 3.32 | 5.1 / 84.0 | -50.6% (xz-9e) | -50.6% | — |
| -x | rec+cm/tf7 100% | 3.42 | 0.3 / 0.4 | -52.1% (xz-9e) | -52.1% | — |

**Against the leaders:** −50% vs xz at `ratio`; `-x` −52%.

### Model weights

**Corpus:** MiniLM-L6 safetensors: float32 (first 24 MB) and the same tensors as bf16. **Route:** float graph (exponent and mantissa planes) from `balanced` up; `speed` stores (LZ4 cannot shrink it); `-x` on bf16: CM over the plain bytes.

2 files, 70.6 MB: minilm_bf16.safetensors, minilm_f32_24M.safetensors. Best general tool: xz-9e (1.30×, 1.4 / 21.2 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | store 96% | 1.00 | 1159.4 / 1184.7 | -0.0% (lz4-1) | +30.0% | — |
| balanced | kg:float2+graph/tf0 64%, kg:float4+graph/tf0 36% | 1.40 | 289.8 / 715.4 | -13.2% (zstd-3) | -7.2% | — |
| eco | kg:float2+graph/tf0 64%, kg:float4+graph/tf0 36% | 1.41 | 8.8 / 232.8 | -8.3% (brotli-11) | -7.5% | — |
| hc | kg:float2+graph/tf0 64%, kg:float4+graph/tf0 36% | 1.40 | 39.2 / 359.9 | -13.1% (zstd-19) | -7.2% | — |
| ratio | kg:float2+graph/tf0 64%, kg:float4+graph/tf0 36% | 1.41 | 6.7 / 23.7 | -7.6% (xz-9e) | -7.6% | — |
| -x | cm 64%, kg:float4+graph/tf0 36% | 1.41 | 0.3 / 0.4 | -8.0% (xz-9e) | -8.0% | — |

**Against the leaders:** −7% to −13% against the general tools. Mac: `balanced` −0.8% vs ZipNN (10 threads) at 2.1× its compress and 4.8× its decode speed. `-x` on bf16 runs CM for 208 s for −0.7% (plan item 11).

### Images, 8-bit

**Corpus:** Kodak photographs as PPM (RGB) and PGM (grey). **Route:** IMG8 graph (median predictor per channel, row length from the header or a lag search); `ratio`: + block-sorted leaves; `-x`: CM image model (codec 17).

5 files, 5.1 MB: kodim01.ppm, kodim03_grey.pgm, kodim05.ppm, kodim08.ppm, kodim12.ppm. Best general tool: bzip2-9 (1.86×, 10.1 / 13.4 MB/s); specialist: jxl-e9 (2.62×, 0.5 / 8.4 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 1.12 | 222.3 / 412.2 | +0.0% (lz4-1) | +66.8% | +134.8% |
| balanced | kg:img8+graph/tf0 95%, zstd 5% | 1.64 | 28.5 / 187.9 | -16.7% (zstd-3) | +13.8% | +60.1% |
| eco | kg:img8+graph/tf0 100% | 1.78 | 6.7 / 85.3 | -4.8% (brotli-11) | +4.8% | +47.5% |
| hc | kg:img8+graph/tf0 100% | 1.81 | 2.8 / 154.0 | -6.5% (zstd-19) | +2.7% | +44.5% |
| ratio | kg:img8+graph/tf0 100% | 2.10 | 1.7 / 7.9 | -14.7% (xz-9e) | -11.3% | +24.9% |
| -x | cmimg 100% | 2.90 | 0.2 / 0.2 | -38.3% (xz-9e) | -35.9% | -9.7% |

**Against the leaders:** **Largest gap.** `balanced` +60%, `ratio` +25% against JPEG XL e9 (RGB alone: +63% at `balanced`), and bzip2 -9 is smaller than `balanced`, `eco` and `hc`; only `-x` wins (−9.7%), at 0.2 MB/s. No colour decorrelation: a reversible colour transform alone cuts the residual entropy by 30% (plan item 1).

### Images, 16-bit

**Corpus:** Silesia x-ray (12-bit in 16), mr (16-bit DICOM). **Route:** 16-bit image transform (row stride search, median prediction) everywhere but `speed`; `-x`: + CM.

2 files, 18.4 MB: mr, x-ray. Best general tool: bzip2-9 (2.84×, 16.5 / 28.5 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | shuf+lz4/tf1 54%, lz4 40%, store 6% | 1.44 | 501.2 / 1386.8 | -7.5% (lz4-1) | +97.4% | — |
| balanced | img16+zstd/tf6 100% | 3.09 | 238.9 / 326.5 | -38.0% (zstd-3) | -8.0% | — |
| eco | img16+brotli/tf6 100% | 3.13 | 16.4 / 126.8 | -21.5% (brotli-11) | -9.3% | — |
| hc | img16+zstd/tf6 100% | 3.12 | 6.5 / 205.4 | -28.6% (zstd-19) | -9.0% | — |
| ratio | img16+bwt/tf6 54%, img16+lzma2/tf6 46% | 3.29 | 2.6 / 26.1 | -22.6% (xz-9e) | -13.6% | — |
| -x | img16+cm/tf6 100% | 3.35 | 0.3 / 0.3 | -24.1% (xz-9e) | -15.3% | — |

**Against the leaders:** −8% to −15% against the best general tool (bzip2), −21% to −38% against each level's rival. Mac: JPEG-LS beats `balanced` by 1.8% on x-ray, `-x` beats JPEG-LS by 0.9%; JPEG XL e9 is 5.9% below `-x` on MR (`MT.md`).

### Audio (WAV)

**Corpus:** 16-bit PCM WAV (ESC-50 clips, 44.1 kHz mono, and the clips joined). **Route:** kaud (codec 19): profile 0 (LPC + Rice) from `balanced` to `hc`, profile 1 (LMS cascade + binary coder) at `ratio`/`-x`; `speed` is LZ4 (by design).

6 files, 6.6 MB: 1-100032-A-0.wav, 1-100038-A-14.wav, 1-100210-A-36.wav, 1-101296-A-19.wav, 1-101336-A-30.wav, esc_joined.wav. Best general tool: brotli-11 (1.80×, 0.4 / 86.6 MB/s); specialist: flac-8 (2.39×, 67.0 / 126.2 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 80%, store 13%, shuf+lz4/tf1 7% | 1.28 | 257.4 / 318.0 | -2.2% (lz4-1) | +40.3% | +86.2% |
| balanced | aud 100% | 2.53 | 23.4 / 77.6 | -40.0% (zstd-3) | -28.8% | -5.5% |
| eco | aud 100% | 2.53 | 7.3 / 84.4 | -28.8% (brotli-11) | -28.8% | -5.5% |
| hc | aud 100% | 2.53 | 3.2 / 79.1 | -38.1% (zstd-19) | -28.8% | -5.5% |
| ratio | aud 100% | 2.63 | 1.3 / 10.2 | -34.9% (xz-9e) | -31.6% | -9.2% |
| -x | aud 100% | 2.63 | 0.2 / 9.3 | -34.9% (xz-9e) | -31.6% | -9.2% |

**Against the leaders:** Beats FLAC -8 from `balanced` up (−5.5%, −9.2% at `ratio`); decodes slower than FLAC (77 against 126 MB/s at `balanced`, 10 MB/s at `ratio`). Mac, 40 clips 10 at a time: p0 −4.0%, p1 −8.2% vs FLAC -8 (`benchmark/rc2/results/mt/MT.md`). Leaders not measured here: OptimFROG, Monkey's Audio. A WAV inside a tar is not routed to kaud (gap).

### Video (Y4M)

**Corpus:** xiph CIF 4:2:0 Y4M: akiyo (static), foreman (motion), 300 frames each. **Route:** kvid (codec 20) everywhere but `speed`: per-strip MED / temporal / temporal + MED prediction, layout coded by a nested Kolmo frame.

2 files, 91.2 MB: akiyo_cif.y4m, foreman_cif.y4m. Best general tool: xz-9e (2.67×, 1.1 / 36.1 MB/s); specialist: ffv1 (2.51×, 29.2 / 26.8 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 1.17 | 315.0 / 614.4 | +0.0% (lz4-1) | +128.7% | +115.0% |
| balanced | vid 100% | 2.96 | 51.3 / 139.4 | -36.8% (zstd-3) | -9.8% | -15.2% |
| eco | vid 100% | 3.08 | 4.4 / 98.8 | -18.3% (brotli-11) | -13.5% | -18.7% |
| hc | vid 100% | 3.24 | 1.4 / 134.3 | -27.2% (zstd-19) | -17.7% | -22.6% |
| ratio | vid 100% | 3.40 | 0.7 / 24.3 | -21.6% (xz-9e) | -21.6% | -26.3% |
| -x | vid 100% | 3.68 | 0.4 / 0.4 | -27.5% (xz-9e) | -27.5% | -31.8% |

**Against the leaders:** −15% (`balanced`) to −32% (`-x`) against FFV1 (one thread, 4 slices); `balanced` decodes at 139 MB/s per core. Mac, 3 clips, all threads: `balanced` −14% vs FFV1 at 0.94× its compress and 2.1× its decode speed (`MT.md`).

### FASTQ

**Corpus:** Illumina FASTQ, SRR062634 (ENA), first 24 MB. **Route:** `-x`: family coder (codec 16); every other level: plain coder (`ratio`: block sorting).

1 files, 25.2 MB: SRR062634_24M.fastq. Best general tool: xz-9e (3.92×, 0.7 / 65.0 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 1.92 | 262.4 / 825.1 | +0.0% (lz4-1) | +104.6% | — |
| balanced | zstd 100% | 3.04 | 146.2 / 633.9 | -0.2% (zstd-3) | +28.9% | — |
| eco | brotli 100% | 3.36 | 5.4 / 225.3 | +13.3% (brotli-11) | +16.7% | — |
| hc | zstd 100% | 3.74 | 1.3 / 464.3 | +0.5% (zstd-19) | +4.9% | — |
| ratio | bwt 100% | 4.59 | 12.1 / 16.4 | -14.6% (xz-9e) | -14.6% | — |
| -x | fqfam 100% | 5.13 | 4.2 / 3.7 | -23.5% (xz-9e) | -23.5% | — |

**Against the leaders:** `-x` (codec 16) is −10.4% vs `ratio` at 2.3–3.2× its compress and 3.6–4.9× its decode time (plan item 4). Mac: codec 16 −34.3% vs CRAM 3.1 at 3.4× its compress speed, 0.25× its decode (`MT.md`).

### FASTA

**Corpus:** chr22 of hg38, 16 MB from 20 MB on (upper/lower case runs, N runs). **Route:** `ratio`: block sorting; `-x`: CM; no DNA-specific route in release builds (the frontier DNA codecs are experimental).

1 files, 16.8 MB: chr22_16M.fa. Best general tool: xz-9e (3.89×, 0.9 / 73.6 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 1.76 | 304.5 / 851.6 | +0.0% (lz4-1) | +120.5% | — |
| balanced | zstd 100% | 2.99 | 167.4 / 663.1 | -0.0% (zstd-3) | +29.9% | — |
| eco | brotli 100% | 3.38 | 4.7 / 187.0 | +12.9% (brotli-11) | +15.1% | — |
| hc | zstd 100% | 3.77 | 1.0 / 316.6 | -0.1% (zstd-19) | +3.3% | — |
| ratio | bwt 100% | 4.15 | 13.3 / 17.2 | -6.4% (xz-9e) | -6.4% | — |
| -x | cm 100% | 4.62 | 0.2 / 0.2 | -15.9% (xz-9e) | -15.9% | — |

**Against the leaders:** `ratio` −6.4% vs xz; `-x` −15.9%. kfam one-stream family is +9.9% vs `-x` on chr22 (rc2). `-x` is 1.73 bits per byte of file here; DNA specialists (2-bit packing with order-k models) were not measured.

### Executables

**Corpus:** x86 code: Silesia mozilla (tar of 2002 Linux x86 binaries), ooffice (Windows DLL), first 16 MB of libLLVM.so (x86-64). **Route:** x86 call/jump filter when E8/E9 density ≥ 1/1000 (ooffice); DEC64 graph on the ELF's relocation and symbol tables (`balanced`–`hc`); else plain.

3 files, 38.9 MB: elf_so16, mozilla16, ooffice. Best general tool: xz-9e (3.74×, 2.1 / 53.4 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 2.00 | 405.1 / 980.6 | +0.0% (lz4-1) | +87.1% | — |
| balanced | zstd 59%, kg:dec64+graph/tf0 25%, x86+zstd/tf8 16% | 3.07 | 118.8 / 531.1 | -4.3% (zstd-3) | +21.7% | — |
| eco | brotli 43%, kg:dec64+graph/tf0 41%, x86+brotli/tf8 16% | 3.43 | 8.9 / 204.5 | +4.8% (brotli-11) | +9.0% | — |
| hc | zstd 43%, kg:dec64+graph/tf0 41%, x86+zstd/tf8 16% | 3.55 | 4.1 / 358.5 | -4.9% (zstd-19) | +5.2% | — |
| ratio | lzma2 84%, x86+lzma2/tf8 16% | 3.84 | 2.0 / 79.6 | -2.7% (xz-9e) | -2.7% | — |
| -x | cm 43%, kg:dec64+graph/tf0 41%, x86+cm/tf8 16% | 4.28 | 0.4 / 0.4 | -12.8% (xz-9e) | -12.8% | — |

**Against the leaders:** `ratio` −2.7% vs xz (mozilla +0.8%, ooffice −14% through the x86 filter, libLLVM equal); `-x` −12.8%. The x86 filter is not chosen on mozilla, and xz's BCJ is larger there too (0.6%), so not a miss. Leaders not measured: zpaq/paq8 with exe models (`-x` vs zpaq -m5 general −7.4%, rc2).

### Other binary and scientific text

**Corpus:** Silesia nci (chemical structures, text), osdb (database pages). **Route:** nci: line classes (`balanced`), log templates (`eco`/`hc`), block sorting (`ratio`); osdb: plain, block sorting at `ratio` (slice test), `-x`: block sorting (R0: was CM, +1.0% over `ratio`).

2 files, 26.9 MB: nci16, osdb. Best general tool: brotli-11 (7.46×, 0.5 / 370.5 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 100% | 3.33 | 503.1 / 1444.2 | +0.2% (lz4-1) | +124.1% | — |
| balanced | lines+zstd/tf12 62%, zstd 38% | 5.86 | 252.2 / 718.3 | -7.3% (zstd-3) | +27.3% | — |
| eco | log+brotli/tf5 62%, brotli 38% | 6.63 | 15.0 / 246.2 | +12.6% (brotli-11) | +12.6% | — |
| hc | log+zstd/tf5 62%, zstd 38% | 6.85 | 4.9 / 309.1 | -0.8% (zstd-19) | +9.0% | — |
| ratio | bwt 100% | 9.48 | 9.3 / 30.5 | -21.4% (xz-9e) | -21.3% | — |
| -x | cm 62%, bwt 38% | 9.76 | 0.2 / 0.5 | -23.6% (xz-9e) | -23.5% | — |

**Against the leaders:** `ratio` −21% vs Brotli 11.

### Archives of mixed files

**Corpus:** tar of 7 mixed members: prose, CSV, PPM, WAV, x86 DLL, log, 16-bit image. **Route:** tar members planned one by one (step 3); each member then routes as its type.

1 files, 16.3 MB: mixed.tar. Best general tool: bzip2-9 (3.06×, 13.1 / 26.6 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | lz4 81%, shuf+lz4/tf1 19% | 1.70 | 369.9 / 836.5 | -7.4% (lz4-1) | +80.0% | — |
| balanced | zstd 22%, tbl+zstd/tf4 19%, x86+zstd/tf8 19% | 3.12 | 105.9 / 478.4 | -25.4% (zstd-3) | -1.9% | — |
| eco | word+brotli/tf9 19%, tbl+brotli/tf4 19%, x86+brotli/tf8 19% | 3.42 | 14.9 / 196.8 | -13.1% (brotli-11) | -10.5% | — |
| hc | word+zstd/tf9 19%, tbl+zstd/tf4 19%, x86+zstd/tf8 19% | 3.53 | 5.6 / 300.4 | -20.1% (zstd-19) | -13.1% | — |
| ratio | word+bwt/tf0 19%, tbl+lzma2/tf4 19%, x86+lzma2/tf8 19% | 3.82 | 2.7 / 36.6 | -19.9% (xz-9e) | -19.8% | — |
| -x | cm 35%, gr6 19%, x86+cm/tf8 19% | 4.48 | 0.1 / 0.2 | -31.6% (xz-9e) | -31.6% | — |

**Against the leaders:** Ahead of the level rival at every level (`speed` −7% vs LZ4 through the shuffle on the x-ray member; `ratio` −20% vs xz), because members get their own transforms. The WAV member is coded as plain bytes: the audio route (step 2) needs the whole file to be a WAV.

### Already-compressed data

**Corpus:** JPEG, PNG, .tar.gz. **Route:** no detection: every level runs its coder and stores the block when it does not shrink.

3 files, 1.0 MB: apache.tar.gz, kodim02.png, kodim03.jpg. Best general tool: zstd-19 (1.01×, 2.6 / 110.4 MB/s).

| mode | route (share of bytes) | ratio | MB/s c / d | vs level rival | vs best general | vs specialist |
|---|---|---:|---:|---:|---:|---:|
| speed | store 100% | 1.00 | 80.0 / 85.0 | +0.0% (lz4-1) | +0.6% | — |
| balanced | store 100% | 1.00 | 36.5 / 91.4 | +0.0% (zstd-3) | +0.6% | — |
| eco | store 73%, brotli 27% | 1.00 | 14.3 / 88.9 | +0.3% (brotli-11) | +0.4% | — |
| hc | store 73%, zstd 27% | 1.01 | 6.1 / 133.4 | +0.0% (zstd-19) | +0.0% | — |
| ratio | lzma2 92%, store 8% | 1.01 | 4.2 / 36.8 | -0.0% (xz-9e) | +0.0% | — |
| -x | cm 100% | 1.02 | 0.2 / 0.2 | -1.2% (xz-9e) | -1.2% | — |

**Against the leaders:** Nothing to gain (≤ 1.2% at `-x`), but `ratio` spends 4 MB/s and `-x` 0.2 MB/s on it (plan item 6). Recompression of JPEG (lepton/brunsli class; published gains about 20%) and of deflate streams (precomp class) is not in Kolmo and not measured here.

## Where Kolmo is not the best

| type | level | leader | gap | why | plan |
|---|---|---|---:|---|---|
| RGB photographs | `balanced`–`ratio` | JPEG XL e9 (lossless) | +25% (`ratio`) to +63% (`balanced`, RGB only) | no colour decorrelation; order-0 coding of median residuals | items 1, 2 |
| 8-bit photographs | `balanced` | JPEG-LS | +2.6% | same | item 1 |
| 16-bit CT | `balanced` | JPEG-LS (Mac) | +1.8% | no context modelling of residuals below `-x` | item 2 |
| 16-bit MR | `-x` | JPEG XL e9 (Mac) | +5.9% | same | item 2 |
| text, source, JSON lines | `balanced` | zstd -3 (one stream) | +1% to +5% | eight independent blocks for parallel decode | item 5 |
| any text | `eco` | Brotli 11 | +9% to +15% | `eco` is Brotli 8 by design (25–86× faster than Brotli 11) | — |
| decimal float arrays | `ratio` | Kolmo `hc` | +0.4% to +1.3% | block-sorted decimal planes | item 10 |
| WAV inside an archive | every level | FLAC | (the member is coded as bytes) | the audio route needs the whole file to be one WAV | item 12 |
| already-compressed | `ratio`, `-x` | — | slow, not larger | no incompressibility probe before the slow coders | item 6 |
| FASTQ | `ratio` | Kolmo `-x` (codec 16) | +11.6% | codec 16 only at `-x` | item 4 |
| FASTQ (Mac) | decode speed | CRAM 3.1 | 0.25× decode speed | arithmetic-coded family streams | — |
| audio (Mac) | decode speed | FLAC | 0.25× (p0) to 0.08× (p1) | adaptive coders | — |
| lines of text: logs, long tables | `ratio` | GR6+ / GR6-L3 (Kolmo's own) | 7–30% | GR6 only at `-x` | item 3 |

Fixed in this branch: `-x` was larger than `ratio` on osdb (+1.0%) and owid-energy (+1.0%) because its LZMA2 and
block-sorting alternatives were skipped by a pessimistic slice test (R0 in the plan).

Not measured here (no build or no data on this VM): CRAM, ZipNN, OptimFROG/Monkey's Audio, zpaq/paq8/cmix, DNA
specialists, JPEG/deflate recompressors. Where the Mac measured one, the table cites `benchmark/rc2/results/mt/MT.md`.

## GPU: alone or beside the CPU threads

| work | measured | verdict |
|---|---|---|
| suffix sort / BWT (`ratio`, `-x` text) | M5: 0.30–0.84× of one P-core on text, logs, JSON, DNA; 1.05–2.16× on x-ray, sao, bf16 (which `ratio` does not block-sort) | no route uses it |
| GPU worker beside ten CPU threads (block scheduler) | M5: 0.91–0.98× of CPU-only on block-sorted text; no effect on one-block files | dropped from the default build |
| kvid strip prediction | M5: 12.3 ms (6 ms kernel + copies) vs 6.5 ms on ten threads | CPU threads |
| planner histograms | M5: slower below 16 MiB, +65 ms Metal set-up per process | CPU |
| GR6 two-tier design (static transform + interleaved rANS on the GPU; formula search; field hashing; dictionary sort-reduce) | unmeasured (6-bit study §11) | candidate speed tier for very large structured inputs already in GPU memory; formula search is the first part worth a GPU, after a CPU-thread version (plan) |

Sources: `benchmark/rc3/gpu/RESULTS.md`, `research/CODECS.md` (GPU rows), branch `claude/dazzling-maxwell-p8y4u0`
`research/FINDINGS.md` §11.

## Evidence index

| evidence | what |
|---|---|
| `benchmark/rc3/router/RESULTS.md` | this lane: the matrix (25 types × 6 levels × 7 general tools and the specialists), the `-x` fix, GR6-L3 against rc3 `-x`, the experiments behind the plan |
| `benchmark/rc3/router/matrix.jsonl`, `xnew.jsonl` | raw rows (sizes, times, routes) |
| `benchmark/rc3/mac/RESULTS.md` | rc3 against rc2 on the Apple M5, 1 and 10 threads |
| `benchmark/rc2/results/mt/MT.md` | rc2 against the leaders with all cores (CRAM, ZipNN, FFV1, JPEG XL, JPEG-LS, FLAC, zpaq) |
| `benchmark/rc3/gr6/RESULTS.md`, `benchmark/rc3/xmode/RESULTS.md`, `benchmark/rc3/bwt/RESULTS.md`, `benchmark/rc3/format2/RESULTS.md` | the codecs each route uses |
| `research/CODECS.md` | every codec and transform with its measured impact and verdict |
