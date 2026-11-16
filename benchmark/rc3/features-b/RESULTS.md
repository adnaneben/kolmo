# rc3 features-b: queryable archives and cross-file deduplication

Lane `features-b` of `research/RC3-PLAN.md`. Formats: `SPEC.md`, "Container extensions" (KEXT, column-split table
blocks, IDX1, DDP1). Code: `src/core/extension.c`, `src/core/index.c`, `src/core/dedup.c`, `src/core/features.h`,
`src/transforms/table_cols.c`, `src/cli/features.c`; hooks in `core/blocks.c`, `core/compress.c`, `core/decode.c`,
`plan/plan.h`, `api/api.c`. Tests: `make check-features-b` (part of `make check-rc`). Raw results: `results/*.json`.

## Machine and protocol

- Cloud VM, Linux 6.18, 4 vCPU "Intel Xeon Processor @ 2.10GHz", 15 GiB RAM, gcc 13.3, `make` default flags
  (`-O3`). Not the protocol's Mac: these numbers are for comparing tools on one machine, not for the rc3 report's
  cross-machine tables.
- Following `benchmark/rc2/PROTOCOL.md`: one thread unless a row says T4 (4 threads); the two are never mixed in one
  comparison. Whole-process wall time. Queries: best of 3. Dedup: one run per row (each row takes from 0.1 s to 18 min;
  sizes are exact, times within roughly 10–20% on this VM).
- Tools: zstd 1.5.5, borg 1.4.5 (pip, in a venv), restic 0.16.4 (`GOMAXPROCS=1` for one thread), GNU tar 1.35.
- Kolmo: this branch. Sizes were measured before the merge of `claude/rc3-structure-20261116` and checked after it:
  the merge does not change any frame (same sizes; the determinism check passes).
- Every query answer was compared with the answer of a full scan (`--no-index`), and every restored archive with
  the original files (`cmp`).

## Data

| set | what | source |
|---|---|---|
| owid-co2-data.csv | 15.2 MB, 50,411 rows × 79 columns, quoted fields | Our World in Data, `raw.githubusercontent.com/owid/co2-data/master/owid-co2-data.csv` (16 Nov 2026) |
| sensor.csv | 153 MB, 3,000,000 rows × 8 columns | generated, `gen_query_sets.py` (seed 1) |
| sensor.jsonl | 87.5 MB, 1,000,000 JSON objects | generated, `gen_query_sets.py` (seed 5) |
| hdfs256.log | 268 MB, first 256 MiB of the HDFS_v1 log (whole lines) | loghub, `zenodo.org/records/8196385/files/HDFS_v1.zip` |
| snapshots | 10 daily CSV exports, +5000 rows and 200 edited rows a day | generated, `gen_sets.py` |
| srctree | 5 versions of this repository (`git archive`, 20 commits apart), 5519 files | `gen_sets.py` |
| finetune | 8 checkpoints of a 48 MiB float32 model, each step changes 20% of the tensors | generated, `gen_sets.py` (seed 11) |
| growlog | 6 hourly backups of a growing log (first 24, 48 … 144 MiB of the HDFS log) | `gen_sets.py` |
| pythia | 3 consecutive checkpoints of EleutherAI pythia-70m (steps 141000, 142000, 143000; float32 safetensors) | huggingface.co |

Downloads were kept in their own directories and read with `python3 -I`.

## 1. Queryable archives (IDX1)

`kolmo --index FILE OUT.kol` writes a normal frame (any Kolmo reader decodes it) with line-aligned blocks of 4 MiB,
table blocks split into one coded segment per column, and an IDX1 section in the KEXT trailer: per block the line
count and line-boundary flags, the column names, and a filter of case-folded trigrams and whole tokens (4 bits per
token). `kolmo cat --col NAME --rows A-B` and `kolmo grep` then decode only what they need:

- a column of a CSV/TSV is read from its own segment, the other columns are not decoded;
- a row range decodes only the blocks that hold it (found from the line counts);
- a grep decodes only the blocks whose filter may hold the pattern's literal (for `-E` patterns: the longest literal
  the regex requires; none for alternations, so every block).

### Size of the index

Indexed frames against the plain balanced frame (default blocks). The filter costs most on JSON lines, whose tokens
(timestamps, values) are almost all unique.

| file | bytes | plain balanced | indexed | indexed, no filter | zstd -19 | full decode T1 (plain / indexed) |
|---|---:|---:|---:|---:|---:|---:|
| owid-co2-data.csv | 15,242,657 | 1,392,694 | 1,557,302 (+11.8%) | 1,480,072 (+6.3%) | 3,605,469 | 0.062 / 0.054 s |
| sensor.csv | 153,379,795 | 15,973,611 | 16,678,431 (+4.4%) | 16,288,294 (+2.0%) | 26,424,390 | 0.367 / 0.411 s |
| hdfs256.log | 268,435,415 | 15,545,055 | 16,139,162 (+3.8%) | 15,545,374 (+0.0%) | 18,317,969 | 0.360 / 0.472 s |
| sensor.jsonl | 87,542,646 | 2,631,383 | 3,361,817 (+27.8%) | 2,631,528 (+0.0%) | 6,180,457 | 0.140 / 0.163 s |


Column split alone, against plain frames with the same 4 MiB blocks (indexed with `--no-filter`):

| file | balanced | hc | max |
|---|---:|---:|---:|
| owid-co2-data.csv | +7.4% (1,480,072 vs 1,378,723) | +9.3% (1,452,671 vs 1,329,343) | +11.2% (1,386,183 vs 1,246,048) |
| sensor.csv | +2.0% (16,288,294 vs 15,973,611) | −0.3% (15,774,073 vs 15,815,764) | −0.0% (15,087,345 vs 15,089,281) |

OWID pays more because it has 79 columns, many sparse: each segment has its own codec frame and check, and the
codec loses the context between neighbouring columns.

Filter bits per token on the HDFS log (blocks ruled out of 65 for one block id):

| k | index overhead | `grep -F -w 1660256537217386052` | `grep -F -w blk_7503483334202473044` |
|---:|---:|---:|---:|
| 1 | +1.09% | 34 ruled out | 44 |
| 2 | +2.00% | 51 | 49 |
| 3 | +2.91% | 59 | 53 |
| 4 (default) | +3.82% | 60 | 54 |
| 6 | +5.64% | 59 | 58 |

### Queries against a full scan

"scan" is the same query with `--no-index`: every block decoded, lines processed in order (the same as decoding and
filtering, without writing the file). Times in seconds, T1 and T4. "zstd -dc | grep" is the usual way with a
zstd -19 file and the same grep options (T1).

| file | query | blocks (total / decoded / by column / ruled out) | bytes decoded (index) | stored bytes read | index T1 | scan T1 | index T4 | scan T4 | zstd -dc \| grep |
|---|---|---|---:|---:|---:|---:|---:|---:|---:|
| owid | `cat --col co2` | 4 / 0 / 4 / 0 | 795,866 | 56,193 | 0.008 | 0.110 | 0.005 | 0.090 | — |
| owid | `cat --col co2 --rows 50000-51000` | 4 / 0 / 1 / 0 | 129,987 | 10,652 | 0.003 | 0.103 | 0.003 | 0.080 | — |
| owid | `cat --rows 30000-30100` | 4 / 1 / 0 / 0 | 4,194,249 | 425,008 | 0.018 | 0.033 | 0.019 | 0.027 | — |
| owid | `grep -F -w Morocco` | 4 / 1 / 0 / 3 | 4,194,226 | 378,821 | 0.019 | 0.053 | 0.019 | 0.028 | 0.031 |
| owid | `grep -c -E '^Mor[a-z]+,20[12][0-9],'` | 4 / 2 / 0 / 2 | 8,388,518 | 777,211 | 0.035 | 0.058 | 0.021 | 0.031 | 0.027 |
| sensor | `cat --col co2` | 37 / 0 / 37 / 0 | 12,001,923 | 3,401,394 | 0.167 | 0.585 | 0.082 | 0.576 | — |
| sensor | `cat --col co2 --rows 1000000-1010000` | 37 / 0 / 1 / 0 | 330,773 | 91,298 | 0.006 | 0.149 | 0.005 | 0.146 | — |
| sensor | `cat --rows 2500000-2500100` | 37 / 1 / 0 / 0 | 4,194,263 | 443,018 | 0.018 | 0.341 | 0.016 | 0.250 | — |
| sensor | `grep -c -F -w fault` | 37 / 37 / 0 / 0 | 153,379,795 | 16,287,398 | 0.439 | 0.484 | 0.217 | 0.373 | 0.261 |
| sensor | `grep -c -F 1700012345` | 37 / 37 / 0 / 0 | 153,379,795 | 16,287,398 | 0.456 | 0.483 | 0.213 | 0.376 | 0.380 |
| hdfs | `cat --rows 1000000-1000100` | 65 / 1 / 0 / 0 | 4,194,270 | 191,624 | 0.011 | 0.232 | 0.015 | 0.176 | — |
| hdfs | `cat --col '#4' --rows 1000000-1100000` | 65 / 4 / 0 / 0 | 16,776,973 | 776,887 | 0.033 | 0.240 | 0.025 | 0.209 | — |
| hdfs | `grep -F -w 1660256537217386052` | 65 / 5 / 0 / 60 | 20,971,022 | 1,288,737 | 0.040 | 0.571 | 0.032 | 0.402 | 0.240 |
| hdfs | `grep -c -F WARN` | 65 / 58 / 0 / 7 | 243,265,769 | 14,384,502 | 0.539 | 0.485 | 0.212 | 0.459 | 0.241 |
| hdfs | `grep -c -E 'blk_-?16089[0-9]+'` | 65 / 64 / 0 / 1 | 268,431,288 | 15,543,525 | 0.429 | 0.480 | 0.149 | 0.342 | 0.288 |
| json | `cat --col co2` | 21 / 21 / 0 / 0 | 87,542,646 | 2,631,015 | 0.233 | 0.268 | 0.121 | 0.220 | — |
| json | `cat --col co2 --rows 400000-410000` | 21 / 1 / 0 / 0 | 4,194,295 | 125,856 | 0.013 | 0.074 | 0.014 | 0.084 | — |
| json | `grep -c -F -w maint` | 21 / 21 / 0 / 0 | 87,542,646 | 2,631,015 | 0.166 | 0.160 | 0.078 | 0.153 | 0.112 |


Reading:

- **Columns of a CSV: 3.5–14× faster than a scan** at T1 (OWID co2: 0.008 s vs 0.110 s; sensor co2: 0.167 s vs
  0.585 s; 7× at T4), decoding 5% (OWID) and 8% (sensor) of the bytes. With a row range the gain is larger (sensor 10k rows: 0.006 s vs
  0.149 s, 330 KB decoded instead of 54 MB).
- **Row ranges: one block** decoded (4 MiB), 2–21× faster depending on how deep the rows are (the scan can stop early).
- **Selective grep** (a rare id): HDFS block id 0.040 s vs 0.571 s scan and 0.240 s for `zstd -dc | grep`; 60 of 65
  blocks ruled out.
- **No gain** for frequent words (`fault` in every block, `WARN` in 58 of 65), for the JSON key column (JSON objects
  have no column segments: each block is decoded whole), and for `-E` alternations. At T4 the indexed
  query is still 1.7–2.3× faster than the scan (0.08–0.22 s vs 0.15–0.46 s: the index lets every worker start at a
  known line, while the scan joins lines across blocks in order), but at T1 it is the same or slightly slower (HDFS `WARN`: 0.539 s vs 0.485 s), and `zstd -dc | grep` (GNU grep) is faster
  on one thread.
- Full decode of an indexed frame is up to 31% slower at T1 (HDFS; OWID is 13% faster; smaller blocks, the column
  segments) and within ±16% at T4.

## 2. Cross-file deduplication (DDP1)

`kolmo a ARCHIVE FILES|DIRS…` cuts every file into content-defined chunks (FastCDC with a Gear table, normalised
chunking, average 4 KiB by default (16 KiB when these tables were measured); on text the cut moves to the next line end within 4 KiB), keeps one copy of each
distinct chunk (XXH3-64, confirmed with memcmp), and compresses the unique chunks in file order as one ordinary Kolmo
frame, so the typed transforms (tables, logs, floats) still apply to unique data. The file table (names, modes,
times, chunk lists) is a second frame. `kolmo x` restores everything or the named files (decoding only their blocks);
`kolmo l` lists. Rows "kolmo, no dedup" compress each file on its own; "zstd -19 --long=27, per file" likewise.


#### snapshots: 10 files, 42,818,594 bytes

| tool | size | ratio | compress s | extract s | restored |
|---|---:|---:|---:|---:|---|
| kolmo a, balanced, 16 KiB chunks | 5,747,643 | 7.45× | 0.28 | 0.11 | ok |
| kolmo a, balanced, 4 KiB chunks | 3,043,747 | 14.07× | 0.13 | 0.07 | ok |
| kolmo a, hc, 4 KiB chunks | 1,187,399 | 36.06× | 2.99 | 0.06 | ok |
| kolmo, balanced, no dedup (one frame per file) | 10,030,328 | 4.27× | 0.33 | 0.19 | ok |
| zstd -19 --long=27, per file | 15,553,973 | 2.75× | 35.78 | 0.19 | ok |
| tar | zstd -3 --long=31 | 2,109,640 | 20.30× | 0.19 | 0.07 | ok |
| tar | zstd -19 --long=31 | 1,773,721 | 24.14× | 4.75 | 0.09 | ok |
| borg, zstd,3 (default chunker) | 18,256,712 | 2.35× | 1.21 | 0.35 | ok |
| borg, zstd,19 (default chunker) | 15,405,606 | 2.78× | 20.49 | 0.38 | ok |
| borg, zstd,19, 64 KiB chunks | 14,828,384 | 2.89× | 12.39 | 0.42 | ok |
| restic, compression max | 16,798,095 | 2.55× | 8.47 | 0.88 | ok |
| kolmo a, balanced, 16 KiB chunks, T4 | 5,747,643 | 7.45× | 0.14 | 0.06 | ok |
| tar | zstd -19 --long=31, T4 | 1,773,721 | 24.14× | 5.21 | 0.07 | ok |
| restic, compression max, all threads | 16,788,385 | 2.55× | 3.80 | 0.72 | ok |

#### srctree: 5519 files, 135,341,460 bytes

| tool | size | ratio | compress s | extract s | restored |
|---|---:|---:|---:|---:|---|
| kolmo a, balanced, 16 KiB chunks | 3,337,588 | 40.55× | 0.48 | 0.22 | ok |
| kolmo a, balanced, 4 KiB chunks | 3,327,606 | 40.67× | 0.52 | 0.50 | ok |
| kolmo a, hc, 4 KiB chunks | 2,552,217 | 53.03× | 5.78 | 1.21 | ok |
| kolmo, balanced, no dedup (one frame per file) | 35,204,641 | 3.84× | 29.94 | 23.65 | **mismatch** |
| zstd -19 --long=27, per file | 29,921,477 | 4.52× | 96.86 | 26.12 | ok |
| tar | zstd -3 --long=31 | 3,262,627 | 41.48× | 0.44 | 2.62 | ok |
| tar | zstd -19 --long=31 | 2,521,081 | 53.68× | 15.79 | 2.32 | ok |
| borg, zstd,3 (default chunker) | 4,605,800 | 29.39× | 1.17 | 3.60 | ok |
| borg, zstd,19 (default chunker) | 4,047,273 | 33.44× | 7.94 | 3.48 | ok |
| borg, zstd,19, 64 KiB chunks | 4,119,619 | 32.85× | 9.17 | 3.47 | ok |
| restic, compression max | 4,613,462 | 29.34× | 5.06 | 3.62 | ok |
| kolmo a, balanced, 16 KiB chunks, T4 | 3,337,588 | 40.55× | 0.57 | 2.25 | ok |
| tar | zstd -19 --long=31, T4 | 2,521,081 | 53.68× | 8.27 | 2.33 | ok |
| restic, compression max, all threads | 4,613,523 | 29.34× | 3.94 | 1.95 | ok |

#### finetune: 8 files, 436,492,288 bytes

| tool | size | ratio | compress s | extract s | restored |
|---|---:|---:|---:|---:|---|
| kolmo a, balanced, 16 KiB chunks | 124,643,847 | 3.50× | 1.08 | 0.69 | ok |
| kolmo a, balanced, 4 KiB chunks | 121,150,185 | 3.60× | 0.81 | 0.41 | ok |
| kolmo a, hc, 4 KiB chunks | 121,131,193 | 3.60× | 2.73 | 0.76 | ok |
| kolmo, balanced, no dedup (one frame per file) | 362,792,877 | 1.20× | 1.14 | 0.59 | ok |
| zstd -19 --long=27, per file | 403,669,032 | 1.08× | 177.82 | 0.96 | ok |
| tar | zstd -3 --long=31 | 133,904,250 | 3.26× | 1.04 | 0.71 | ok |
| tar | zstd -19 --long=31 | 134,026,374 | 3.26× | 67.30 | 0.70 | ok |
| borg, zstd,3 (default chunker) | 220,155,791 | 1.98× | 1.89 | 1.47 | ok |
| borg, zstd,19 (default chunker) | 220,294,457 | 1.98× | 40.08 | 1.60 | ok |
| borg, zstd,19, 64 KiB chunks | 138,365,713 | 3.15× | 13.88 | 1.82 | ok |
| restic, compression max | 172,262,279 | 2.53× | 7.30 | 1.71 | ok |
| kolmo a, balanced, 16 KiB chunks, T4 | 124,643,847 | 3.50× | 0.73 | 0.51 | ok |
| tar | zstd -19 --long=31, T4 | 134,026,374 | 3.26× | 27.42 | 0.97 | ok |
| restic, compression max, all threads | 180,152,950 | 2.42× | 4.81 | 1.27 | ok |

#### growlog: 6 files, 528,482,078 bytes

| tool | size | ratio | compress s | extract s | restored |
|---|---:|---:|---:|---:|---|
| kolmo a, balanced, 16 KiB chunks | 7,618,586 | 69.37× | 1.29 | 2.08 | ok |
| kolmo a, balanced, 4 KiB chunks | 7,756,837 | 68.13× | 1.30 | 0.57 | ok |
| kolmo a, hc, 4 KiB chunks | 6,571,540 | 80.42× | 18.92 | 1.98 | ok |
| kolmo, balanced, no dedup (one frame per file) | 26,647,586 | 19.83× | 2.59 | 0.86 | ok |
| zstd -19 --long=27, per file | 32,697,939 | 16.16× | 402.41 | 0.80 | ok |
| tar | zstd -3 --long=31 | 14,160,331 | 37.32× | 1.22 | 0.81 | ok |
| tar | zstd -19 --long=31 | 9,494,982 | 55.66× | 146.54 | 0.86 | ok |
| borg, zstd,3 (default chunker) | 15,606,616 | 33.86× | 1.84 | 1.68 | ok |
| borg, zstd,19 (default chunker) | 10,515,108 | 50.26× | 81.84 | 1.43 | ok |
| borg, zstd,19, 64 KiB chunks | 12,814,191 | 41.24× | 90.62 | 1.49 | ok |
| restic, compression max | 12,889,291 | 41.00× | 8.03 | 1.31 | ok |
| kolmo a, balanced, 16 KiB chunks, T4 | 7,618,586 | 69.37× | 0.82 | 1.54 | ok |
| tar | zstd -19 --long=31, T4 | 9,494,982 | 55.66× | 47.23 | 1.03 | ok |
| restic, compression max, all threads | 12,681,489 | 41.67× | 5.49 | 1.74 | ok |

#### pythia: 3 files, 845,145,528 bytes

| tool | size | ratio | compress s | extract s | restored |
|---|---:|---:|---:|---:|---|
| kolmo a, balanced, 16 KiB chunks | 383,996,412 | 2.20× | 7.39 | 2.62 | ok |
| kolmo a, balanced, 4 KiB chunks | 384,749,413 | 2.20× | 7.85 | 3.19 | ok |
| kolmo a, hc, 4 KiB chunks | 362,084,394 | 2.33× | 300.25 | 3.51 | ok |
| kolmo, balanced, no dedup (one frame per file) | 383,694,215 | 2.20× | 4.68 | 2.46 | ok |
| zstd -19 --long=27, per file | 416,263,917 | 2.03× | 1073.41 | 3.28 | ok |
| tar | zstd -3 --long=31 | 484,376,638 | 1.74× | 9.00 | 2.34 | ok |
| tar | zstd -19 --long=31 | 415,375,404 | 2.03× | 999.88 | 3.12 | ok |
| borg, zstd,3 (default chunker) | 484,810,089 | 1.74× | 9.62 | 3.29 | ok |
| borg, zstd,19 (default chunker) | 435,606,084 | 1.94× | 447.61 | 3.81 | ok |
| borg, zstd,19, 64 KiB chunks | 473,962,673 | 1.78× | 177.68 | 3.54 | ok |
| restic, compression max | 474,218,056 | 1.78× | 96.49 | 5.68 | ok |
| kolmo a, balanced, 16 KiB chunks, T4 | 383,996,412 | 2.20× | 3.07 | 1.26 | ok |
| tar | zstd -19 --long=31, T4 | 415,375,404 | 2.03× | 299.08 | 2.70 | ok |
| restic, compression max, all threads | 474,165,698 | 1.78× | 26.44 | 3.09 | ok |


Reading:

- **CSV snapshots: Kolmo leads only with small chunks and `-m hc`** (36.1× at 4 KiB, against 24.1× for
  `tar | zstd -19 --long=31`). At balanced it loses: 14.1× at 4 KiB and 7.45× at 16 KiB, because the 200 daily
  edits are spread over the file and touch most 16 KiB chunks, while the long-window LZ matches around them. borg
  and restic stay below 3× (their chunks average 0.5–2 MiB, larger than the gap between edits, and they compress
  chunks one by one). - **Checkpoints and logs: Kolmo leads.** Checkpoint series 3.50–3.60× vs 3.26× (zstd) and 3.15× (borg, 64 KiB chunks); the
  pythia checkpoints 2.20× vs 2.03× (no chunk repeats between steps: all tensors change; the gain is the float
  transform). Growing logs 69.4× vs 55.7×, at 1.3 s instead of 147 s.
- **Source trees: tar | zstd -19 --long=31 wins** (53.7× vs 40.6× at balanced; `-m hc` 4 KiB 53.0×). The files are small
  and their repeats are whole files that the long-window LZ also finds; the chunk table costs space.
- **Speed**: `kolmo a` at balanced is 0.1–7.9 s on these sets at one thread (0.1–3.1 s at T4), faster than borg and
  restic in every row and 17–135× faster than `tar | zstd -19 --long=31` (which it beats in size on three sets of
  five). Extraction is 0.06–3.5 s.
- 4 KiB chunks against 16 KiB: smaller on snapshots (−47%), checkpoints (−2.8%) and source trees (−0.3%), slightly
  larger on growing logs (+1.8%) and pythia (+0.2%), at about the same speed. After these runs the default was
  changed from 16 KiB to 4 KiB (`--chunk 16` gives the measured 16 KiB rows; the format stores the size).

## What failed or was dropped

- **Per-column graph tables** (forcing the graph route so each column becomes a graph leaf) were +58% on OWID and
  were dropped; the column split of the `tbl2` stream (tflag 13) replaced them.
- **A trigram-only filter** saturated on logs (every block held every trigram of an id); whole-token keys and `-w`
  were added.
- **`kolmo, no dedup` on srctree: mismatch.** One file of the tree,
  `benchmark/rc2/results/mt/M1-numeric_npy.jsonl` (76,542 bytes), fails its checksum on decode at balanced, eco,
  hc and ratio. The bug predates this lane: it reproduces on the base commit (`2207da1`) and on rc2, in the JSON-lines
  log transform (plan T_LOG, tflag 5). It was reported as a separate task, not fixed here. `kolmo a` restored the same
  tree correctly (there the file is one part of a larger frame).
- The first benchmark driver passed every file name on one command line and failed on srctree ("Argument list too
  long"); `kolmo a` now takes directories and the driver passes the top-level entries.
- The first run of the regex queries failed because `kolmo grep` did not accept `-E`; it now does (extended regular
  expressions are the default) and those two rows were measured again after the merge of the structure branch.
- The first T4 extraction time of srctree (2.25 s) overlapped with a test run; measured again on a quiet machine:
  0.12 s (T1) and 0.11 s (T4), file creation dominating.
- Some pushes of the lane before the last one ran only `make check-features-b`, the CLI test and the determinism
  check instead of the whole plan list. The full list was run on the final head (below).

## Commands

```sh
make all
python3 -I benchmark/rc3/features-b/gen_query_sets.py DATA/gen
python3 -I benchmark/rc3/features-b/gen_sets.py DATA DATA/loghub/hdfs256.log .
python3 -I benchmark/rc3/features-b/bench_query.py $PWD/build/kolmo query.json \
    owid=DATA/owid/owid-co2-data.csv sensor=DATA/gen/sensor.csv hdfs=DATA/loghub/hdfs256.log json=DATA/gen/sensor.jsonl
BORG=/path/to/borg python3 -I benchmark/rc3/features-b/bench_dedup.py $PWD/build/kolmo DATA dedup.json
# by hand
build/kolmo --index -T4 file.csv file.kol
build/kolmo cat --stats --col co2 --rows 1000-2000 file.kol
build/kolmo grep --stats -F -w Morocco file.kol
build/kolmo a --chunk 4 backup.kol day*.csv && build/kolmo x -C out backup.kol
```

## Tests

`make check-features-b` (in `make check-rc`): 3,708 checks optimised and under ASan/UBSan — CSV with quotes,
brackets, irregular and long rows, TSV, JSON lines, logs; every query against a reference implementation, with
the index and without; damaged, truncated and forged indexes (line counts, flags, filters, section CRCs) must give
an error or the right answer, never a wrong one; column-split round trips in modes 0–4 and damaged segments; DDP1
round trips and damage. `tests/test_features_b_cli.sh`: the commands end to end, unsafe archive names refused.
