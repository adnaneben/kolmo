# M1 — ratio, encode/decode speed and average/peak memory

13 November 2026. New native-API measurements, not a repackaging of earlier timings. Research-only; no production defaults, other branches, merge or release changed.

## Coverage
- 91 registered configurations; 763 successful applicable input/configuration cases; 4578 timed operation batches, each internally repeated to at least 25 ms; 1526 separate memory-operation processes.
- Ten already observed inputs: eight exact R3 numeric NPY files and two deterministic CSV table views. Two original source families, not ten independent datasets.
- Released v1.0.1 is pinned to a1d26f2075120f2a48e72e7026754d1a17bb2e8f. The research branch core and its tuning are distinct from this release. Algorithms are frozen at the N2/P2 reference 4c78ea9893a7882bc4d427cca29076623312d66a; S1/S2 sources are restored from their hashed archives.
- OpenZL v0.3.0 pin 32246b48faee46807f84183dac4db479089f5445; untrained exact-header / whole-numeric / serial / default CSV graphs. Vendored zstd, Brotli, LZ4 and BSC are unchanged; XZ uses liblzma.
- Six release modes; six branch modes; seven named tunings; ten type-first arms; one table-zstd12 arm; 32 nonduplicate S1 sequences; ten S2 policies; eighteen competitor settings; one duplicate control.
- Large-input ultra/ultra-l1 files >=512 KiB were deliberately not run under the 4 GiB runtime: 21 explicit NOT_RUN_LIMIT cases. SmallNumeric3 compares all six modes on exactly three 239,468-byte files.

## Definitions
- Ratio = total original bytes / total stored bytes, higher better. Throughput = original MiB / sum of per-input median operation seconds. Per-input medians use three independent process/batch repeats.
- All typing, prediction, coding, model/context creation, frames, checksums, allocation, cleanup and adapter copies are timed; original input loading and external byte comparison are outside timers. A new process is used for each input, arm, operation and repeat, with one CPU affinity.
- Memory is a separate >=50 ms active operation batch. Average = time-weighted smaps_rollup RSS sampled about every 1 ms; cohort average = mean of per-input averages. Maximum = largest reset VmHWM or sampled RSS across the matched inputs.
- Each memory process loads only that input and its gold frame; include codec workspaces, input/reference/output buffers, touched library pages and allocator retention. Not isolated codec scratch; not worst-case memory for all input sizes.
- An early smoke test exposed pre-exec inherited ru_maxrss (~83 MiB) on otherwise 4–8 MiB processes. It is retained diagnostically but excluded from reported maxima. Corrected memory uses post-exec, phase-reset VmHWM plus sampled RSS.
- Speed model and archive model remain repository defaults (1 Gbit/s; 30 days; 3.3 reads); this report colors individual metrics and cost separately. Green is a measured difference, not statistical significance or deployment acceptance.
- Existing integrity defaults differ: direct zstd default checksum off, Kolmo/OpenZL checks enabled. LZ4 is a raw-block comparator with original length supplied separately; BSC is its vendored internal payload plus integrity prefix, not the official CLI.

## Numeric8: six release modes and retained controls
| Option | Coverage | Ratio x | Encode MiB/s | Decode MiB/s | Enc avg / peak MiB | Dec avg / peak MiB |
|---|---:|---:|---:|---:|---:|---:|
| release_rt | 8/8 | 4.630 | 323.13 | 1714.52 | 5.93 / 7.38 | 5.13 / 6.54 |
| release_fast | 8/8 | 6.848 | 35.42 | 1109.49 | 7.64 / 12.80 | 5.30 / 6.51 |
| release_eco | 8/8 | 7.827 | 18.66 | 395.33 | 14.88 / 28.39 | 6.14 / 8.55 |
| release_hc | 8/8 | 7.950 | 6.23 | 1214.98 | 23.01 / 42.45 | 5.55 / 8.52 |
| release_max | 8/8 | 8.465 | 3.75 | 190.21 | 11.07 / 22.94 | 5.52 / 8.10 |
| release_ultra | 3/8 | Not aggregated | — | — | — | — |
| tp_phuf_pg16 | 8/8 | 6.317 | 552.06 | 716.68 | 5.26 / 6.06 | 5.67 / 7.36 |
| S2_opt_HZ | 8/8 | 7.547 | 329.47 | 367.35 | 5.64 / 7.76 | 5.54 / 7.80 |
| zstd_3 | 8/8 | 4.401 | 277.07 | 1353.22 | 5.43 / 6.25 | 4.46 / 5.26 |
| ozwhole_9 | 8/8 | 8.054 | 48.62 | 793.80 | 8.39 / 18.84 | 5.61 / 8.79 |
| ozexact_9 | 8/8 | 7.679 | 54.65 | 1165.57 | 8.89 / 19.53 | 5.77 / 8.83 |
| lz4_fast | 8/8 | 2.373 | 581.74 | 4097.86 | 4.24 / 5.05 | 4.21 / 5.13 |

## Main observations
- On Numeric8, release rt has 4.63x ratio, 323 MiB/s encoding and 1,715 MiB/s decoding, versus zstd-3 at 4.40x, 277 and 1,353 MiB/s. Both encode and decode RSS are slightly higher. This is a two-source development observation, not a broad release claim.
- The retained tp_phuf_pg16 research path reaches 6.32x, 552 MiB/s encoding and 717 MiB/s decoding. It saves bytes and encoder time but loses decoder throughput versus direct zstd-3.
- S2_opt_HZ remains 41.68% smaller than direct zstd-3, but its new matched speed-model total is 1.112x zstd-3: a cost regression in this run. Do not reuse the older narrow-margin cost win. Different ABI staging and repeated operation boundary are declared; no old samples are pooled.
- On the two derived CSV tables, release eco is 8.96x versus OpenZL CSV-9 at 5.67x. Encode is 31.32 versus 20.09 MiB/s; decode is 136.11 versus 154.77 MiB/s. Encode average/peak RSS: 41.37/73.07 versus 46.67/88.44 MiB. Decode average/peak: 13.73/24.25 versus 22.13/54.28 MiB.
- Ultra on SmallNumeric3 reaches 3.91x but only 0.215 MiB/s encode and 0.228 MiB/s decode. Average RSS is 182.55/187.05 MiB for encode/decode; observed peaks are 259.40/258.44 MiB. These are SMALL-FILE maxima, not its global maximum.

## Evidence and limits
- All 763 generated cases decoded exactly. All 4578 timed outputs matched source/gold. All 1526 memory batches completed. No failed codec case was hidden; 21 safety exclusions remain explicit.
- 42 duplicate and output-identical staging checks passed. Ten infrastructure/provenance tests passed. Numerical summary formulas in the workbook were independently checked against the audit.
- No new sanitizer certification: prior codec studies retain their gates; this new performance driver has valid-input reconstruction and bookkeeping checks, not comprehensive malformed-input testing.
- Three performance repeats and shared hardware are not precise latency guarantees. The workbook colors observed values, without adding an arbitrary acceptance-gain threshold.
- Historical E26/E27 CSV results are separately recomputed from original records and placed on their own worksheet. Their old memory method is excluded, and no average/peak memory is invented for the CSV picker. Its native sampled-memory run remains outstanding.
- Dictionaries, source-trained OpenZL, all block sizes/levels, streaming, multithread scaling and target-M5 validation are outside this finite matrix.
- The native S1/S2 C ABI adapter makes an additional ownership copy in both directions; those costs are included. This is not a claim to reproduce old optimized S2 throughput.
- The release object exports 14 helper symbols also present in the research core. objcopy renamed those RELEASE helper symbols solely to avoid link collisions; algorithms and czn API functions are untouched.

## Files
Workbook: Kolmo_Performance_Ratio_Speed_Memory.xlsx. HTML: Kolmo_Performance_Comparison.html. Evidence: Kolmo_Performance_Memory_Evidence.zip. Source archive: Kolmo_M1_Benchmark_Source.zip.
Source/method notes: https://www.kernel.org/doc/html/latest/filesystems/proc.html

## Delivery receipt
The executed source, build support, pinned identities and tests are in the delivered 24,350-byte Kolmo_M1_Benchmark_Source.zip (SHA-256 17a66a3888a0965e59345f2f7a7306b31fbd7f2af414e837e993efcffdcf943c). The full evidence archive is 135,642,172 bytes with 2,330 hash-verified payload members; SHA-256 3b01ccd7156bd8271d95ede722c68593bc4e31b8e660b055b953d5fd9874a970. Raw events SHA-256 6584b9e1bc52e24b2067aa0133858319a4c5c822df27a9e5fb4ec66d2cf1d142. These archives and the workbook/HTML are conversation attachments, not Git-tracked objects in this commit. This report is Git-tracked; no remote benchmark or CI pass is claimed.