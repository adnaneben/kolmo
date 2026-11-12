# R3 checkpoint — bounded selection and shared scratch

12 November 2026. **Implementation and local development verification completed. Fresh-source validation is queued, not passed.**

All continuation writes are on `fix/planner-record-ownership-isolated-20261112`; PR #7 remains draft/unmerged. Production source, vendors, formats/defaults, release criteria, other branches and the frozen R1/R2/E02/E03 experiments are unchanged. Concurrent Claude Code E02G work is preserved, not overwritten or counted as this experiment's execution.

## Exact references

- Preregistration, published before new measurement-data download: `c72628a6330e224a59a5a7e1d995a2a4439c7b51` (`R3-guard/PREREGISTRATION.md`).
- Published implementation: `06db7f2f2849fa6ccdc207227cfaf4498d17870e`.
- Frozen R2 implementation: `be41cc58185712ae660dbf3b7e7a93959625b8e9`.
- Repaired production reference: `2f3308a3380a140ad0f6873ae228de7715e5eef5`.
- Fresh validation workflow: **43276455708**, job **131386028141**, exact implementation head above. Last inspected state: **queued, with no steps executed**. No new-source sizes, timings, eligibility result or scientific verdict is established.

The documentation checkpoint deliberately avoids launching additional push/PR CI with a documentation-only `[skip ci]` commit. This does not cancel the already queued implementation validation, change any workflow, or supply a passing check for this documentation revision. The PR remains blocked by any unmet required gate; no merge is authorized.

## What changed algorithmically

One bounded heuristic selects either the original native stored/Huffman/rANS schedule or compatible plane-aligned HUF/rANS with singleton support. Unlike R2's strict safety control, the candidate does not fully compress both schedules.

The no-op path keeps the original schedule for a single residual byte plane, fewer than 1,024 values per plane, or fewer than 16,384 residual bytes. Otherwise it samples at most 1,024 evenly spaced values per plane and compares pooled versus per-plane byte entropy. Alignment requires predicted savings of at least both 1/32 of the residual bytes and `256 + 1024*(planes-1)` bytes. This rule and its thresholds were fixed before the new-source evaluation. The estimate is not a lower bound on the actual encoded size.

When aligned, one rANS temporary buffer is sized for the actual per-plane chunk and reused across planes within the same encode call. Normalization/arithmetic and output bytes match the previous singleton-capable implementation. There is no new entropy-table reuse or arbitrary per-plane codec mixing; one existing leaf still has one codec.

A fixed aligned policy, original native baseline, costed original-or-aligned safety control and duplicate-baseline noise control remain in the matrix. The candidate must emit exactly the original or aligned control's frame according to its recorded gate. Raw stored fallback limits expansion against the raw-residual research frame only: **there is no guarantee of non-expansion against an uncomputed compressed native baseline**.

## Development results — not fresh validation

The exact 58 previously observed E03 inputs were replayed under sanitizers and in two release passes. These are development inputs, including the known household-source overlap with the repository holdout programme.

| Group | Inputs | Original native bytes | Candidate bytes | Aligned inputs |
|---|---:|---:|---:|---:|
| E02D full real-derived streams | 11 | 337,195 | **319,539 (-5.24%)** | 4 |
| E02C small real-derived files | 7 | 51,436 | 51,436 | 0 |
| E02D nested 128-value prefixes | 11 | 3,237 | 3,237 | 0 |
| E02D nested 1,024-value prefixes | 11 | 8,317 | 8,317 | 0 |
| Synthetic controls | 18 | 628,234 | 612,292 (-2.54%) | 6 |

The gate retained the entire observed full-stream alignment saving while avoiding attempts on the seven single-plane streams. It intentionally skipped the small-file gains that always-on alignment could obtain; the synthetic retained saving was also below always-on HRS. These trade-offs remain visible, not tuned away after inspection.

No candidate frame in this development replay grew relative to its original native baseline. This observation does not prove the heuristic's non-expansion on new data.

### Shared-work gain that stacks without changing the encoded bytes

For the aligned bicycle-count stream, peak directly tracked requested heap within the encode operation fell from **4,464,656 to 358,807 bytes** versus fixed R2 HRS, with **five allocations reduced to four**. The traffic-volume case fell from **4,711,256 to 775,367 bytes**. This measures the harness and staged leaf-function heap; it excludes vendor-internal allocations, stack, allocator overhead and other process memory. It is not an RSS reduction or a whole-product memory bound.

This is an implementation gain layered on the retained representation/modeling gain: scratch reuse changes allocation lifetime and size but produces identical payload bytes. The sample histograms are not reused by the entropy encoder; claiming that all redundant scans/model building were removed would be incorrect.

### Timing remains descriptive

On the eleven full streams, candidate residual-to-frame encoding duration was **0.941 / 0.943x** baseline across the two local passes. Warm verified-input decoding was **0.958 / 1.000x**. These do not establish a small speedup: the duplicate original-baseline encode ratios were **0.946 / 0.993x**, and identical-output kernel timings also varied. The useful structural result is avoiding the repeated full schedule search, not a confidence-qualified 6% speed claim.

Measurements start from frozen captured residuals and include gate/model trials, allocation, copying, frame formation/checksum and cleanup; they exclude unchanged prediction/type detection/capture and pre-existing original-input checksum. Decode is separately measured as (1) prevalidated-frame entropy/buffer kernel and (2) complete CRC-verified integer reconstruction, warm and with fixed 32-MiB displacement. Neither kernel nor displaced timings are whole-tool or guaranteed cold-storage performance.

## Executed checks and audit

- Full source distribution's complete `src` and `vendor` Git tree hashes match the validated snapshot. This is a source snapshot, not a full local Git checkout.
- Final GCC release and Clang ASan/UBSan/leak builds passed 351 inherited R1 checks; 144 inherited R2 deterministic round trips, 1,710 truncation checks, 3,456 mutation probes and 12 single-plane identities; seven new gate checks and 18 shared-scratch byte-identity checks.
- Eight synthetic infrastructure unit-test groups passed. They are not corpus measurements.
- The new full-production numeric capture adapter reproduced predictor parameters, header/tail and residual bytes on **40 archived uint32 inputs**. Only final serialization is intercepted in a generated copy; production source stays unchanged.
- **290 configurations per pass** (58 inputs x five policies): sanitizer correctness replay and two release passes all completed. Candidate-to-selected-control identity, original baseline identity, exact C reconstruction and independent Python numeric inverse passed.
- The sanitizer replay exercised **348 directly tracked allocation-failure positions**, permitting only error cleanup or valid decoded fallback and leaving zero tracked outstanding bytes.
- Independent audit verified **870 retained frame hashes** and **5,800 reported wall/CPU medians**. Release passes contained **17,400 timing batches**, not independent datasets.
- Vendor objects are not sanitizer-instrumented; finite tests do not prove general security.

Setup history is retained: two preliminary local build driver invocations were interrupted by the outer command timeout and are not counted as complete builds. Untimed full-core capture compilation was changed to O0 with contraction disabled before the code freeze; its identity checks passed. One synthetic test fixture initially lacked a required field; it was corrected before the freeze, and the final eight groups passed. No new-source outcome was used to change code or policy.

## Fresh validation already registered and launched

The frozen sources are UCI Covertype (first 262,144 rows, five specified integer fields) and SNAP CollegeMsg (all 59,835 SRC/DST/time triples, three exact integer streams), separate from the previously observed families and the parallel E02G sources. These are exact derived uint32 NPY files, not reconstruction of publisher text. The workflow performs a fail-closed repository exposure audit before any data download, freezes all source/member/input hashes before capture, and keeps every specified stream. A failure or overlap means incomplete coverage, not substitution.

Both applicable independent families must meet the preregistered family-level size, per-file regression, encode and two decode limits in both passes; duplicate-baseline timing noise can make the performance conclusion inconclusive. The complete policy is frozen. Previously observed R2/E03 data cannot supply missing new-family coverage.

**Next action:** inspect and audit the existing workflow 43276455708 and its retained artifact. Do not submit duplicate runs or tune the candidate while the registered validation is pending. Record the resulting keep/reject/inconclusive verdict against the exact tested source, without changing historical decisions or release gates.
