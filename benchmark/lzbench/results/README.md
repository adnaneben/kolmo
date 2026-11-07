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
