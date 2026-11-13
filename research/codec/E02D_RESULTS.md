# E02D — preregistered independent numeric validation

## Decision

Completed on 12 November 2026. **Packing has real workload-specific value, but no unconditional transform, new default or production integration is justified.** Keep local/exception packing as optional native-leaf research candidates. Do not promote small packed storage merely because it beats rANS on short messages when the existing Huffman policy remains stronger. The next experiment must compare incremental gains against an actual costed native adaptive-leaf baseline, not only a weaker fixed policy.

All continuation writes remain on `fix/planner-record-ownership-isolated-20261112`, draft PR #7. No merge, release, change to other branches, or changes to production source, vendored code, E02C implementations, wire formats, defaults or release thresholds occurred in E02D.

## Registration and executed evidence

- Preregistration commit: `e6bf5644a25cb39be7ccf22f3c899f127628e338`, published before source-data download or compression evaluation. Protocol SHA-256: `63be9a9cad12cab00d86c836ee37e78e031b5dc7153be6cc7de980781a2007bf`.
- Executed orchestration commit: `2ad085f62c795ea84ce7b87d8e5bc8f49e821d87`.
- Frozen E02C codec reference: `706cdab65caace2ce698e5dfe4c5dd99e8fcbe00`; repaired production reference: `2f3308a3380a140ad0f6873ae228de7715e5eef5`.
- GitHub Actions run **43242180594**, artifact **13955991234**: successful full validation workflow. Artifact SHA-256: `5b89de92cfab64d2baed3aa416affd83d329d7475bcb06132bd197f52df2a7a8`.
- Downloaded archive verified locally against the GitHub digest; all **86 evidence-manifest files** matched. Corpus/source-member hashes, 660 configuration identities, median arithmetic, aggregate arithmetic and repeated capture/frame hashes were independently checked. All **1,766,456 derived full-stream values and their row maps** were checked against publisher CSV tokens using exact Decimal comparisons.
- No outcomes were used to tune the codec, representation widths, sub-block size, backend settings, source selection or acceptance conditions. No dataset was replaced. There was no failed real-data run to discard.

The evidence audit checks recorded outputs and provenance; it is not a separate re-execution of C or an independent entropy decoder. Old and new CI evidence applies only to the exact code it tested. Documentation commits and unrelated automatically triggered jobs are not automatically declared passed.

## Corpus and scope

Four UCI source families, none used in E02C: household electricity (235), Metro Interstate traffic (492), bicycle sharing (275), and appliance energy (374). Source metadata, attribution, exact URLs and predetermined fields/windows are in `E02D/PREREGISTRATION.md` and the artifact's `prepared/preparation.json`.

| Family | Source rows used | Full numeric streams | Valid values |
|---|---:|---:|---:|
| Household power | first 524,288 | 3 sub-metering columns | 1,561,062 |
| Metro traffic | 48,204 | traffic_volume and clouds_all | 96,408 |
| Bicycle sharing | 17,379 | casual, registered, cnt and instant | 69,516 |
| Appliances | 19,735 | Appliances and lights | 39,470 |
| Total | not pooled as independent observations | 11 | 1,766,456 |

Full derived NPY input size: **7,067,232 bytes**. Every value is an exactly verified little-endian uint32 conversion, in original row order. Household columns each exclude 3,934 missing tokens within the fixed source-row window; no missing sentinel, rounding, imputation or artificial outlier was introduced. Source-row maps are retained as provenance outside the compression boundary. This compresses and reconstructs exact DERIVED VALID-VALUE NPY files, **not original CSV spelling, all source columns or missing positions**.

Each full stream also supplies its predetermined first 128 and 1024 values: 33 cases total. These nested prefixes are not independent source families and their bytes are not pooled into the full-stream headline. Power supplies most bytes; family-balanced results are reported separately. Metro clouds_all and Bike instant are retained non-target controls. All validation data is now observed; it is not an untouched final holdout for future changes.

## Execution and correctness

The codec and all E02C files were byte-frozen. Local segmentation stays 128 values. All production predictors/GCD and original metadata are unchanged. Five representations x four fixed backend policies x 33 cases yield **660 configurations**. Main/confirmation passes use five/three native C timing repetitions in different predetermined shuffled orders: **5,280 timing pairs**, not independent datasets.

- Twenty infrastructure unit tests passed locally and on CI; their synthetic fixtures are not measured corpus data.
- Frozen full-source Clang ASan/UBSan/leak self-tests passed: 240 round trips, 4,038 rejected truncations, 5,760 mutation probes. Existing vendored instrumentation boundaries and vector-ABI warnings are unchanged from E02C.
- **220 real small-message configurations** also passed under the sanitized probe, with independent packing/integer reconstruction.
- All 660 release configurations passed exact C and independent packing/integer reconstruction in both passes.
- All **660 frame sizes/hashes**, input hashes, residual/parameter/rest hashes and product-control frame records agreed between passes.
- 99 complete-tool size-control round trips per pass (33 cases x balanced/hc/zstd-1) succeeded.

The independent reference does not implement entropy decoding. These finite tests do not establish comprehensive security or cross-architecture/M5 acceptance of research code.

## Full-stream complete experimental-frame sizes

Positive percentages mean larger than original layout under the SAME policy. All research framing, seeds/bias/GCD, metadata, exceptions, header/tail, checksums and backend overhead are counted. Policies permit and explicitly record stored fallback.

| Policy | Original layout | Global bitpack | Local bitpack | Patched | Local byte planes |
|---|---:|---:|---:|---:|---:|
| Stored | 1,869,016 | 1,530,458 (-18.11%) | 662,033 (-64.58%) | 473,119 (-74.69%) | 1,864,864 (-0.22%) |
| Huffman | 426,202 | 435,809 (+2.25%) | **326,388 (-23.42%)** | 337,306 (-20.86%) | 428,982 (+0.65%) |
| rANS | 354,873 | 365,553 (+3.01%) | **321,461 (-9.42%)** | 337,650 (-4.85%) | 341,589 (-3.74%) |
| zstd-1 | **273,571** | 292,939 (+7.08%) | 292,050 (+6.75%) | 309,416 (+13.10%) | 303,138 (+10.81%) |

Unlike E02C's small development aggregate, local/exception packing can improve the native entropy-policy aggregate on these different real workloads. This does not overturn the rejection of unconditional packing: zstd variants still regress, and native gains vary materially by family.

Local packing versus original layout, full streams:

| Family | Huffman size change | rANS size change |
|---|---:|---:|
| Household | -35.58% | -6.98% |
| Bicycle | -10.83% | -29.70% |
| Appliances | +7.82% | +17.35% |
| Metro | -0.01% | +0.35% |

Equal-family mean changes are -9.65% for local/Huffman and -4.74% for local/rANS, rather than the byte-pooled -23.42%/-9.42%. Individual regressions remain visible in DECISION.json. No filename-based selection rule is justified.

## Predeclared hypothesis decisions

**H1 sparse outliers:** only household sub_metering_3 meets the exact preregistered residual phenotype. Patched packing beats the matched local-plane control by 38.99% under Huffman and 18.14% under rANS; it also meets the provisional cost screen for that family. Nevertheless there is only ONE eligible source family: **provisional, not independently confirmed across two families**.

**H2 changing widths:** only the household family meets the exact preregistered phenotype. Local/Huffman beats its matched local-plane control by 36.33% and global-packing control by 38.49%. Local/rANS gains are 10.16% and 20.39% against those controls. These meet the provisional cost screen on that family, but again have only ONE independent family. Bicycle packing gains do not meet H2's phenotype and remain exploratory for H2 rather than being counted as confirmation.

**H4 bounded non-byte-aligned residuals:** the global/Huffman size screen passes on two eligible families (appliances' qualifying light stream: 24.48%; bicycle qualifying counts: 5.30%), but neither supports a fast-path promotion. Other eligible families regress. Stored/global packing passes the size screen on four families versus stored layout, which is a weaker comparator than entropy-coded output.

**H3 small streams:** patched STORAGE beats the original-layout rANS policy by 30.59%, 7.68% and 5.01% on the bicycle, household and traffic families respectively, and meets the provisional timing/memory screen for those comparisons. It does **not** meet the 5%/64-byte size screen against Huffman in ANY family. This is a real small-message rANS-overhead result, not evidence that packed storage should replace the existing stronger native choice.

No hypothesis licenses unconditional packing or release. The two-family size screen, costs and limitations were fixed before data evaluation and were not relaxed.

## Time, memory and complete-product controls

Full-stream sums of per-case median durations, first/confirmation passes:

| Path vs same original-layout policy | Encode ratio | Decode ratio |
|---|---:|---:|
| Local + Huffman | 1.233 / 1.226 | 1.467 / 1.380 |
| Local + rANS | 1.135 / 1.138 | 1.161 / 1.173 |
| Patched + Huffman | 1.700 / 1.729 | 1.324 / 1.336 |
| Patched + rANS | 1.524 / 1.511 | 1.079 / 1.099 |

These are native C forced-numeric pipeline measurements, including capture/allocation/framing/checksums but not normal planner detection, process startup or I/O. They are descriptive on a shared runner, not confidence-qualified M5 performance. Aggregate local decode cost fails the provisional <=1.10 screen. Small-message timings have particularly weak resolution and must not be generalized.

Post-exec native-image VmHWM across release passes: **2,664–18,076 KiB**, including input/capture/reference/output buffers and runtime. It is not separate encode/decode scratch, a hard bound, or a pure-coder memory win.

Complete unchanged Kolmo full-stream size totals: **297,673 bytes balanced**, **236,834 bytes hc**; direct system zstd-1: **438,947 bytes**. Those are different complete-tool framings/planning boundaries, not comparable kernel-speed measurements. Neither unconditional packed native configuration beats complete Kolmo hc on the aggregate.

An explicitly POST-HOC offline diagnostic strengthens the native comparator: choose the smallest original-layout stored/Huffman/rANS frame per file (337,195 bytes), then compare with the smallest of all tested native representations/policies (305,891 bytes). The extra representation headroom is **9.28% within this candidate set**, before selection cost. The latter is still larger than complete Kolmo balanced and hc. This diagnostic is NOT a preregistered hypothesis, implemented selector, universal limit, or free encoding result.

## Next substantive task

**E03: establish a costed native-only adaptive leaf control using the existing stored/Huffman/rANS implementations, then test at most a bounded optional local/exception-packing trial against that stronger control.** Preserve fixed native-pipeline versus complete-product boundaries and charge the trial/selection cost. Do not invent another entropy coder or an expensive selector first. Use E02C/E02D only as observed development evidence for any new decision rule and reserve fresh validation/holdout for its evaluation.

The goal is to find incremental value beyond what the existing native coding choices already achieve. No default, production format, SIMD, float/JSON/LZ/neural extension, merge or release follows from this report. TARGETS.md and eventual M5 acceptance are unchanged.
