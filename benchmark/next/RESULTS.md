# Independent codec and GPU milestone results

7 November 2026. The existing Kolmo improvements are preserved. A standalone independent codec and working Metal/hybrid implementation are now available, but remain opt-in research tools because their compression ratio is not competitive on general files.

## Executive decisions

| Milestone | Result | Decision |
|---|---|---|
| Protect current gains | Checkpoint source integrity, exact default frames and all nine paired performance checks passed | Retain normal Kolmo as the production baseline |
| Independent codec | 240 CPU cases passed normally and under ASan/UBSan; no third-party compression backend | Retain as a separate experimental codec; improve ratio before promotion |
| GPU acceleration | 280 final CPU/GPU/hybrid exact-plan/frame cases passed on Apple M5 | Retain optimized Metal and hybrid execution as opt-in; smaller jobs should use CPU |
| Fair comparison | 32 frozen files × 8 configurations × 5 repeats, all decoded and verified | Publish both wins and losses; no leaderboard or world-best claim |

The [executive plan](PLAN.md) defines the next delivery cycle and the update format. The [codec guide](../../src/experimental/README.md) documents build commands, format and memory requirements.

## What improved, and what did not

These final measurements use the same warm-workspace protocol for every KRC planner. MB/s is decimal input bytes per second. Aggregate ratio is total input / total compressed bytes. Aggregate speed is total input / sum of per-file median times, not an average of MB/s. Every compressed byte, including headers, tile metadata and checksums, is counted.

| Complete Silesia, 12 files | CPU thread budget | Ratio | Encode MB/s | Decode MB/s |
|---|---:|---:|---:|---:|
| Independent KRC, CPU | 1 | 1.107 | 200.9 | 1626.8 |
| Independent KRC, CPU | 10 | 1.107 | 859.3 | 1629.7 |
| Independent KRC, Metal planning | 1 + GPU | 1.107 | 771.9 | 1632.7 |
| Independent KRC, hybrid planning | 10 + GPU | 1.107 | 985.4 | 1633.0 |
| Normal Kolmo fast | 1 | 3.623 | 275.2 | 1859.3 |
| Normal Kolmo fast | 10 | 3.623 | 875.6 | 8858.8 |
| Zstandard 3 | 1 | 3.205 | 570.1 | 2284.6 |
| LZ4 | 1 | 2.101 | 1071.5 | 6799.0 |

Hybrid KRC improved encoding throughput by 14.7% over ten-thread CPU KRC with byte-identical output. On Mozilla, KRC CPU / Metal / hybrid measured 969 / 1154 / 1390 MB/s. This establishes a useful local acceleration result for the same codec, not superiority over normal Kolmo or other codecs. KRC decoding remains serial; GPU acceleration currently covers planning only.

The general-purpose wkneseas is large: Silesia KRC output is about 3.27 times the size of normal Kolmo fast output. On complete Canterbury (11 files), KRC ratio is 1.175 versus Kolmo fast 4.919, Zstandard 3 at 4.430 and LZ4 at 2.287. CPU KRC encodes at 729.5 MB/s there; hybrid falls to 414.9 MB/s and Metal alone to 368.6 MB/s. The GPU is not an automatic improvement.

The three additional files frozen after execution-layout tuning confirm the limitations:

| Final validation file | KRC ratio | Kolmo fast ratio | KRC CPU10 MB/s | KRC hybrid MB/s |
|---|---:|---:|---:|---:|
| E.coli | 1.091 | 3.310 | 768 | 656 |
| bible.txt | 1.026 | 3.411 | 791 | 660 |
| world192.txt | 1.026 | 3.650 | 769 | 642 |

Synthetic integer tests are diagnostics, not evidence of general leadership. For example, KRC compresses the random-walk fixture 9.57× at CPU/hybrid 892/961 MB/s, versus Kolmo fast 10.23× at 698 MB/s. Conversely Kolmo fast compresses the interleaved and quadratic fixtures 5243× and 870×, versus KRC 4.30× and 9.14×. Even the apparently suitable numeric fixtures do not justify replacing Kolmo's existing structure discovery.

## Preserving the earlier gains

`benchmark/checkpoints/cpu-planning-v1` retains the earlier source overlay and native reference library with SHA-256 hashes. The current source gate verifies those protected sources exactly; additions to the Makefile, root README and CI must preserve their saved prefix. Experimental files and build targets are separate. No normal compression algorithm or format was changed in this delivery cycle.

The final isolated regression run compared the saved reference against current Kolmo on Mozilla, NCI and OWID, at 1, 4 and 10 threads, using five randomized repeats per pair. All default frames and restored contents agreed. All median slowdowns were below 5%; the largest was 0.74%. Mozilla ten-thread current throughput was 995 MB/s versus checkpoint 989 MB/s. This confirms retention of the earlier parallel-planning improvement; variation within this run is not a new optimization claim. See `protected-performance.csv`, `protected-gate.json` and `protected-tests.txt`.

A second source snapshot under `benchmark/checkpoints/independent-gpu-v1` preserves this experimental milestone, including the working optimized GPU layout. Earlier measurements and rejected kernel variants remain on disk. Neither checkpoint is overwritten when testing a new idea.

## Fairness and practical limits

Hardware is the previously inventoried Apple M5: 10 physical CPU cores (4 performance + 6 efficiency), 10 logical CPU threads, 10 GPU cores, 24 GiB unified memory. There are 10 CPU hardware threads available, not 20. Work is dispatched to Metal threadgroups; this does not measure occupancy or establish that all GPU cores were continuously busy. Encoding and decoding still have serial phases. CPU auto-detection and fallback exist, but universal all-core utilization on arbitrary machines is not implemented.

The final harness uses five randomized interleaved repetitions for each file/configuration and reports medians. KRC CPU and GPU paths use preallocated caller-owned plans and output; the Metal context is reused. Warm GPU timing includes input copies, submission, synchronization, result copies and CPU encoding/checksums. GPU buffer growth is included when it occurs during a timed call. Device-only timing is diagnostic. Setup is reported separately: this run took 55.0 ms. Each KRC candidate is compared against a CPU reference plan and complete frame before decoding is verified.

A separate process using the standalone hybrid CLI on the 8 MiB random-walk fixture took 67.1 ms for the codec operation including 54.6 ms setup, plan allocation and context destruction. File I/O, output allocation, round-trip verification and writing are outside that timer. Driver shader caches were not controlled, so this is process startup evidence, not a guaranteed uncached compile time. CLI process peak RSS was 31.0 MB, with 5.57 MB of requested Metal buffers. Those memory figures overlap; do not add them. The comparable CPU CLI observation was 19.6 ms and 19.6 MB peak RSS. Neither is a full repeated memory/latency frontier study.

Thread counts and block layouts differ across codecs. Zstandard and LZ4 rows here are single-thread reference points; they are not equal-resource opponents for ten-thread or GPU KRC. The strongest acceleration comparison is KRC against its own CPU path with identical frames. KRC uses independent 1 KiB integer tiles, while normal Kolmo and the baselines use different layouts. Earlier pinned lzbench measurements remain in `benchmark/audit`; the new KRC results use a separate verified harness and have not yet been integrated into lzbench. CPU and GPU share the Mac's memory bandwidth. Desktop load, clock frequency, thermals and energy were not controlled, and no confidence intervals are claimed.

No remote CI run or second machine was available during this milestone. The added CI job covers CPU correctness, sanitizers and source preservation on Linux/macOS; GPU tests ran locally on the M5.

## Reproduction and provenance

From the repository root, with no other benchmark/test jobs running:

```sh
make -j10 build/krc build/krc-metal build/krcbench-gpu build/krcgputest
make experimental-check
./build/krcgputest src/experimental/krc_simd.metal
./build/krcbench-gpu -G -r 5 -T 10 -s src/experimental/krc_simd.metal \
  benchmark/audit/data/silesia/* benchmark/audit/data/owid-co2-data.csv \
  benchmark/next/data/canterbury/* benchmark/next/data/mitdb-100.dat \
  benchmark/next/data/synthetic/* benchmark/next/data/validation-large/* \
  > /tmp/kolmo-krc-repeat.csv
python3 benchmark/next/protect_gains.py
python3 benchmark/next/summarize.py
```

GPU execution needs normal access to a Metal device; a restrictive process sandbox may hide it. For CPU-only platforms use `build/krcbench-cpu`; GPU targets require macOS frameworks. The protected speed gate currently requires the local macOS checkpoint library and corpus; the portable source-only gate runs in CI. Do not overwrite the recorded final CSV with a new run.

`inputs.json` freezes 29 file hashes before initial evaluation; `validation-inputs.json` freezes the three additional validation files before the final run. All 32 hashes were rechecked. Canterbury and the signal file were new to the earlier Kolmo audit, but are now development evidence. The synthetic generator seed is 20261107. New future tuning requires another frozen family for final validation.

Public corpus sources are [Silesia](https://sun.aei.polsl.pl/~sdeor/index.php?page=silesia), [Canterbury and Large corpus descriptions](https://corpus.canterbury.ac.nz/descriptions/) and [MIT-BIH Arrhythmia Database 1.0.0](https://physionet.org/content/mitdb/1.0.0/). The signal fixture is raw `100.dat`; attribution: Moody GB, Mark RG, MIT-BIH Arrhythmia Database, PhysioNet, [doi:10.13026/C2F305](https://doi.org/10.13026/C2F305), Open Data Commons Attribution License 1.0. The fixture is used as bytes without medical interpretation. Exact download URLs and hashes are in the manifests.

Builds use Apple clang 21, `-O3 -mcpu=native`, and the existing vendored comparator versions (Zstandard 1.5.7 and LZ4 1.10.0). `results.csv` retains the first full run with the original slower GPU kernel; subsequent smoke CSVs retain tuning observations. The final protocol standardized caller-owned KRC plan workspaces, so those earlier speeds should not be treated as a perfectly controlled kernel-only before/after comparison. `rejected/krc_simd_allwidths.metal` retains the rejected intermediate layout. Final files and binary/source hashes are recorded in `final-manifest.json`.
