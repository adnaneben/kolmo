# Kolmo executive delivery plan

Protect the working compressor while testing an independent codec and GPU acceleration. Existing modes stay the production baseline until an experiment demonstrates a useful advantage under declared constraints.

| Milestone | Business outcome | Acceptance rule | Status |
|---|---|---|---|
| Protect current gains | A recoverable source checkpoint and automated comparisons | Existing modes keep their bytes, correctness and measured CPU scaling | Passed: exact frames; all nine paired speed gates passed |
| Independent codec | A small lossless codec with no third-party compression backend | Exact round trips; include every metadata byte; compare ratio, encoding, decoding and memory | Functional prototype passed; ratio not competitive; experimental |
| GPU acceleration | A Metal experiment with a CPU fallback | CPU/GPU decisions agree; measure cold start and complete warm execution | Passed on M5; hybrid wins on larger inputs and loses on smaller inputs; opt-in |
| Promote evidence | Keep improvements that help a defined workload | Pass correctness and regression gates; record failures as well as wins | Retain protected normal Kolmo; retain prototype separately; no default promotion |

## Next delivery cycle

Current cycle (user priority: lzbench first): the independent codecs now have real callbacks in the pinned local lzbench build. The initial 108-row Silesia baseline, 250-row randomized development matrix, 126-row frozen Calgary14 matrix, nine-setting board-tar run and matched-block scaling checks passed. A separate byte-context candidate, KBC1, passed 97 correctness cases normally and under ASan/UBSan. Silesia ratio improved from 1.107× to 2.733× and Calgary14 from 1.172× to 2.784×, but established codecs beat the new model on ratio and both speeds. The local ZPAQ5 reference matched the published board's exact compressed size. Preserve the independent ratio gain as a separate reference; do not promote this model into normal Kolmo. [This cycle's results](../ratio/RESULTS.md) record the measured gap and promotion decision.

| Current checkpoint | Status | Next evidence |
|---|---|---|
| Preserve earlier CPU/GPU work | Passed: source/raw-evidence hashes, identical default frames and all nine paired speed gates; third checkpoint saves KBC1 | Keep future outputs separate and every checkpoint immutable |
| Establish lzbench position | Passed: 503 local configurations including initial baseline; existing ultra stored 6.21% fewer bytes than ZPAQ5 and passed fresh-process frame verification | Reproduce on the target platform with isolated codec symbols before a public-board claim |
| Improve independent ratio | Passed: 59.49% fewer bytes on Silesia and 57.90% on frozen Calgary14 versus KRC1 | Retain separately; decoding and encoding costs rule out replacing the fast path |
| Decide against leading codecs | KBC1 failed competitive gate on complete development/validation corpora and board tar | Improve prediction quality and coding cost before any default promotion |

| Order | Work | Decision gate |
|---|---|---|
| 1. Improve independent compression | Measure where raw tiles and exception/metadata costs dominate; compare independently implemented residual probability and byte-oriented models | Keep only a useful ratio/speed/memory tradeoff with all model bytes counted; version new formats separately |
| 2. Make acceleration selective | Estimate CPU/GPU completion time by input size and model cost, amortize setup in reusable contexts, then test bounded splits | Beat CPU-only complete execution on a newly frozen validation family; choose CPU where GPU loses |
| 3. Challenge the leaders fairly | Add the independent codec to the pinned local lzbench adapter; expand competitor settings and equal-thread/block comparisons | Report the full ratio/encode/decode/memory frontier, with no universal-best claim from selected wins |
| 4. Promote and broaden | Run protected gates, then test additional CPU/GPU hardware before integrating any winning path | No correctness or protected speed regression; documented fallback and resource budget; retain prior checkpoints |

This cycle starts with lzbench, then measures the ratio candidate. Do not change normal Kolmo's default policy while the competitive gate fails. [Previous milestone results](RESULTS.md) explain the preserved CPU/GPU decision. New evidence is recorded separately under `benchmark/ratio`.

Next model priority: profile prediction loss, then compare longer/adaptively weighted histories and explicit run/record relationships under bounded memory. Retain KBC1 as the independent ratio reference and KRC1 as the fast/GPU reference. A candidate must offer a useful measured tradeoff against leading codecs as well as our own references, with a newly frozen input family before final evaluation. Reduce the serial probability/coding cost once prediction quality justifies it. Public-board qualification then needs the target machine/compiler/input protocol and an independently reviewable adapter; local Mac speed results do not establish that rank.

The strongest measured existing-mode result is ultra at 37,956,550 bytes (5.584×), versus ZPAQ5 at 40,470,202 bytes (5.237×), on the same tar. Its process peak was 5922.3 MiB versus 979.7 MiB for ZPAQ; timings are single observations. This hybrid-mode result is separate from the independent model's 2.732× board-tar ratio and does not establish a win under a common memory budget or an official public-board placement.

## Rules that protect progress

The CPU planning checkpoint is stored under `benchmark/checkpoints/cpu-planning-v1`. Its manifest hashes the source archive, individual files and native reference library. The archive is an overlay for the pinned base Git revision. A checkpoint is never overwritten by an experiment.

Experiments live under `src/experimental` and have separate build targets, formats and tests. They do not change the normal compressor, its format or its default selection policy. Promotion requires a deliberate code change supported by the recorded comparisons.

Hard failures are any incorrect round trip, silent corruption, default-frame difference, or loss of the protected source. Treat a paired median slowdown over five percent as a regression requiring investigation and a repeat; do not dismiss it as noise or quietly replace the baseline. Compressed sizes are exact. Speed claims require identical hardware, thread budgets, data and verification, with randomized order and repeated samples.

## Benchmark policy

Keep Silesia and OWID as regression sets because they have already influenced development. Canterbury, the signal file and synthetic tests have now also been evaluated during this cycle; they are development evidence, not untouched holdouts. Three additional Canterbury Large files were frozen after GPU-layout tuning and before final validation; retain their results individually. Freeze another input family before the next tuning cycle. Do not describe synthetic wins as general-purpose leadership. Use the same harness for candidates and existing baselines, count complete frames and checksums, and verify every decoded output.

For GPU work, report runtime compilation/setup separately, then include input preparation, copies, command submission, synchronization, returned plans and CPU encoding in warm end-to-end timing. Kernel-only timing is a diagnostic, not the headline. A GPU that is unavailable or slower must leave a working CPU path.

The independent implementation may use established mathematical primitives such as integer prediction, bit packing and CRC32C. Independent source code does not establish novelty of those primitives. Keep provenance clear and compare against current tools instead of claiming invention or guaranteed world leadership.

## Update format

Each update states milestone, measured result, decision and next step. Statuses are in progress, passed, experimental or rejected. Reports distinguish prototype capability from changes promoted to normal Kolmo modes.
