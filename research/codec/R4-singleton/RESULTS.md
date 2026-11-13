# R4 result: keep a targeted singleton decoder optimization; no general promotion

13 November 2026. Local development execution and cross-checks completed. **No production integration, new encoder format, fresh holdout, M5 pass or competitor-cost acceptance is claimed.** The work stayed on the authorized isolated branch and used zero new Actions runner-minutes.

## Main result

The unchanged R3 CollegeMsg timestamp payload contains four rANS chunks, one representing **59,833 repetitions of a single byte**. R4 replaces that chunk's symbol loop with validated repeated-byte output. Its full CZN1 frame remains exactly **70,850 bytes**, reconstructing the original **239,468-byte** derived NPY file. No compressed payload, KG_INT parameter, prediction, GCD, source value, header or tail changed.

| Timestamp measurement | First pass | Confirmation |
|---|---:|---:|
| Warm entropy-kernel duration, candidate/original | 0.798x | 0.777x |
| Warm full verified CZN1 decode duration | 0.901x | 0.914x |
| Displaced-before-batch entropy kernel | 0.810x | 0.855x |
| Displaced-before-batch full verified decode | 0.894x | 0.880x |
| Identical-baseline warm kernel control | 1.027x | 0.963x |
| Identical-baseline warm full-decode control | 0.975x | 1.016x |

Warm kernel medians were 375,994 -> 300,101 ns and 411,955 -> 320,163 ns. Warm complete decode medians were 809,551 -> 729,124 ns and 807,644 -> 737,817 ns. These are per-file medians on a shared x86-64 host, not fixed throughput guarantees.

**Interpretation:** singleton support, previously an encoding enabler, also presents an implementation-level decode opportunity. The observed target improvement is about 20-22% in its rANS kernel and 9-10% in full warm decoding. It is not an additional compression-ratio percentage, and must not be added to R3's 14.70% size result.

## Negative and no-op results remain visible

The six other RANS inputs contain no singleton chunks. Their measurements vary around the original, with outliers. The byte-identical baseline controls also vary: across inputs and both passes, warm full-decode control ratios range from 0.851x to 1.060x; displaced full controls from 0.918x to 1.081x. Cache displacement does not turn this shared host into a controlled performance lab.

Pooled over the **seven timed RANS inputs**, sums of median full warm durations are **1.015x / 1.006x**, slightly worse, not better. Displaced full durations are **0.995x / 1.013x**. There is **no demonstrated aggregate full-decoder speedup**. Do not remove the nonsingleton inputs or present the timestamp result as a universal improvement.

In particular, the two previously slow Covertype streams have high-plane tables with **18 and 2 symbols**. This shortcut does not act on them and does not resolve R3's size-only HUF-to-rANS selection problem. Keep the decode-cost-aware choice and stronger model work as separate experiments.

## Exactness, wire compatibility and malformed inputs

The proof obligation is narrower than adding a new coding scheme. For one full-scale symbol, the existing inverse state update is the identity. Its original terminal conditions imply exactly 32 coded bytes and all four states equal R64_L. R4 checks those conditions directly. The existing normalized-frequency sum, length, state lower-bound and output-capacity checks remain.

Executed in **each** GCC original, GCC candidate and Clang candidate ASan/UBSan build:

| Check | Cases per build |
|---|---:|
| Valid leaf cases (all 256 symbols, short lengths, boundary and mixed chunks) | 2,663 |
| Truncation differential checks | 10,748 |
| Leaf mutation checks | 12,805 |
| Complete KG_INT/CZN1 round trips, read ranges and output limits | 107 |
| Full-frame mutation probes | 1,536 |
| Direct rANS allocation-failure position | 1 |

All three builds produce the same decision digest `4f59cfd2bd87566c`. Eight generated golden frames are byte-identical across builds and decode through the separately built unchanged production CLI. All **eight archived R3 candidates** were rewrapped as ordinary CZN1/KG_INT frames, matched across all three builds, and reconstructed through that unchanged CLI. The original pre-repair core's rANS reader is byte-identical to the repaired reader; its extracted SHA-256 is `66650ea9a4b8b71148f6825ba760a346f998f26bcd9d5467b6acde3bf272050d`.

Eleven Python tests independently check singleton table/state parsing, all byte symbols, boundaries, malformed tables, truncation, extra words and the state-identity algebra. This is not an independently implemented general entropy decoder.

A deliberately induced allocation failure initially did not trigger because upstream macros undefine a whole-file `malloc` override. The failing assertion is retained. The harness was corrected to inject the same explicit allocation hook at the actual rANS allocation in both staged variants, then all tests were rerun. No input, acceptance criterion or decoder rule was removed to obtain a pass. The initial unsupported interactive build invocation also ran no command; the subsequent bounded noninteractive build succeeded.

Finite tests and a local compatibility argument are not comprehensive security assurance or certification of every historical binary. The full core/harness were instrumented; vendor objects were not. Failed decodes may leave different partial destination contents, which callers must discard; successful output and observed error classifications agree. Only one direct rANS allocation point was fault-injected, not every allocation in the full codec.

## Measurement and provenance

- Exact pinned production core: `e06467c8872d494b97270811bedf6fd0bac2aa31`.
- Exact graph: `91ad067dc583b9d9e6f060f214a0f7c402bb4a31`.
- Detached local history checkpoint: `85c4a28`; production source equals the newer branch's source. No claim of a complete local checkout of all newer research.
- Archived R3 input evidence SHA-256: `00ae71bdc5e4f3612d017dfeabb0f5154742feda67bde2be92f1bfeefb50ebe1`; all **282 entries** verified.
- Both encoders are frozen: R4 times decoding of prebuilt identical frames only.
- **504 timed batches**: 7 RANS inputs x 2 regimes x 3 arms x (7 + 5) rounds. Policy order is randomized with fixed seeds; no measured encoder immediately precedes decoding.
- Batch work is fixed by input size (approximately 4 MiB of reconstructed bytes, capped at 256 decodes). Timing excludes subprocess startup/input loading; full decode includes core inverse/checksums/allocations. Reusable caller output allocation lies outside both timed regimes.
- GCC `-O2 -fno-plt`, common vendor objects, `-ffp-contract=off`; candidate Clang sanitizer build is correctness-only. No Apple M5 or matched zstd/OpenZL evaluation.
- Memory allocation remains unchanged; no memory reduction is credited to this shortcut.

## Decision and next task

**Keep R4 as a small, conditional implementation layer and compatibility regression. Do not promote a production default or claim a mode win.** Preserve the raw timing controls, including the absence of aggregate benefit.

Next substantive task: replay the strongest current production-frame pipeline against the **header-exact OpenZL NPY comparator** on a common complete-file/cost boundary, carrying R4 only as a separately measured decoder ablation. The current broader archive reports using `le-*` over entire NPY files are not interchangeable with the exact NPY adapter's comparison. Do not infer that their per-file win counts transfer. Any new combined policy needs frozen fresh-source validation before generalization.
