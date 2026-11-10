# lzbench results

`silesia-2026-11-07.csv`: lzbench 2.4.1, full Silesia corpus (`silesia.tar`, 211,957,760 bytes),
one thread, on a 2-core Intel Xeon at 2.1 GHz (cloud VM), GCC 13.3, every codec at `-O3 -march=native`.
Fast and medium codecs ran with a 2 s minimum per measurement (`-t2,2`). The context-mixing tier
(kolmo 5, kanzi 7-9, zpaq 4-5) ran one pass each (`-i1,1 -t0,0`). glza was stopped after 30 minutes
of its first compression pass (it also runs two threads) and is not included. lzbench verified every
round trip. The "Ratio" column is compressed size as a percentage of the original (lower is better).

Reproduce: `sh benchmark/lzbench/setup.sh <lzbench checkout>` and run the same `-e` lists.

## `silesia-2026-11-07-ultra-cm2.csv` — context-mixing model 2 (commit `eecdc83`)

kolmo level 9 (`ultra`) after the CM model-2 work, same harness and machine: **17.09% (5.85×)**, up from
17.91% (5.58×); zpaq -5 is 19.09% (5.24×). Compression and decompression run at 0.20 MB/s (was 0.28 / 0.37).

## `silesia-2026-11-08-ultra-cm2-hugepages.csv` — model 2 final + huge pages (commit `626dcb7`)

kolmo level 9 (`ultra`): **17.01% (5.88×)** at 0.25 MB/s compress, 0.29 MB/s decompress (1.0.0: 17.91% at
0.28 / 0.37 MB/s; zpaq -5: 19.09% at 0.39 / 0.39 MB/s).

## `silesia-2026-11-08-fast-quick.csv` — quick analysis (`-q`) for the fast tier

New ladder levels 1–3 use quick analysis (`kolmo -m fast -l N -q`): only the cheap structure transforms, scored with
zstd -1 on a 256 KiB slice. Two rounds on this VM (speeds vary by about ±15% between rounds; ratios are exact):

| Entry | Ratio | Compress (2 rounds) | Decompress |
|---|---|---|---|
| zstd -1 | 34.53% | 444–459 MB/s | 1.2–1.5 GB/s |
| zstd -2 | 32.70% | 374–380 MB/s | 1.15–1.42 GB/s |
| **kolmo -1** (`fast -l1 -q`) | **30.33%** | 324–382 MB/s | 0.97–1.15 GB/s |
| zstd -3 | 31.20% | 210–232 MB/s | 0.98–1.05 GB/s |
| **kolmo -2** (`fast -l2 -q`) | **28.89%** | 294 MB/s | 0.99–1.15 GB/s |
| **kolmo -3** (`fast -q`) | **27.87%** | 210–234 MB/s | 1.1 GB/s |
| zstd -5 | 29.60% | 110–121 MB/s | 0.88–0.91 GB/s |
| kolmo -4 (`fast`) | 27.49% | 132–141 MB/s | 0.83–0.98 GB/s |

## `silesia-2026-11-08-bwt-tier.csv` — `max -l 1` (BWT for every block)

New ladder level 7 (`kolmo -m max -l 1`): kolmo's transforms with libbsc's BWT on every block (default `max` sends
binary blocks to LZMA2, which is ~8x slower). One run, same harness:

| Entry | Ratio | Compress | Decompress |
|---|---|---|---|
| **kolmo `max -l 1`** | **20.93%** | **21.6 MB/s** | **33.2 MB/s** |
| bsc -m0 -e1 | 23.26% | 15.4 MB/s | 22.4 MB/s |
| kanzi -7 | 22.32% | 10.7 MB/s | 16.1 MB/s |
| bzip3 -5 | 22.29% | 9.4 MB/s | 9.7 MB/s |

It beats all three on ratio, compression speed and decompression speed at once.

## `silesia-2026-11-08-ladder.csv` — the kolmo ladder (levels 0–9 of the 0–10 ladder at the time; `ultra` measured separately)

Measured before `max -l 1` was inserted as level 7, so levels 7–9 there are `eco`, `hc` and `max`.

## `silesia-2026-11-08-max-ultra-levels.csv` — levels 7, 10, 11 of the 0–12 ladder (one pass each)

| Level | Setting | Ratio | Compress | Decompress |
|---|---|---|---|---|
| 7 | `max -l 1` | 20.93% | 19.4 MB/s | 32.7 MB/s |
| 10 | `max` (now also BWT for binary blocks when smaller) | 19.34% | 2.67 MB/s | 53.8 MB/s |
| 11 | `ultra -l 1` (CM model 1 only) | **17.99%** | **0.43 MB/s** | **0.45 MB/s** |

`ultra -l 1` beats zpaq -5 (19.09%, 0.39 / 0.39 MB/s) on ratio, compression speed and decompression speed at once;
`ultra` (level 12, 17.01%) holds the ratio crown.

## `silesia-2026-11-08-ultra-final.csv` — `ultra` with sample-gated alternatives (commit `ce349f0`)

Level 12 (`ultra`): **17.01% at 0.28 MB/s compress, 0.30 MB/s decompress** — 1.0.0's compression speed (0.28) with
5% smaller output (1.0.0: 17.91%).

## `silesia-2026-11-08-kolmo-ladder-final.csv` — the full 0–12 ladder at the head of `claude/perf`

Levels 0–10 in one run (`-t2,2`); levels 11 and 12 from their single-pass runs above.

| Level | Setting | Ratio | Compress | Decompress |
|---|---|---|---|---|
| 0 | `rt` | 46.58% | 592 MB/s | 2470 MB/s |
| 1 | `fast -l1 -q` | 30.33% | 387 MB/s | 1171 MB/s |
| 2 | `fast -l2 -q` | 28.89% | 361 MB/s | 1165 MB/s |
| 3 | `fast -q` | 27.87% | 191 MB/s | 1002 MB/s |
| 4 | `fast` | 27.49% | 145 MB/s | 899 MB/s |
| 5 | `hc -l 9` | 24.70% | 47.8 MB/s | 726 MB/s |
| 6 | `hc -l 12` | 24.30% | 26.9 MB/s | 694 MB/s |
| 7 | `max -l 1` | 20.93% | 21.6 MB/s | 32.6 MB/s |
| 8 | `eco` | 23.88% | 8.45 MB/s | 343 MB/s |
| 9 | `hc` | 22.47% | 2.46 MB/s | 645 MB/s |
| 10 | `max` | 19.34% | 2.81 MB/s | 50.6 MB/s |
| 11 | `ultra -l 1` | 17.99% | 0.43 MB/s | 0.45 MB/s |
| 12 | `ultra` | 17.01% | 0.28 MB/s | 0.30 MB/s |

## 2026-11-10: kolmo 1.1.0 against the field

- `silesia-2026-11-10-v1.1.0-fast.csv`, `silesia-2026-11-10-v1.1.0-slow.csv`: lzbench on
  silesia.tar, single thread, 2-core cloud VM (timings vary 10 to 15% between runs).
  kolmo 1.1.0 levels 0 to 10 next to lz4, libdeflate, zstd, brotli, OpenZL 0.2.3, xz, bsc,
  bzip3, ppmd8, kanzi and zpaq. Sizes are identical to 1.0.x at every level: none of the
  1.1.0 transforms fire inside a tar.
- `panel-2026-11-10-v1.1.0-vs-openzl.csv`: the 32-file OpenZL panel, one row per file and
  compressor (ratio, encode MB/s, decode MB/s). kolmo 1.0.x and 1.1.0 were run back to
  back through the CLI; OpenZL rows were measured on the same VM earlier the same day.
  Files under 1 MB mostly measure process start-up.
  At fast, 1.1.0 is smaller than OpenZL's best profile on 17 of 32 files, encodes faster
  on 21 and decodes faster on 28; at max it is smaller on 29.
