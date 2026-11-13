# S2: selective plane layers and copy reduction — executed results

13 November 2026. **16 fixed policies × all 14 S1 inputs, real-codec release/sanitizer checks, two timed passes and independent reconstruction completed.** Research only on the isolated branch. No production default, decoder, format ID, release, new holdout or RL change.

## Decision

Retain the output-identical staging-copy reduction and keep fixed upper-plane-only policies as size/cost alternatives. **Do not promote the first sample-based selector:** it produces larger outputs than applying H->Z to every plane, without a consistent aggregate cost improvement. Its extra H/Z/L menu selects exactly the same frames and does not earn its additional probes. Keep both favorable and negative cases.

H = frozen Huffman; Z = zstd-3; L = LZ4. A->B compresses A's full framed output with B; it does not replace an internal entropy stage. Every complete KST1 research frame contains its parameters, original header/tail, stage IDs/lengths and CRC. Decode remains the unchanged S1 decoder. All timings include typing/forward, selection, probes, failed trials, allocation, copies, framing, integrity and cleanup.

## 1. Output-identical copy optimization

The optimized implementation passes borrowed plane spans to the existing leaf encoder, avoids making another vector copy of each codec-owned output, borrows the predictor's output when already in the needed layout, and reserves the final frame. It does not change prediction, GCD, tables, codec levels, bytes or the inverse.

| Fixed per-plane path | Bytes, both implementations | Encode candidate/original, pass 1 / 2 | Modeled cost ratio, pass 1 / 2 |
|---|---:|---:|---:|
| H | 956,284 | 0.967 / 0.992 | 0.978 / 0.990 |
| Z | 809,134 | 0.985 / 0.977 | 0.987 / 0.982 |
| H->Z | 789,949 | **0.959 / 0.958** | **0.973 / 0.978** |
| H->L | 819,778 | 0.958 / 0.981 | 0.973 / 1.004 |

Totals cover all eight real-derived files, not the synthetic controls. The HZ encoding decrease is approximately 4.1–4.2% in the two passes. Other small deltas, especially the inconsistent HL modeled cost, must not be advertised as universal wins. All 140 S1 control-frame comparisons match the archived bytes; the new and old implementation controls agree on all inputs.

Separate diagnostic profiling places much of encoding time in prediction/GCD and CRC, with additional framing/copy/allocation work. For HZ, summed diagnostic profile medians attribute about 6.78 ms to forward processing, 4.79 ms to codec calls, 3.26 ms to CRC and 1.49 ms to remaining work in the original path; the optimized remainder is about 0.39 ms. These diagnostic runs are not comparison timing. The remainder mixes framing, copies, allocations and timer overhead; it is NOT a pure allocator counter, process-memory bound or causal attribution for every nanosecond. No decoding optimization is implemented: differences between byte-identical decode controls are measurement variation.

## 2. Placement and conditional decisions

All following policies use the copy-reduced implementation. Low/high policies apply the second layer only to the lower/upper half of planes; the rest stay H-only. Least-significant planes come first. A one-plane stream belongs to both halves, so those cases are not independent evidence for a location preference.

| Policy | Real bytes | Size versus all-plane HZ | Encode ratio vs all-plane HZ, passes 1 / 2 | Modeled cost ratio vs all-plane HZ, passes 1 / 2 |
|---|---:|---:|---:|---:|
| H only | 956,284 | +21.06% | 0.875 / 0.876 | 0.972 / 0.971 |
| Z only | 809,134 | +2.43% | 1.254 / 1.276 | 1.115 / 1.125 |
| **H->Z on all planes** | **789,949** | reference | 1.000 / 1.000 | 1.000 / 1.000 |
| H->L on all planes | 819,778 | +3.78% | 0.904 / 0.912 | 0.955 / 0.971 |
| H->Z lower planes only | 913,047 | **+15.58%** | 0.955 / 0.969 | **1.012 / 1.020** |
| H->Z upper planes only | 811,601 | +2.74% | **0.930 / 0.911** | **0.964 / 0.958** |
| H->L lower planes only | 934,497 | +18.30% | 0.890 / 0.916 | 0.978 / 0.995 |
| H->L upper planes only | 825,898 | +4.55% | **0.875 / 0.892** | **0.942 / 0.947** |
| Sample-gated H->Z | 797,694 | +0.98% | 0.999 / 0.993 | 1.001 / 0.991 |
| Sample-gated H/Z/HZ/HL menu | 797,694 | +0.98% | 0.994 / 0.993 | 1.005 / 0.995 |

Upper-plane policies expose a useful trade-off; the tested lower-plane HZ policy is dominated in aggregate by all-plane HZ on both size and modeled cost. A cheaper upper-only policy is not automatically preferable under the user's objective: when all-plane HZ also fits the competitor budget, its smaller output remains preferable. No online per-file budget selector is provided by these fixed comparisons.

## 3. Why the sample gate does not yet earn promotion

Across the real files there are 17 planes. Both gates apply a full second layer to eight and skip nine. Gate-HZ performs 15 bounded sample compressions over 61,440 sampled bytes; the menu performs 30 sample compressions on the same sample bytes. No admitted full trial was rejected on those real files. Both select H for nine planes and HZ for eight: **neither Z-only nor HL is selected**, so the larger menu provides no output improvement.

The two gates produce identical bytes on every one of the 14 real/synthetic inputs. On dense/full-width synthetic controls, the menu additionally pays for five full Z trials that are discarded. Therefore the extra menu is not justified by this experiment even if small timing differences appear favorable in one noisy run.

The safety floor misses a clear opportunity: CollegeMsg timestamps have a 7,519-byte Huffman-framed plane, below the frozen 8,192-byte sampling threshold. The gate leaves it H-only. Complete timestamp size is **76,877 bytes**, versus **69,530** with all-plane HZ: **7,347 bytes lost**. No threshold was retuned after seeing this result. The gain of checking small but repetitive coded planes is a NEXT-experiment hypothesis, not a validated patched policy.

Local nonexpansion is guaranteed only against this run's H-only group result, because an attempted full alternative is retained only when its complete group is smaller. That guarantee does not extend to an uncomputed Z-only compressed baseline or competitor cost.

## 4. Individual real-input effects remain visible

| Input | All-plane HZ | Upper-only HZ | Sample gate |
|---|---:|---:|---:|
| Source IDs | 78,941 | 78,914 | 79,457 |
| Destination IDs | 82,853 | 82,826 | 82,799 |
| Timestamps | 69,530 | 72,586 | 76,877 |
| Cover class | 17,071 | 17,071 | 17,071 |
| Elevation | 157,101 | 157,069 | 157,069 |
| Fire distance | 103,640 | 113,816 | 103,640 |
| Hillshade | 175,017 | 174,985 | 174,985 |
| Road distance | 105,796 | 114,334 | 105,796 |

Skipping an unhelpful low-plane second stage recovers small header savings on IDs/elevation/hillshade, but skipping a useful stage loses far more on road/fire/timestamps. These are conditional effects, not additive compression percentages. The sample gate's 8/17 admitted layers do not by themselves show that it chose the right planes.

## 5. Designated-competitor boundary

Direct zstd-3 stores 1,354,554 bytes. Optimized all-plane HZ remains **41.68% smaller in aggregate**, with observed summed modeled cost ratios **0.984 / 0.990** in these two passes. Upper-only HZ is 811,601 bytes at **0.949 / 0.949** cost; upper-only HL is 825,898 bytes at **0.928 / 0.937** cost. These are descriptive aggregates, not proof every file fits.

The optimized all-plane HZ and both sample gates have five of eight strict smaller-and-every-pair-cost opportunities in both passes: both IDs, timestamps, elevation and hillshade. Upper-only HZ has four, upper-only HL three. No arbitrary minimum saving threshold is applied.

Identical zstd controls show per-file encode ratios 0.954–1.076, decode 0.938–1.041 and cost 0.976–1.055. Thus the optimized HZ aggregate's roughly 1–1.6% cost margin is **not a robust generalized speed guarantee**. Its bytes are exact and stable; its cost margin needs controlled-machine confirmation. Do not pool this run with S1 or N2 timings or retroactively alter their verdicts. Direct zstd retains checksum-off defaults versus KST1's CRC; this is not equal-integrity-feature dominance.

No matched OpenZL arm was run: **archive acceptance NOT_EVALUATED**. No M5 run, source-disjoint validation or deployable competitor-budget selector was implemented.

## Verification, provenance and limitations

- 16 fixed arms × 14 inputs = **224 configurations** in each release/sanitizer/timing pass.
- GCC and Clang ASan/UBSan/LSan selftests each passed **192 round trips, 882 truncation rejections and 1,008 mutation probes**.
- All 224 sanitizer frames equal the release gold frames. All 224 first/confirmation frames reproduce them; all sampled outputs are verified.
- **5,376 timed batches** retained; **1,344 medians** and per-round cost decisions independently recomputed; **672 measured/sanitized frame instances** checked.
- Independent S1 Python KST1 parser and integer inverse reconstructed **224 frames** (134 distinct pinned leaf-reader calls after caching). Entropy decoding is reused, not independently implemented.
- **20 Python infrastructure/cost tests** passed. All **1,619 original S1 manifest entries** were verified before restoration; the pinned full-source dependency checks passed.
- Instrumented driver/bridge/core, uninstrumented vendor objects. Finite tests are not exhaustive allocation-failure or security assurance.
- The original combined vendor/source build exceeded the command timeout; its incremental rerun completed. No data case or policy was removed. No performance retuning followed observations.

All fourteen inputs were already exposed development data: eight real-derived files from two source families plus six artificial mechanism controls. No aggregate generalization or production promotion follows. No workflow or paid resource was launched. Source, compact receipts and reports are committed; large raw frames/measurements are delivered separately with a manifest and archive identity.

## Next substantive task

Keep the fixed, output-identical optimization and upper-only controls. Develop ONE charged microprobe for the small coded-plane blind spot, comparing with the simpler fixed all-plane HZ rather than another broad codec menu. Use the current observed data only for development; freeze the complete policy and matched rivals before source-disjoint validation. Production integration, new format semantics, release and merges remain separately gated.
