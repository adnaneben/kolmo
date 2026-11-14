# E02C results and decision — 12 November 2026

## Decision

**E02C development experiment completed. Reject unconditional packing before the existing entropy coders. Retain global/local bitpacking and patched packing only as workload-specific candidates for independent validation. Do not promote a default, merge or release.**

All writes in this continuation were confined to `fix/planner-record-ownership-isolated-20261112`, draft PR #7. Production `src/`, vendored code, Makefile, normal CLI, wire formats and release acceptance rules remain unchanged from the validated repaired reference. This report is outside the watched experimental source directory so documenting the completed result does not by itself rerun the same research matrix. Ordinary repository CI is unchanged.

## Executed references and evidence

- Validated repaired production reference: `2f3308a3380a140ad0f6873ae228de7715e5eef5`.
- Initial research code: `c880659f4490bfe49831411fb30f91bb8af1e321`; successful Actions run **43232093686**.
- Corrected memory-observation code: `706cdab65caace2ce698e5dfe4c5dd99e8fcbe00`; successful Actions run **43233624953**.
- The correction changes the experimental memory observation, not production code, representation semantics or the corpus. All stored sizes, complete frame hashes, corpus manifests and captured predictor/residual identities matched the earlier run.
- Raw artifacts and archive digests are recorded in `E02C_EVIDENCE.json`. Both archives were downloaded, their SHA-256 digests matched GitHub, and 1,000 exported frame hashes per artifact were checked against the measurement records.
- Earlier baseline evidence was downloaded and its recorded command-log hashes, source identities, 45 frame pairs and 90 decoded outputs verified by `baseline_evidence.py`. This is evidence preservation, not an invented new baseline test count.

The research workflow success does not imply that every unrelated automatically triggered workflow on subsequent documentation commits has completed. Passing evidence is associated with the exact tested source above. PR #7 remains draft/unmerged.

## Scope and protocol

A generated, hash-verified full-source adapter intercepts only the numeric transform's final serialization. It captures residuals and parameters after the unchanged production predictor/GCD. Existing Huffman-only, rANS and zstd implementations are executed; no detector or entropy-cost stubs replace them. Production files are not edited.

This is **forced typed-numeric processing**, not automatic production selection of a new transform. Each result includes the complete experimental E2C1 frame: parameters, seeds/bias/GCD, original header/tail, metadata, exception data, backend payload and checksums. Ordinary uninstrumented Kolmo CLI controls are recorded separately. Research framing is not CZN1/CZNS.

The matrix contains **25 development inputs x 5 representations x 4 backend policies = 500 configurations**. Each workflow executes a first pass with five native C timing pairs per configuration and a confirmation with three: **4,000 timing pairs per workflow**, not 4,000 independent datasets. Both passes reconstruct complete input bytes and are checked by the independent packing/integer reference. The reference does not independently implement entropy decoding.

Seven derived real-data inputs come from four source families: sklearn digits (pixels and labels), Plotly Gapminder (population and years), statsmodels Nile (volume), and statsmodels sunspots (years and activity tenths). There are also eighteen synthetic inputs: six families with three explicit seeds. All data is DEVELOPMENT. Total original bytes are 246,166 real-derived and 1,968,384 synthetic. Related columns are correlated; the digits-pixel fixture dominates the real-derived byte totals. No deployment workload or untouched holdout has been tested.

Local bitpacking, patched packing and local byte planes use the same 128-value sub-blocks. Global/local segmentation is a separate variable. Fixed backend policies permit stored fallback; tables, framing and fallback bytes are counted. Requested and actual codecs are recorded. A result labelled rANS or Huffman policy is not necessarily an entropy-coded payload when storing is smaller.

## Complete sizes: seven real-source-derived inputs

Positive changes below mean larger files, compared with the original residual layout under the SAME backend policy.

| Backend policy | Original layout | Global bitpacking | Local bitpacking | Patched packing | Local byte-plane control |
|---|---:|---:|---:|---:|---:|
| Stored/native | 127,862 | 81,995 (-35.87%) | 82,253 (-35.67%) | 82,455 (-35.51%) | 128,811 (+0.74%) |
| Huffman-only, with stored fallback | 51,728 | 54,775 (+5.89%) | 55,475 (+7.24%) | 56,525 (+9.27%) | 52,356 (+1.21%) |
| rANS, with stored fallback | 52,227 | 54,917 (+5.15%) | 55,646 (+6.55%) | 56,364 (+7.92%) | 52,514 (+0.55%) |
| zstd-1, with stored fallback | 52,588 | 53,825 (+2.35%) | 54,647 (+3.92%) | 55,658 (+5.84%) | 53,386 (+1.52%) |

The native stored/packed reduction is real but does not demonstrate beating entropy-coded Kolmo. The aggregate experiment rejects unconditional packing before all three tested entropy/backend policies on these real-derived fixtures.

## Workload-specific observations

- Gapminder population: patched + Huffman gives **5,414 vs 5,665 bytes** (4.43% smaller), while the rANS policy gives **5,451 vs 6,019** (9.44% smaller). The patched rANS-policy winner actually falls back to STORED packed data; do not attribute that gain to rANS. Direct Kolmo balanced/hc give 5,255/5,175 bytes on this fixture, so this is not a demonstrated complete-product win.
- Nile volume: global packed storage gives **327 bytes** versus the 385-byte original-layout Huffman-policy frame. This is a very small input; it does not establish large-stream performance.
- Sunspot activity: patched packed storage gives **624 bytes** versus the 711-byte original-layout Huffman policy. Again, the stored fallback and small-message overhead matter.
- Digits pixels: global packing increases Huffman-policy size from **43,306 to 46,731 bytes** and patched packing to **48,694**. This dominates the real-derived aggregate regression.
- Synthetic sparse large values, three seeds: patched/Huffman policy gives **21,521 vs 66,251 bytes** compared with original layout, and versus the matched local-byte-plane control's **45,685 bytes** it is **52.89% smaller**. Patched/rANS policy gives **22,170 vs 41,072**, and versus its matched plane control **36,832** it is **39.81% smaller**. These are promising synthetic distribution effects, not independent real-workload confirmation.
- Synthetic changing-width ranges and bounded 9-bit values also favour packing. Counter/quadratic-jitter families often regress, and full-range random data receives no useful gain.

These examples justify retaining candidates for targeted validation, not an unconditional transform or filename-based selector.

## Selector headroom diagnostic

An OFFLINE per-file smallest-frame choice among the five tested representations (selection cost not included) yields:

| Real-derived backend policy | Original layout total | Offline best among tested representations | Maximum saving in this candidate set |
|---|---:|---:|---:|
| Huffman | 51,728 | 51,093 | 1.23% |
| rANS | 52,227 | 50,970 | 2.41% |
| zstd-1 | 52,588 | 52,400 | 0.36% |

This is a bound within this small tested set, not a production selector, a universal compression bound, or a throughput result. The aggregate headroom is too small to justify an expensive exhaustive selector on these inputs. No selector has been promoted.

## Time and memory

Native C full-pipeline timers include the forced numeric predictor/capture, allocations/copies, framing, checksums, coding and inverse, but not automatic planner detection, Python process startup or file I/O. Narrower representation+entropy timers exclude explicitly documented framing/cleanup portions. Product CLI measurements use a separate boundary and must not be compared as kernel speeds.

On the corrected workflow's first/confirmation passes, global packing under the Huffman policy took approximately **1.36x encoding and 1.66-1.73x decoding duration** compared with the original layout, using sums of per-case medians across the seven real-derived inputs. Patched/Huffman took **2.05-2.07x encoding and 1.71-1.74x decoding duration**. rANS-policy packing also slowed the aggregate. These are descriptive measurements on one shared Linux runner, not confidence-qualified production or M5 claims. No fast-profile promotion is justified.

The first workflow's `peak_process_rss_kib` is invalid for post-exec codec ranking: Linux retained the Python launcher's pre-exec high water. It remains preserved, not overwritten. `E02C/MEASUREMENT_NOTES.md` documents the reproducer and correction. The amended probe records both diagnostic pre-exec-inclusive ru_maxrss and `/proc/self/status` VmHWM for its native process image. Corrected VmHWM spans **2,668-5,996 KiB** across the two passes. It includes inputs, capture, retained reference frame, library/runtime and output buffers; it is neither pure coder scratch nor separate encoder/decoder peaks. No memory winner or hard resource bound is claimed.

## Correctness and repeatability

Each successful workflow performed:

- Standalone packing: **448 round trips**, **276,681 truncated parser probes**, **6,784 mutation probes**. Parser probes check bounded behaviour; not every mutated packing payload must be invalid.
- Independent Python packing inverse: **616 cases**.
- Full-source research probe in the sanitized and release builds: **240 round trips**, **4,038 rejected truncations**, **5,760 mutation probes per build**. Half the mutation probes repair frame CRCs to exercise deeper parsing; accepted outputs must reconstruct exactly.
- Full-source Clang ASan/UBSan with leak detection: passed. The vendored archive is built as in the existing sanitizer workflow, not fully instrumented. Existing table-vector ABI warnings are retained in build logs; no sanitizer finding was reported in these finite tests.
- GCC release matrix: **500 configurations**, exact reconstruction and independent numeric inverse for all cases.
- Confirmation: all 500 stored sizes and frame hashes, plus corpus/capture identities, matched.
- After the memory-only correction, the two new passes matched the prior workflow's corresponding 500 frame/size records and all corpus/capture manifests. Downloaded archive/frame hashes were also independently checked.

This is finite developmental testing, not a security proof, exhaustive parser audit, independent entropy decoder or cross-architecture test of the new research code.

## Product controls

The normal uninstrumented Kolmo CLI processed the same seven real-derived files: balanced total **50,675 bytes**, hc **46,242 bytes**. Direct system-zstd single-thread level 1 produced **67,437 bytes**. These are complete-tool size controls with differing framing and planning; no fair speed superiority claim is inferred from their CLI timings. Per-block codec/tflag/plan-prefix records show which production paths were actually used.

No tested unconditional packed configuration beats complete Kolmo hc on this aggregate. Several small forced-numeric cases improve over normal planner output, but that is a separate type-detection/integration question.

## Keep / reject / next action

**Reject:** unconditional global/local/patched packing before the tested entropy coders; any claim that stored-leaf savings prove a superior complete codec; production speed/default promotion; an expensive selector justified only by the small real-derived aggregate oracle.

**Retain provisionally:** bounded-width global packing, local packing for changing-width data, patched packing for sparse large outliers, and low-overhead stored packing for small numeric streams. Retention is conditional on independent, workload-matched evidence and acceptable decode cost.

**Next substantive task:** run a pre-registered, source-disjoint validation of those narrow distribution hypotheses on larger real integer/telemetry datasets while freezing the current representations, 128-value segmentation and backend settings. Include non-target controls and the existing layout/complete Kolmo baselines. Decide whether any specialization earns its complexity before investing in SIMD or a learned/adaptive selector. Do not tune on that validation set; reserve a fresh final holdout for eventual integration.

No new LZ engine, float/JSON codec, neural selector, format identifier, default or release is introduced. M5 acceptance and production integration remain separate unpassed gates.
