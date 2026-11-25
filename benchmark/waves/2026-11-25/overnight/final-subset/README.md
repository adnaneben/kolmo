# Final candidate subset

Fresh comparison of qualified main a1d39559 with the opt-in combined build from 5af8c465. Each of the three requested settings had a warmup and eleven scored main/main-AA/candidate triplets. Every timed arm restores the original bytes. Each warmup additionally checks the frame or archive with the stock main decoder. Absolute sizes, phase throughput, RSS and complete frame hashes are in score.csv; within-pair changes and win counts are in paired.csv. This is a separate run from the four-way 38d512f5 matrix.

| Workload and options | Bytes main → final | Encode MB/s main → final | Decode MB/s main → final | Combined MB/s main → final | Paired Δ vs main / main-AA | Same-binary Δ |
|---|---:|---:|---:|---:|---:|---:|
| enwik8.zip L1 T4 | 36,492,173 → 36,492,173 | 564.34 → 632.66 | 2054.46 → 2031.40 | 442.73 → 453.61 | +2.43% / +11.82% | -4.04% |
| owid-co2.csv L1 T1 | 6,724,159 → 2,210,685 | 362.40 → 291.28 | 980.75 → 614.79 | 262.77 → 198.90 | -25.89% / -27.69% | +0.83% |
| prose-64k L3 T1 | 19,211 → 19,211 | 7.15 → 7.29 | 10.39 → 8.93 | 4.44 → 4.16 | -0.97% / -12.21% | +1.59% |

The complete compressed SHA-256 for the final candidate matched the earlier 38d512f5 candidate at all three settings. The archive and ordinary prose frames also matched qualified main. For CSV the opt-in produces a smaller v12-compatible frame. The program warms shared-host file cache; each operation includes launching the CLI and reading/writing files. Times and ratios are descriptive and localized to the settings and CPU recorded here.
