# Combined experimental build against main

The build at 38d512f5 combines the native Gear archive scanner, decoder/writer fixes, optional typed-table routing, KLZ capacity changes and the offline learning tooling. This is a research build. Dynamic policy training does not run on every input. The experiment compares a historical main, qualified main, a second copy of the same main executable, and the combined build. Each setting includes a warmup and eleven shuffled scored trials of every arm. Timed runs require byte-exact recovery, and the warmup cross-checks each emitted frame/archive with the stock main reader. The enwik8 archive also checks stored mode and mtime.

| Input | Level | T | Stored B pre / main / combined | Encode MB/s pre / main / combined | Decode MB/s pre / main / combined | Combined MB/s pre / main / combined | Paired combined Δ vs main / AA | A/A spread |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| enwik8.zip | 1 | 1 | 36,492,173 / 36,492,173 / 36,492,173 | 479.69 / 471.10 / 535.41 | 1979.46 / 1910.81 / 1932.86 | 387.14 / 381.20 / 419.27 | +10.0% / +7.7% | +0.8% |
| enwik8.zip | 1 | 4 | 36,492,173 / 36,492,173 / 36,492,173 | 601.61 / 614.54 / 786.33 | 2480.88 / 2333.17 / 2546.82 | 485.99 / 494.99 / 614.56 | +17.7% / +18.6% | -2.0% |
| owid-co2.csv | 1 | 1 | 6,724,159 / 6,724,159 / 2,210,685 | 378.77 / 350.68 / 270.98 | 1310.14 / 1189.89 / 646.58 | 286.15 / 263.12 / 187.01 | -32.0% / -33.0% | -4.0% |
| owid-co2.csv | 1 | 4 | 6,724,159 / 6,724,159 / 2,210,685 | 815.41 / 764.81 / 536.63 | 1452.68 / 1719.27 / 1079.77 | 528.05 / 530.81 / 365.72 | -32.5% / -31.1% | +4.2% |
| owid-co2.csv | 2 | 1 | 4,393,006 / 4,393,006 / 1,927,296 | 313.62 / 311.96 / 257.36 | 974.85 / 732.00 / 611.49 | 232.74 / 223.38 / 178.95 | -17.7% / -24.5% | +8.4% |
| owid-co2.csv | 2 | 4 | 4,393,006 / 4,393,006 / 1,927,296 | 626.32 / 620.43 / 499.15 | 1233.03 / 1170.55 / 984.15 | 419.58 / 428.80 / 338.61 | -16.9% / -18.0% | -0.5% |
| prose-64k | 3 | 1 | 19,211 / 19,211 / 19,211 | 8.32 / 8.93 / 8.30 | 11.63 / 11.27 / 11.54 | 4.98 / 5.16 / 4.80 | -12.9% / -4.8% | -0.7% |
| prose-64k | 3 | 4 | 19,211 / 19,211 / 19,211 | 10.17 / 9.51 / 8.70 | 12.61 / 11.81 / 11.72 | 5.56 / 5.15 / 5.10 | +0.3% / +9.2% | -5.2% |
| prose-64k | 9 | 1 | 18,926 / 18,926 / 18,926 | 4.72 / 5.04 / 4.92 | 6.95 / 7.00 / 10.47 | 3.05 / 2.87 / 3.44 | +16.3% / +11.5% | +1.6% |
| prose-64k | 9 | 4 | 18,926 / 18,926 / 18,926 | 4.72 / 5.33 / 5.65 | 6.87 / 8.15 / 7.32 | 2.90 / 3.35 / 3.30 | -2.6% / +0.2% | -3.3% |
| prose-64k | ultra | 1 | 15,176 / 15,176 / 15,176 | 0.69 / 0.80 / 0.85 | 0.81 / 0.78 / 0.83 | 0.37 / 0.40 / 0.42 | +5.2% / +9.1% | -0.3% |
| prose-64k | ultra | 4 | 15,176 / 15,176 / 15,176 | 0.79 / 0.83 / 0.80 | 0.76 / 0.81 / 0.74 | 0.37 / 0.40 / 0.39 | -2.5% / +5.4% | -12.1% |

Original input hashes, executable digests and settings are in manifest.json. raw.csv preserves all phase times, RSS, CPU and order. score.csv reports independent medians per phase. paired.csv reports each repeated within-round comparison, including combined vs the historical baseline, raw byte changes and win counts. The CLI was built on an Apple M5; these warm-cache, shared-host timings cannot establish native x86 throughput or generalize to unrelated producers and sizes. Requested thread count is recorded; actual active worker counts vary.
