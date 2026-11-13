# N2: real matched native execution — 13 November 2026

**Completed locally: pinned full-source build, real-codec sanitizer/cross-decoder gate, frozen 12-arm development matrix and independent audit.** No production integration, new source validation, online selector, RL model, target-machine acceptance, merge or release.

## Result and decision

The fixed guarded type-first per-plane Huffman path (`tp_phuf_pg16`) is strictly smaller and within the recorded zstd-3 round-trip cost on **5/8 files in both passes and every measured pair**. These are all three CollegeMsg streams plus Covertype elevation and hillshade. Keep it as a speed-portfolio candidate, not a universal default.

None of the other three fixed typed candidates clears the zstd-3 screen on a file in BOTH passes. No candidate beats BOTH exact-header and whole-file-numeric OpenZL-9 controls within archive cost on any file in both passes. Two candidates beat the exact-header rival on elevation/hillshade, but not the stronger whole-file rival; do not choose the weaker comparator to manufacture a win.

| Complete pipeline | Total stored bytes | Encode ms, first / confirmation | Decode ms, first / confirmation |
|---|---:|---:|---:|
| zstd-3 | 1,354,554 | 21.509 / 21.094 | 5.247 / 5.572 |
| Unchanged Kolmo fast (diagnostic, not column-first) | 789,043 | 196.318 / 200.981 | 8.346 / 8.780 |
| Typed production zstd-3 leaves | 809,427 | 26.360 / 27.542 | 9.559 / 9.491 |
| Guarded type-first plane Huffman | **943,819** | **14.081 / 14.062** | **9.144 / 9.278** |
| Type-first min-size Huffman/rANS | 815,534 | 26.092 / 26.071 | 14.041 / 14.201 |
| Type-first archive trial portfolio | **732,488** | 798.758 / 813.493 | 10.933 / 10.527 |
| OpenZL exact-header, level 6 | 998,391 | 73.912 / 78.382 | 7.610 / 7.900 |
| OpenZL exact-header, level 9 | 776,404 | 108.669 / 117.123 | 4.405 / 4.591 |
| OpenZL whole-file numeric, level 9 | **740,257** | 119.706 / 124.293 | 7.627 / 7.731 |
| OpenZL serial, level 9 | 1,229,924 | 99.765 / 102.807 | 12.115 / 12.297 |

Times are sums of per-file medians, not a continuous-stream throughput figure. Duplicate controls and every individual sample are retained. Costs are calculated per paired round with the active >3% CPU charging rule, not by adding rounded table medians.

Guarded Huffman is **30.32% smaller in aggregate** than zstd-3 and its summed modeled round-trip cost is **17.61% / 17.14% lower** at the registered 1-Gbit/s link. Its raw decoding is nevertheless slower (about 1.74x / 1.67x); this is not dominance in both encode and decode speeds. The aggregate does not erase three non-winning inputs:

- Cover class expands from 26,332 to 26,411 bytes and is slower.
- Fire distance gets smaller but misses the paired cost gate.
- Road distance gets smaller and has a slightly lower first-pass median modeled cost, but does not fit every measured pair. Retain this as a near-budget diagnostic, not a pass.

The source-ID win is only **315 bytes (0.395%)**, from 79,748 to 79,433, but satisfies the observed cost rule in both passes. No arbitrary minimum percentage was reintroduced.

The archive trial portfolio is **1.05% smaller in aggregate** than whole-file OpenZL-9 but costs **1.53x** as much under the 30-day/3.3-read archive model. The extra trials therefore do not earn their cost here. These observations do not invalidate earlier results measured on different cohorts or CLI boundaries.

## Boundary and identity

The exact eight previously observed R3 NPY files total **5,961,924 bytes**. They represent two correlated source families, not eight independent populations. Typing, predictor/GCD, probes, unsuccessful internal trials, model construction, allocations, frame/checksum formation and cleanup are timed. Independent warm encode/decode rounds use fixed batching and deterministic shuffled order. No file I/O, process startup or Python codec wrapper is timed. Existing zstd checksum-off versus Kolmo/OpenZL checksum-on defaults are retained and declared; this is not an equal-integrity-feature speed claim.

Frozen algorithms: Kolmo/research reference `4c78ea9893a7882bc4d427cca29076623312d66a`, repaired production source `2f3308a3380a140ad0f6873ae228de7715e5eef5`, OpenZL `32246b48faee46807f84183dac4db479089f5445`, and unmodified N2 native driver/run/protocol from `f9e8fcc4aefebf9accc4787d41b02928cf805a1e`. **254 pinned source dependencies** verified byte-for-byte. Active policy SHA-256 remains `1ff10706bbb737c4b949583ecc99746fb28dcb710c24bc51d382c5f614dee657`.

The current Git checkpoint was recovered through workflow **43381556318**, artifact **14016905681**, and verified before restoration at `03e45ed40f3f99986fab796402d702196295db75`. The archived OpenZL source was checked against **3,674 Git blobs**; its tree and exact signed commit objects were reconstructed and hash-checked against the official pin. This is shallow source restoration, not invented upstream history.

## Build correction, not algorithm tuning

The first actual combined build exposed `build.py` rewriting `#include "openzl/..."` to a nonexistent source-relative path. The copy-codec fixture never exercised those headers. The minimal fix leaves public OpenZL headers on the compiler include-search path, while preserving relative-source rewriting and explicit overrides. Two regression tests were added; all **19 infrastructure tests passed**. The failed real build log is retained. No predictor, representation, coder, decoder, setting, candidate, input, protocol threshold or timing loop was changed after observing a performance result.

## Executed verification

- GCC release and Clang ASan/UBSan/leak-detection real matrix: **96 configurations each**, deterministic frames and exact reconstruction; all frames identical across these builds.
- **96 separate stock-reader cross-decodes:** unchanged Kolmo CLI, unchanged public OpenZL decoder API and zstd library reader. All restored exact input bytes.
- Fixed performance matrix: **96 configurations in each pass**, 7 then 5 rounds, **2,304 timed batches**. Every timed encode/decode output checked; all 192 retained performance frames matched source/gold/repeat hashes.
- Independent audit: **236 manifest entries, 2,304 raw timing samples, 1,152 medians**, paired cost arithmetic and duplicate-frame identities verified. Ten new auditor regression tests passed.
- Instrumentation covers the C++ driver/native header adapter and included full Kolmo core/research bridge. Linked OpenZL and vendor libraries are uninstrumented. This is a valid-input real-codec gate, not a comprehensive malformed-input/security certification.

Identical-frame timing controls remain variable: zstd encode 0.949-1.112x, zstd decode 0.858-1.149x, OpenZL encode 0.919-1.072x, OpenZL decode 0.562-1.197x. Therefore do not infer a precise speedup or tight generalized SLA from the shared-host timings. All cost decisions are observed paired checks, not statistical confidence guarantees.

## Preservation and next task

Only research build/test support and new execution evidence are changed on the authorized isolated branch; production files, other branches, historical results, P2-E23 and active cost parameters are preserved. No new benchmark Actions workflow or overnight automation was started. The short source-checkpoint workflow is separate from the local benchmark and not an experiment result.

Keep guarded Huffman as the measured speed candidate and withhold archive promotion. Next, profile the guard/reconstruction/copy cost on the three non-winning inputs and test one output-identical workspace/copy optimization before adding another search trial. Re-evaluate any new policy on genuinely fresh preregistered sources only after the complete deployable choice and competitor configurations are frozen. Do not relabel this observed cohort as fresh or its per-file opportunity as a free online selector.
