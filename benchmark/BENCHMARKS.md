# kolmo benchmarks

All competitors are built from the **same vendored sources** (`vendor/`) with the **same
compiler and flags** as kolmo, so this is apples-to-apples — not a comparison against
whatever system packages are installed. Every output is decompressed and compared
byte-for-byte; speeds are best-of-repeated in-memory runs. Reproduce with:

```sh
make -j
benchmark/bench.sh data/owid-co2-data.csv        # one file; add -x / -X for the slower / archive tiers
```

Machine for the numbers below: 2-core Intel Xeon VM, gcc, `-O3 -march=native`, single thread
(`T=1`). Ratio = original ÷ compressed; speeds in MB/s (compress / decompress).

## Compression ratio by data type (higher is better)

| Data | kolmo fast | kolmo ultra | zstd -19 | brotli -11 | xz -9e | OpenZL* |
|---|---|---|---|---|---|---|
| Tables (OWID CO2 + energy, 23.6 MB) | **10.26** | 11.44 | 4.20 | 4.56 | 4.54 | 4.82 |
| Logs (16 Loghub logs, 4.5 MB) | **14.10** | 22.42 | 13.48 | 14.50 | 14.67 | 9.84 |
| Scientific (star catalogue + X-ray, 15.7 MB) | **2.64** | 2.76 | 1.55 | 1.70 | 1.76 | 2.56 |
| Text (enwik8, 100 MB) | 2.81 | **5.11** | 3.71 | 3.88 | 4.03 | 3.06 |
| Mixed (Silesia, 212 MB) | 3.64 | **5.58** | 4.01 | 4.27 | 4.38 | 3.35 |

\*OpenZL (Meta's format-aware framework) uses its own CLI with its best matching profile,
trained on the tables. See the cz-vs-OpenZL head-to-head: kolmo compresses smaller on every
dataset tested — including OpenZL's own SAO benchmark (3.15× vs 2.09×) — with zero configuration.

## Full mode sweep — tables (reproducible with `bench.sh`)

`owid-co2-data.csv` (14.4 MB):

| codec | ratio | compress MB/s | decompress MB/s |
|---|---|---|---|
| lz4 | 1.95 | 376 | 2616 |
| zstd -1 | 3.50 | 240 | 1015 |
| zstd -3 | 3.17 | 168 | 842 |
| zstd -9 | 3.57 | 40 | 919 |
| brotli -8 | 3.55 | 6.3 | 315 |
| xz -6 | 4.62 | 1.3 | 64 |
| **kolmo fast** | **10.63** | 138 | 480 |
| kolmo hc | 11.98 | 12 | 316 |
| kolmo ultra | 12.68 | 0.4 | 128 |

`owid-energy-data.csv` (9.2 MB):

| codec | ratio | compress MB/s | decompress MB/s |
|---|---|---|---|
| zstd -3 | 3.37 | 178 | 1083 |
| xz -6 | 4.34 | 1.7 | 79 |
| **kolmo fast** | **9.74** | 89 | 553 |
| kolmo hc | 10.66 | 10 | 560 |
| kolmo ultra | 11.18 | 0.4 | 124 |

## Decode scales with cores (what zstd can't do)

kolmo's blocks are independent, so decode and compress scale with cores; zstd's single
stream is flat. On a 4-core Apple-silicon build, `kolmo fast` decompresses **faster than
`zstd -3` on every data type** while leading ratio:

| decompress, 4 cores (MB/s) | kolmo fast | zstd -3 |
|---|---|---|
| CO₂ table | 3644 | 1565 |
| SAO (scientific) | 6371 | 1236 |
| Logs | 8486 | 4063 |

## Notes

- The archive tiers (`max`, `ultra`) trade speed for ratio and run at well under 1 MB/s/core.
- Output is byte-identical across x86-64 and Apple silicon.
- Numbers move a little with machine and build; `bench.sh` regenerates them on yours.
