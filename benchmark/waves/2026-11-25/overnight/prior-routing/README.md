# Earlier routing results and retained constraints

## Exact main and RC qualification

At L8/T1, both pinned main and the exact release candidate reproduced the earlier corrected frame sizes and SHA-256s. Stacked KLZMA saved 12.07% on Backblaze rows and 7.19% on BGL. Decoding ran faster on both files. The extra encoding work raised round-trip latency to 2.43–2.84× stock across the four comparisons, above the 1.50× archival gate. This evidence supports retaining stock L8 on these workloads. [Pinned main](https://github.com/adnaneben/kolmo/commit/a1d39559686a9ec17c8614b9154a64d7b5b98672) · [Exact release candidate](https://github.com/adnaneben/kolmo/commit/a2e430fed2fbc6dc357a78651cadf0e1fed611f2)

| Revision / complete cut | Stored bytes, stock → stacked | Median encode ms | Median decode ms | Paired combined MB/s |
| --- | ---: | ---: | ---: | ---: |
| Main / Backblaze rows | 385,328 → 338,823 | 155.8 → 563.5 | 50.1 → 20.1 | 20.38 → 7.19 |
| RC / Backblaze rows | 385,328 → 338,823 | 155.3 → 559.7 | 51.1 → 20.2 | 20.37 → 7.24 |
| Main / BGL | 93,438 → 86,720 | 98.2 → 277.7 | 19.0 → 8.5 | 35.82 → 14.65 |
| RC / BGL | 93,438 → 86,720 | 98.6 → 276.9 | 19.0 → 8.6 | 35.67 → 14.70 |

Each revision built its normal CLI and unmodified impact binary with its own Makefile. Both revisions contain the same historical `run.py`. The third arm, impact with no overrides, produced the exact stock frames on every round. The trial set `KOLMO_IMPACT_CODEC=24` and `KOLMO_IMPACT_INPUT=stream`. All decodes used stock executables. All 96 timed cells recovered the source SHA-256, and each revision's stock decoder separately read all 12 warmup outputs, for 24 additional cross-reads. All 96 compressed digests matched the prior 4db0425b receipts for their respective arm. These Apple M5 results used one warmup and seven scored rounds, shuffling all settings within each round, with one-minute host load 6.9–7.4. CPU, RSS, individual phase rates, destinations and hashes are recorded in [scored results](exact-score.csv), [raw rounds](exact-raw.jsonl), [cross-reads](exact-cross-read.csv) and [build verification](exact-verification.json).

The timing adapter uses `wait4` on each child, as in the earlier same-day replay. File-based CLI I/O and process startup are included, and hashing is outside timing. Combined speed is original decimal MB divided by the median of the per-trial sum of encode and decode wall seconds. To replay with the frozen corpus and its manifest, run `./build.sh /path/to/kolmo /path/to/new-build-dir`, then `python3 replay.py /path/to/new-build-dir /path/to/impact/corpus /path/to/output`. [Original experiment definition](https://github.com/adnaneben/kolmo/blob/a1d39559686a9ec17c8614b9154a64d7b5b98672/benchmark/impact/run.py)

## Historical correction

The 25 November one-thread spot check rebuilt the impact harness at `4db0425b361012523d8074ca6f2a760d9466e9f4`. It repeated 12 research cells on five frozen cuts. All 12 restored the input. Nine complete frame hashes matched the older impact receipts. The table shows measured file sizes in bytes and child wall time in seconds. Each timing is one observation on a shared Apple M5. [Original impact matrix](https://github.com/adnaneben/kolmo/blob/a2ac5e0f057a91c721c9f66a21ce90ee5dcb6ecb/benchmark/impact/RESULTS.md) · [Replayed main](https://github.com/adnaneben/kolmo/commit/4db0425b361012523d8074ca6f2a760d9466e9f4)

| L8 input / trial | Earlier frame bytes, stock → trial | Replayed frame bytes, stock → trial | Replayed encode / decode seconds, stock → trial |
| --- | ---: | ---: | --- |
| Backblaze rows TSV / transform followed by KLZMA | 385,328 → 338,823 (−12.07%) | 385,328 → 338,823 (−12.07%) | 0.1771 / 0.0600 → 0.9476 / 0.0338 |
| BGL log / transform followed by KLZMA | 191,050 → 127,916 (−33.05%) | 93,438 → 86,720 (−7.19%) | 0.1107 / 0.0193 → 0.2990 / 0.0090 |
| Wasmtime arm64 / KLZMA | 1,391,398 → 1,122,105 (−19.35%) | 1,153,372 → 1,122,105 (−2.71%) | 0.5203 / 0.0521 → 1.4968 / 0.0448 |

**The rows result reproduced. The BGL saving on the replayed baseline was 7.19%.** On both inputs, the smaller frame decoded faster and took longer end to end. The Backblaze cut was 4,194,123 bytes, SHA-256 `ce1f46c812afed1ec508438e80cd248939065e176251f6a6d672d76a228190b6`. BGL was 4,194,269 bytes, SHA-256 `fba140ef27632a08d8304148c42a7f8ee9b09b724cd62d97e3d2d0cb4dd4eb61`. The same-day audit archive `kolmo-audit-evidence-2026-11-25.zip` has SHA-256 `700085ad5c465bbb97cd0e8c1c124eef6f206ad57c232c17263c8f631053586e`. Its `measurements/impact-spot.jsonl` member has SHA-256 `48e610db0223d8c997911b2794523b5e4cf709a04c3ddf2b6a89fdbb9a9c2bea`. [Earlier raw frame receipts](https://github.com/adnaneben/kolmo/blob/a2ac5e0f057a91c721c9f66a21ce90ee5dcb6ecb/benchmark/impact/raw/rows.jsonl)

The tracker separately corrected the interpretation of **L1 timing**. The broad Wave 2 panel showed a BGL slowdown. In the 15-pair recheck, median combined latency went from 8.514 to 8.441 ms (−0.86%) and both controls were within 0.9%. This paired timing check used the Wave 1 and Wave 2 binaries. [Tracker handoff](https://github.com/adnaneben/kolmo/issues/57#issuecomment-7166388488) · [Recheck results](https://github.com/adnaneben/kolmo/blob/8485feecc5ab0890e8d68fa2e47bb461d6fa93f4/benchmark/waves/2026-11-25/results.md#level-1-recheck)

| Preset | Prior decision and remaining qualification |
| --- | --- |
| L8 | Test KLZMA alongside BWT for eligible binary, table, log or numeric blocks. Preserve the existing fallback. The original matrix reported a 4.5–7× encode-time cost for the affected blocks and up to 1.9× peak encode memory. Establish eligibility and costs on independent producers before changing the default. |
| L9 | Preserve the plain-BWT retry. The earlier BGL-only no-transform result saved 6.7% and called for more log producers before changing trial order. Separately, the BF16 opt-in recovered 1,017,499 bytes but increased combined latency from 20.970 to 38.198 s, above its archival gate. |
| Ultra | Keep native GR6+ L1+L2 gated. Below Ultra the earlier matrix measured 3–11 MB/s decode and 6–25× decode memory on admitted blocks. In the 128 KiB JXL code replay, the lexer, prior, and combination yielded 20,404, 20,463 and 20,053 bytes against 20,830. These used the research decoder. The lexer needs a model-format decision, and the small-stream prior adds tables. The later alternate v12 CM4 mask in #88 saved zero bytes in 12 settings and retained a CPU/wall cost on eligible prose. It was excluded from release. |

The archived general ratio-priority promotion gate for L8/L9/Ultra was ≤0.98× stored bytes, ≥0.67× encode throughput, ≥0.95× decode throughput, ≤1.10× peak RSS in both phases, and ≤1.50× median combined latency, relative to matched pinned Wave 1. The distinct weight-specific L8/L9 gate permitted ≥0.55× encode and ≤1.65× combined latency while requiring ≥0.97× decode. Promotion required a sealed independent source group and matched trials. [Routing constraints](https://github.com/adnaneben/kolmo/blob/a2ac5e0f057a91c721c9f66a21ce90ee5dcb6ecb/benchmark/impact/RESULTS.md) · [Acceptance matrix](https://github.com/adnaneben/kolmo/blob/8485feecc5ab0890e8d68fa2e47bb461d6fa93f4/benchmark/waves/2026-11-25/acceptance.md) · [Ultra disposition](https://github.com/adnaneben/kolmo/issues/57#issuecomment-7167305637)
