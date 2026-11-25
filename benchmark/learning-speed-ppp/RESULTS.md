# Speed-priority learning pilot

The direct option disabled optional analysis at level 1 / one thread. Complete file bytes were unchanged on both development producers. HDF5 cleared all gates with 9.09% less median paired round-trip time. ZPAQ improved by 1.61%, below the preregistered 3% floor. The learned table correctly contained zero routes. Holdout validation verified actual fallback dispatch, unchanged frame hashes and exact stock v0.0.2 reconstruction on both independent producers. Keep the native stock CLI for this scope. The unchanged-size HDF5 timing is an exploratory development observation.

| Project | Path | Stored B | Stock encode/decode ms | Tested encode/decode ms | Stock/tested combined MB/s | Median paired total-time ratio | Stock/tested encode MiB |
|---|---|---:|---:|---:|---:|---:|---:|
| zpaq | direct | 167,951 | 2.633 / 1.963 | 2.325 / 2.004 | 64.29 / 66.87 | 0.984 | 3.31 / 3.28 |
| hdf-group-hdf5 | direct | 56,210 | 2.513 / 1.925 | 2.029 / 1.887 | 33.20 / 38.34 | 0.909 | 2.95 / 2.92 |
| libtiff | fallback | 21,872 | 2.470 / 2.081 | 35.145 / 2.091 | 15.96 / 1.95 | 8.148 | 2.86 / 17.75 |
| pcre2 | fallback | 29,668 | 2.494 / 1.946 | 35.333 / 2.124 | 21.23 / 2.48 | 8.428 | 2.88 / 17.73 |

The stock/tested combined columns show unpaired marginal median throughput for each arm. The admission decision uses the within-round paired ratios. These statistics may differ when process start latency or host load varies. Eleven randomized pairs per file, 88 verified round trips across the four files. This warm local Mac pilot is unsuitable for cross-machine performance claims. Decode RSS, frame digests, both phase timings and all rounds are in the JSON receipts.

Source versions (Homebrew Apple arm64) were ZPAQ 7.15_1 and HDF5 h5dump 2.2.0 (development), libtiff tiffcp 4.7.2 and PCRE2 pcre2grep 10.49 (holdout). Recreate using the pinned source, split-manifest filenames and hashes, unmodified executables, the config and the seal/learn/validate commands in docs/ENCODER-POLICY.md. The prior size-priority pilot was frozen at commit f57ee085. These pilots do not ship learned routes.
