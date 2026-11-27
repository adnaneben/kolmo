# E03 — actual costed native selection, 12 November 2026

## Decision

**The costed native baseline is implemented and measured. The first bounded optional-packing selector does not pass its predeclared development cost screen and must not be promoted.** It saves 5.14% over that stronger baseline on the E02D full streams, but encoding takes 1.716x as long in both passes. Retain the baseline and first selector as reproducible experimental controls; do not tune the same rule after seeing this result and relabel it as validation.

Writes are confined to `fix/planner-record-ownership-isolated-20261112`, draft PR #7. No production source, vendors, formats/defaults, Makefile, existing E02C/E02D implementations or release acceptance rules were changed. Parallel Claude Code additions under `E02C-harness/` and `E02E/`, including its later workflow commit, were preserved. Their outcomes are not claimed as verified by this report.

## Executed identity and evidence

- Frozen E03 protocol: `b7de3f5bfc36bc5b5884b55fa29f55ca8a684be9`.
- Executed E03 implementation: `3d596d16c2fa367e44aabc81c5cba80949468403`.
- Successful GitHub Actions run: **43250751948**, job **131295561593**.
- Artifact: **13961345681**, `e03-selection-43250751948-1`.
- ZIP SHA-256: `5d02cee72dbbbc8a396e85e29c2cb3f5bf91f26abd2e808995b9a54205660e82`.
- Downloaded archive audit: all **611 manifest files**, **576 retained frame hashes**, and **4,032 reported timing medians** verified. First/confirmation choices, captures, sizes and frame hashes match. All chosen sizes equal the corresponding archived fixed-coder minimum over the actually attempted candidates; no expansion relative to the native baseline or trial-budget violation occurred.
- Source/reference: repaired production `2f3308a3380a140ad0f6873ae228de7715e5eef5`, original core blob `e06467c8872d494b97270811bedf6fd0bac2aa31`, graph blob `91ad067dc583b9d9e6f060f214a0f7c402bb4a31`; E02C/E02D frozen against `91c4cdaaca57d0bb667a059341dbf25fd5a8df71`.

See `E03_EVIDENCE.json` and `E03/PROTOCOL.md`. Passing evidence applies to that exact research source, not every later documentation commit or automatically triggered workflow. No merge or release follows.

## What was measured

The isolated wrapper reuses the actual full-source numeric capture, predictor/GCD, packing, decoder and existing entropy coders. Capture happens once per encode. Four policies share the same experimental E2C1 envelope:

1. **native_base**: run original-layout stored/Huffman/rANS and keep the smallest. Both entropy trials, allocation, selection and cleanup are actually executed and timed; this is no longer an uncosted offline oracle.
2. **sampled_optional**: do native_base, then sample at most eight 128-value blocks (1,024 residuals). Skip below 1,024 residual values. Prefer local packing unless patched sample storage is at least 10% smaller. A full trial requires estimated represented-size reduction of at least 12.5% and 64 bytes. At most one full alternative representation is evaluated with stored/Huffman/rANS. Reject it on a tie or larger final frame. At most four entropy-coder calls; all rejected work is charged.
3. **huffman_control**: original layout with Huffman/stored fallback.
4. **rans_control**: original layout with rANS/stored fallback.

All reconstruction metadata, seeds/bias/GCD, header/tail bytes, framing, exceptions, model tables and checksums count toward stored size. The emitted chosen representation/coder is enough to decode; the selector is encoder-only. No LZ payload is selected, but the HUF implementation is vendored zstd code and the executable links the existing archive: this is not a dependency-free claim.

The data are the exact previously archived 25 E02C and 33 E02D inputs, now **observed DEVELOPMENT**. 58 inputs x four policies = **232 configurations** in each release pass, seven plus five native timing repetitions = **2,784 encode/decode pairs**. The 22 nested E02D prefixes are reported separately, not pooled into the eleven-full-stream headline. No fresh validation/holdout was run.

## Main result: E02D full streams

| Same-frame policy | Complete bytes | Encode duration, first / confirmation | Immediate post-encode decode duration, first / confirmation |
|---|---:|---:|---:|
| Original layout, Huffman control | 426,202 | 21.942 / 21.968 ms | 10.895 / 10.926 ms |
| Original layout, rANS control | 354,873 | 24.288 / 24.540 ms | 11.108 / 11.279 ms |
| **Costed native_base** | **337,195** | **27.720 / 27.740 ms** | **13.176 / 13.309 ms** |
| **Sampled optional packing** | **319,859** | **47.562 / 47.594 ms** | **12.319 / 12.465 ms** |

Durations are sums of per-input medians, not a single continuous stream. The adaptive original-layout baseline saves 20.88% versus fixed Huffman and 4.98% versus fixed rANS, but pays 1.263x/1.130–1.141x encoding duration respectively. Choosing smallest output is not automatically the fastest decoder.

The optional selector saves **17,336 bytes (5.14%)** beyond native_base, at **1.716x encode duration**. It captures **55.38%** of the archived 31,304-byte representation-oracle headroom (337,195 to 305,891) within the earlier candidate set. This oracle is a diagnostic, not deployed selection or a universal limit.

All eleven full inputs trigger an optional trial; only six retain it. Five full trials are paid for and discarded. Sample processing accounts for about **0.4%** of optional-selector encoding time; the full alternative trial accounts for **43.4%**. Reducing sample overhead alone will not fix the main cost. A raw represented-width gate is insufficiently selective about entropy-coded benefit.

## Per-family outcome

| E02D family, full streams | Native baseline bytes | Optional bytes | Saving | Encode ratio, first / confirmation | Immediate decode ratio, first / confirmation |
|---|---:|---:|---:|---:|---:|
| Household power | 177,334 | 167,344 | 5.63% | 1.800 / 1.797 | 0.909 / 0.913 |
| Bicycle demand | 61,040 | 54,427 | 10.83% | 1.344 / 1.346 | 1.331 / 1.334 |
| Appliance energy | 11,713 | 11,713 | 0.00% | 1.481 / 1.472 | 0.999 / 0.961 |
| Metro traffic | 87,108 | 86,375 | 0.84% | 1.708 / 1.724 | 1.042 / 1.033 |

The predeclared screen required at least 3% incremental savings, encode <=1.25x and decode <=1.10x on two real families in BOTH passes. **Zero families pass all dimensions**, including the smaller E02C families. The result is therefore `do_not_promote_this_selector`, without changing thresholds.

E02C's seven small real-derived inputs save only **0.815%** (51,436 to 51,017 bytes) at **1.595–1.599x encoding duration**. Synthetic inputs save 7.415%, but synthetic success is not real-workload validation. Neither E02D prefix aggregate saves any bytes (3,237 at 128 values; 8,317 at 1024 values); predictor seeds can leave fewer than 1024 residuals and correctly trigger the frozen skip rule.

Across all 58 cases: 31 full optional trials, 18 accepted (13 local, five patched); 40 resulting frames remain byte-identical to native_base. No compared frame expands relative to that baseline. This is not a guarantee that every encoded file is smaller than its raw input.

## Timing caveat discovered during audit

Decode is timed immediately after each encode in the same process. It includes inverse, allocations/copies and verification but excludes I/O/startup. The reported aggregate 0.935–0.937x decode ratio **must not be promoted as an intrinsic decoder speedup**. Forty skipped/rejected cases emit byte-identical frames; their first-pass decode ratios still range from 0.794 to 1.463, revealing noise/cache/allocation-history sensitivity, especially on small messages. Some larger unchanged household frames also decode faster after the more expensive optional encode. Separate decode-only, order-balanced measurements with controlled warm/cold conditions are needed before a decoding-performance decision. The encoding-cost rejection remains supported on both full passes.

All timings are descriptive on one shared Linux runner, not confidence-qualified M5 results. Post-exec image VmHWM spans **2,672–18,308 KiB** across release passes and includes runtime/input/capture/reference/output buffers; it is not isolated coder scratch or a hard resource bound.

Archived complete-tool size context (unchanged bytes): Kolmo balanced **297,673**, hc **236,834**. The optional native experimental output remains 7.45%/35.06% larger respectively. Different framing/planning boundaries prevent transferring kernel-speed claims to these controls. No complete-product superiority is established.

## Tests and a second execution environment

GitHub ran and passed:
- 160 sample-selector cases plus three invalid-argument checks under sanitizers.
- Six synthetic evidence-tool unit tests, not compression measurements.
- Full-source release AND Clang ASan/UBSan/leak selftests: **576 round trips/deterministic re-encodes, 1,554 fixed-control minimum comparisons, 7,337 truncations and 6,912 mutations per build**.
- **112 small-input configurations** under the sanitized probe, with exact complete reconstruction and independent packing/integer reference.
- All 232 configurations in each release pass; fixed candidate-budget/size/capture assertions and repeat confirmation passed.

Independent reference does not reimplement the entropy decoders. Vendor libraries use the existing archive instrumentation boundary; retained compiler vector-ABI warnings are not suppressed. Finite checks are not a security proof.

The local shell still cannot clone GitHub, but the validated wheels run supplied a usable source-distribution artifact (13929641752, run43187874238). Its ZIP digest and **complete Git tree hashes of src and vendor** matched the pinned source. This enabled an additional genuine local full-source build, not an excerpt simulation. GCC release and Clang sanitizer selftests passed with the same counts; 116 native-selector configurations across all58 inputs reconstructed exactly and matched GitHub's frame hashes, sizes and decisions. Local compiler commands/source provenance are retained separately. One first local sanitizer compilation was interrupted by the outer command timeout; it produced no verdict and was rerun to completion, with that interruption preserved. This local source snapshot lacks Git history and is not misrepresented as a full upstream checkout, a new CI pass, M5 test, or separate fresh-data validation.

## Dataset-scope correction and parallel work

The parallel Claude work correctly flagged that UCI235 household power also appears in `benchmark/holdout/inputs.json`. E02D was source-disjoint from OUR E02C fixtures, **not from the repository's entire release/holdout programme**. Its source-family results remain measured evidence, but must not be presented as pristine release validation. E03 already labels all C/D inputs observed development. Any future release decision needs a newly audited source-disjoint holdout. The exact excluded-missing-value/derived-NPY boundary remains unchanged.

Parallel `E02C-harness/` per-plane development and registered `E02E/` work were preserved without mixing their outcomes into this selector's predeclared decision. Do not overwrite or retune their frozen protocol based on E03.

## Next substantive task

**Review the registered per-plane-leaf E02E evidence against this costed native baseline before investing in another packing selector.** Reconcile candidate/production framing, entropy-policy and timing boundaries; do not pool incompatible tables. If packing is subsequently pursued, the demonstrated bottleneck is the unproductive full trial, so an entropy-aware rejection rule and independent decode-only timing are prerequisites—not more sample tuning or immediate SIMD. New rules require fresh validation; no code/default/format integration, merge, release or M5 acceptance is authorized by E03.
