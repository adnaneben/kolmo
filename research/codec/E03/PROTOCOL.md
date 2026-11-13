# E03: costed native leaf selection — development protocol

## Boundary and reference
Only `fix/planner-record-ownership-isolated-20261112` may receive writes. PR #7 stays draft/unmerged; other branches and PR #6 stay untouched. No production integration, defaults, format semantics or release criteria may change.

Starting point: `91c4cdaaca57d0bb667a059341dbf25fd5a8df71`. Repaired production reference: `2f3308a3380a140ad0f6873ae228de7715e5eef5`; E02C implementation reference: `706cdab65caace2ce698e5dfe4c5dd99e8fcbe00`. All production src/vendor/Makefile/TARGETS.md and E02C/E02D code remain byte-frozen. A separate research wrapper reuses the full-source adapter, integer prediction/GCD, packing and inverse routines. The emitted envelope remains experimental E2C1, never a production CZN1/CZNS extension.

This protocol fixes the first selector before its performance execution. E02C/E02D outcomes informed it, so their data are OBSERVED DEVELOPMENT data, not fresh validation. No thresholds will be changed after this run to manufacture a success. New selector rules require subsequent independent validation.

## Four controls
0. `native_base`: capture unchanged numeric residuals once; encode the original layout with existing Huffman and rANS, also considering stored bytes. Keep the smallest complete frame. All trials are actually executed and timed. Ties prefer stored, then Huffman, then rANS.
1. `sampled_optional`: execute exactly native_base, then the bounded sample rule below. At most ONE additional full representation (local bitpacking OR patched packing) is encoded, with stored/Huffman/rANS choices. Retain original-layout baseline on ties or regressions. No output may exceed native_base size. Charge sampling, both sample packing estimates, conversion, rejected full trials and cleanup to encoding time.
2. `huffman_control`: original layout, existing Huffman plus stored fallback, measured in the same wrapper.
3. `rans_control`: original layout, existing rANS plus stored fallback, measured in the same wrapper.

Native here means no LZ backend in the selected payload. Huffman reuses vendored zstd HUF code, and the research executable links the existing vendor archive; this is NOT a dependency-free codec claim.

## Frozen optional-sample rule
- Fewer than 1,024 residual values: skip the optional trial; no sample is built.
- Otherwise take at most eight distinct 128-value residual blocks, evenly spaced including first and last. Preserve block boundaries; the last block may be short. At most 1,024 values are sampled. Do not consult filenames, dataset family, future outcomes or training labels.
- Use unchanged local/patched packing on the sample to estimate their represented lengths. Prefer local packing unless patched is at least 10% smaller than local.
- Try the chosen full representation only if its sample length is at least 12.5% below the sampled original-layout length and the proportional represented-byte saving is at least 64 bytes.
- This is deliberately a cheap representation heuristic, NOT an entropy prediction. The actual full encoded trial determines whether to retain it. There are at most four entropy-coder calls per input (two baseline and two optional), plus two bounded sample packing operations. No full exhaustive representation search is deployed.

## Corpus, comparisons and timing
Reuse exact archived inputs from E02C artifact 13951053398 (SHA256 83eee1ef7ba8dfd84bab172100dd538adcbdd095c75001705bd146854984d33b) and E02D artifact 13955991234 (SHA256 5b89de92cfab64d2baed3aa416affd83d329d7475bcb06132bd197f52df2a7a8). Verify ZIP digests, input hashes and counts before running. 25 E02C + 33 E02D inputs, four policies: 232 configurations. E02D nested prefixes must not be pooled into the full-stream headline. Preserve missing-value/derived-file and correlated-family caveats. Retain archived product controls as SIZE context only, not current same-host speed evidence.

Measure seven native C encode/decode pairs per configuration, then five in a separately shuffled confirmation pass. Preserve all timings; report medians, per-family sums and run-to-run ratios. Native timers encompass capture, selection, allocation/copy, framing/checksum, trials and cleanup, but exclude process startup and file I/O. Record process wall time separately. Record post-exec Linux VmHWM with its full-process boundary. Do not compare old-run timing ratios as same-machine current speed results.

Require exact source/capture identity across policies and passes; complete reconstruction through C and independent packing/integer inverse. Independent entropy decoding is not claimed. Require native_base bytes equal the archived per-input minimum original-layout stored/Huffman/rANS frame size, and optional bytes equal the minimum of baseline and its ONE attempted candidate's native policies. Audit full candidate choice against archived fixed matrix; optional size must never regress. Verify frame hashes and sizes on confirmation. Record selected coder/representation, sample size/cost, attempted full candidate, accepted/rejected trials, all coder-call counts and full cost.

## Tests and decision
New selector unit tests cover sample boundaries, layout widths, local/patched selection, small-input skips and random-data rejection. Full-source tests cover deterministic output, scalar extremes/tails, exact adaptive-minimum agreement with fixed controls, truncation and checksum-repaired mutation probes under ASan/UBSan/leak detection. Vendored instrumentation boundaries remain explicit.

First decision screen (research only, NOT a replacement for release TARGETS.md): at least 3% incremental complete-frame savings over costed native_base on two independent real source families, with per-family summed-median encode ratio <=1.25 and decode ratio <=1.10 on both passes; otherwise do not promote this selector. Report each dimension separately, all regressions and non-target cases. No fast/default promotion from passing development screens; fresh validation/holdout is still required.

Also report how much of archived offline native representation headroom is captured, explicitly separating bound from deployed selection. Whether positive or negative, preserve this first design and decide one next experiment from evidence. No new entropy coder, LZ, float, JSON, neural model, SIMD, merge or release in E03.
