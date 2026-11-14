# Native exact-header OpenZL comparator: executed development result

13 November 2026. **Implemented, locally built, tested and measured; no P2 acceptance or fresh validation.** The next prerequisite is now executable: fair native whole-input comparator measurement without a Python codec wrapper. All work is on the isolated research branch; production sources and other branches are unchanged.

## What changed

New native parser identifies the exact integer payload after an NPY or PCM16 WAV header, then composes standard OpenZL SplitN, endian interpretation, ACE-default numeric and ACE-default LZ graphs. Original prefixes and tails are compressed as binary streams and reconstructed in their original order. Frames are ordinary single-output OpenZL frames; the independent reader calls the unmodified `ZL_decompress` API and knows nothing about NPY/WAV. The parser and graph construction execute within each encoding call.

The recovered official source archive verifies against **3,674 original Git blobs** and all **1,229 N1 artifact manifest entries**. Both its release pin and standard numeric/serial profiles are retained. This adapter is a custom front end, not the official `zli` CLI or a globally optimal OpenZL configuration. All arms use the same library and compiler. No source training took place.

## Observed complete-frame sizes

All eight original prepared R3 NPY files, exact byte reconstruction; all framing/checksums included:

| OpenZL control | Level 6 bytes | Level 9 bytes |
|---|---:|---:|
| Exact header + typed payload + tail | 998,391 | **776,404** |
| Entire file interpreted as fixed-width numbers | 998,685 | **740,257** |
| Entire file treated as serial bytes | 1,246,968 | 1,229,924 |

Exact typing is **4.88% larger in aggregate than the older whole-file-numeric level-9 control**, not automatically better. It is 36.87% smaller than the serial-9 control. The actual change is context-dependent:

| Input | Exact typed-9 | Whole-file numeric-9 | Exact change |
|---|---:|---:|---:|
| CollegeMsg source IDs | 67,917 | 68,064 | −0.22% |
| CollegeMsg destination IDs | 71,394 | 71,536 | −0.20% |
| CollegeMsg timestamps | 63,177 | 63,244 | −0.11% |
| Covertype cover class | 16,223 | 16,204 | +0.12% |
| Covertype elevation | 169,072 | 158,268 | +6.83% |
| Covertype fire distance | 87,431 | 96,950 | **−9.82%** |
| Covertype hillshade | 207,006 | 171,784 | **+20.50%** |
| Covertype road distance | 94,184 | 94,207 | −0.02% |

These differences show that removing a header changes graph selection/context as well as overhead. Do not select the weaker rival arm to manufacture a Kolmo win. Whole-file numeric coding is byte-lossless and remains a valid diagnostic competitor; its limitation was the claim of payload-specific typing, not incorrect reconstruction. Retain both exact and whole-file controls, and freeze rival/profile selection rules before a new candidate comparison. A per-file hindsight minimum is an offline frontier, not a free selector.

## Cost boundary and uncertainty

Both passes retain 56 configurations (six profile/level controls plus identical exact-9 duplicate), with **672 native-process operation samples**. No Python codec startup occurs inside these commands. Full exact-9 encoding summed medians: 125.51 / 146.83 ms; independent stock-reader decoding: 21.36 / 16.77 ms. Whole-file numeric-9 encoding: 133.93 / 154.97 ms; decoding: 22.53 / 18.87 ms. These absolute totals describe this shared host only; they are not pooled with earlier cloud results.

Identical-frame duplicate controls show per-file timing ratios approximately 0.61–1.42 across encode/decode and both passes. Consequently, **no precise speedup, tight cost-budget pass or target-machine performance claim is made**. CPU samples are retained for later charged-cost calculations. This experiment measured no new Kolmo candidate or zstd baseline and therefore cannot grant competitor-relative P2 acceptance. It also does not validate the earlier 47/61 hindsight archive opportunity.

## Correctness and provenance

- 14 parser test groups passed in release and Clang ASan/UBSan/LSan runs, including all integer widths/endianness/NPY versions, 128 random valid layouts and 128 arbitrary-byte rejection cases.
- 5 linked test groups passed in release and adapter ASan/UBSan/LSan builds: 35 exact input fixtures at two levels (70 configurations), deterministic re-encodes, both wrapper and separate stock-API decoding, truncation/mutation checks and overwrite rejection.
- All 56 timed release frames reproduced byte-for-byte in a separate sanitized-adapter replay. Every measured original input was restored exactly.
- The audit verifies all 282 R3 artifact entries, the exact eight input hashes, 672 raw samples and **448 medians**. `audit.py` independently recomputes the committed compact ledger.
- OpenZL/vendor libraries were uninstrumented; the newly written parser and adapter were instrumented. Finite tests are not security certification.
- The original cloud `zli` binary requires GLIBC_2.43, unavailable on this host. It was not timed or claimed to pass here. Instead the exact archived OpenZL source was rebuilt, and the unmodified standard public decoder compiled separately. Official CLI cross-decoding on its compatible runner remains an additional check.
- First smoke construction attempted to nest full-input segmenters beneath a split. OpenZL correctly rejected it. Successors were changed to the official internal ACE graphs, with a common <=16 MiB measured-input cap. The failure was before any benchmark; no input or result was discarded.
- Initial filesystem symlink hashing normalized a trailing slash; checking the tar's exact symlink target reproduced the original Git blob. One target build before CMake regeneration failed to find the new target; the regenerated build and all suites succeeded.

## Decision and next task

**Keep this as the native header-exact comparator alongside the whole-file numeric control.** Do not call it a new compression algorithm or choose only its weaker outputs. It eliminates a concrete measurement-confounding source and permits a matched candidate/rival benchmark with typing and graph construction charged.

Next: run the retained type-first Kolmo pipelines and these pinned OpenZL alternatives in the same native, batched harness on already observed inputs, measuring actual deterministic plan-selection cost and a properly specified rival frontier. Then freeze that complete policy before any source-disjoint validation. No RL implementation is justified merely by this measurement correction.

No new Actions workflow was launched: **zero new runner-minutes** for this continuation. No overnight schedule was extended, and no merge, release, default or production format change occurred.
