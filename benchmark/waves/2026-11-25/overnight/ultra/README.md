# Repair and transfer test for explicit Ultra table limits

The Model 4 encoder on main accepted `--x-tables=16..22` through the public API and then ignored the numeric preference. The historical review A11 had called out this gap. Commit `f5bd6620ef61505e87a2943114046e27dee28a6e` starts explicit settings from the existing full-table policy, then raises the header’s size shift until the largest context fits the requested maximum. There are four 16-byte buckets per 64-byte line. The existing explicit working-memory allowance still takes precedence when it calls for smaller tables. All changes affect only encoding with an explicit numeric setting. Production defaults are unchanged. Source, tests, native binding mirrors and docs are included in `model4-cap.patch`.

## Matched measurements

The baseline is exact main `a1d39559686a9ec17c8614b9154a64d7b5b98672` and all Kolmo cases use the regular Ultra file CLI, T1. Brotli 1.2.1 level 11 is a same-host competitor. Each cell has seven shuffled repetitions. MB/s means decimal raw bytes / elapsed seconds. Combined throughput uses the median of paired encode + decode seconds. Memory is the median peak RSS of the separate encoder/decoder processes (MiB). Shared-host elapsed timings are directional.

| Input | Arm | Stored B | Enc MB/s | Dec MB/s | Combined MB/s | Enc / dec MiB | Model 4 shift |
|---|---|---:|---:|---:|---:|---:|---:|
| GMP Info | Brotli 11 | 70,050 | 0.888 | 71.863 | 0.877 | 17.8 / 2.7 | — |
| GMP Info | main full | 53,347 | 0.280 | 0.653 | 0.196 | 120.3 / 45.2 | 3 |
| GMP Info | fixed numeric17 | 53,535 | 0.281 | 0.603 | 0.185 | 92.9 / 30.5 | 4 |
| GMP Info | main numeric17 | 53,828 | 0.282 | 0.712 | 0.204 | 65.1 / 23.7 | 5 |
| Libgcrypt Info | Brotli 11 | 66,847 | 0.894 | 74.664 | 0.886 | 18.1 / 2.8 | — |
| Libgcrypt Info | main full | 50,488 | 0.298 | 0.714 | 0.210 | 109.7 / 46.6 | 3 |
| Libgcrypt Info | main numeric17 | 50,951 | 0.331 | 0.756 | 0.237 | 65.6 / 24.1 | 5 |
| Libgcrypt Info | fixed numeric17 | 50,666 | 0.316 | 0.793 | 0.227 | 93.1 / 31.2 | 4 |
| Webster 512KiB | main full | 75,921 | 0.776 | 0.693 | 0.367 | 97.8 / 64.5 | 3 |
| Webster 512KiB | fixed numeric17 | 76,157 | 0.686 | 0.663 | 0.323 | 69.8 / 39.1 | 4 |
| Webster 512KiB | Brotli 11 | 140,913 | 0.848 | 90.409 | 0.839 | 20.5 / 3.2 | — |
| Webster 512KiB | main numeric17 | 76,588 | 0.772 | 0.604 | 0.349 | 56.9 / 26.3 | 5 |

All 96 runs restored their source SHA-256. All 9 distinct native frame hashes were further decoded and SHA-verified with the independent v0.0.2 binary. Nine whole-frame digest equalities verified that old numeric17 matched old scaled, and fixed default/scaled and full each matched their original counterparts on all three workloads. Separate fixed-numeric16 controls, with headers, are retained. The stock decoder digest is recorded in `manifest.json`.

## Paired comparisons and decisions

- GMP Info: fixed numeric17 changed bytes by -0.544% against the prior numeric setting, +0.352% against full, and -23.58% against Brotli 11. Median paired combined speed change versus the previous numeric setting was +1.65% (4/7 faster pairs).
- Libgcrypt Info: fixed numeric17 changed bytes by -0.559% against the prior numeric setting, +0.353% against full, and -24.21% against Brotli 11. Median paired combined speed change versus the previous numeric setting was -2.39% (2/7 faster pairs).
- Webster 512KiB: fixed numeric17 changed bytes by -0.563% against the prior numeric setting, +0.311% against full, and -45.95% against Brotli 11. Median paired combined speed change versus the previous numeric setting was -7.54% (2/7 faster pairs).

Treat this as an explicit API contract repair and a controllable size/memory tradeoff. Small byte changes alone do not justify modifying the default. The two whole manuals share a format despite independent authorship. The Webster prefix tests a different text distribution. No cross-ISA or all-text claim follows, and CPU contention can affect the speed ordering. The reader has to allocate according to the shift encoded in each accepted frame; this API is an encoder policy, not a process-wide memory cap.

## Related ownership and open research

Claude R16 explicitly recorded historical claim #68 for an LSTM mixer path and #66 for a new neural codec. The proposal uses a new model nibble or codec; this repair does not implement either, and archived estimates are not Kolmo benchmark results. R11’s #56 prior-pack research, shipping-reader rejection of Lexi/prior extensions, and the GR6 gated existing-column route remain separately recorded. Historic authorship establishes who proposed them, not whether a fresh worker is active. Existing GR6/L3, alternate mask, confidence expert, reset and Lexi LUT counterexamples remain preserved in PRs #48/#91. A future neural experiment needs a distinct opt-in wire contract, deterministic reference decoder, count of model side data and initialization cost, and held-out end-to-end evidence.

## Reproduction

Build commit a1d3955 and apply the patch in a separate checkout. Run `make -j2 build/kolmo check-cm4` for the candidate, then the regular frozen v12 tests. Run `python3 compare.py BASE_MAIN_CLI PATCHED_CLI RELEASED_V002_CLI BROTLI EVIDENCE_DIR` and `python3 report.py EVIDENCE_DIR`. Input paths and slicing are in the runner, source file and slice SHA-256 in the manifest. Inputs are installed public files and are omitted from the evidence. Build/test logs, raw phase/CPU/RSS, method, compatibility receipts and exact source patch accompany this report.
