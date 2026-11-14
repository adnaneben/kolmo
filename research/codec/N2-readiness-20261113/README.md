# N2 real-codec correctness readiness gate

Additive work for the user's 13 November 2026 PPP following the morning report. This does not extend the expired overnight automation. The frozen N2 implementation, its algorithms, protocol and performance runner are unchanged.

## What this adds

`check.py` is a fail-closed gate before the frozen two-pass performance matrix. It loads the exact registered N2 runner, verifies the native-driver identity and build receipt, and uses the existing SHA-pinned R3 archive preparation. It requests one diagnostic round for each of the eight already-observed inputs and all 12 registered arms. These one-round timers are retained as diagnostics, never promoted as performance or acceptance results.

The extra ledger checks bind every row to the actual original input length and registered batch rule, rather than only requiring consistency between rows. Missing/extra frames, ledger/frame size disagreement, unequal duplicate controls, changed executables and malformed records fail. All 12 frames per input must additionally be reconstructed byte-for-byte by separately supplied, hash-pinned Kolmo, OpenZL and zstd decoders (96 cross-decodes for a complete eight-input run). A decoder config is mandatory; a missing decoder is not a pass.

Each child command has retained arguments, stdout, stderr and exit/timeout status. Timeouts kill the child process group. Default limits are 120 seconds per command and 600 seconds for the gate's execution phase, capped at 180/900 seconds. Input size is capped at 16 MiB and declared frame size at 64 MiB. Peak memory is not measured here. A status and output manifest are retained on prerequisite or execution failure after output-directory creation. Existing output directories are never reused.

## Local verification versus real execution

**22 local synthetic/infrastructure tests passed.** Both Python files also passed `py_compile`. Tests cover wrong-but-self-consistent input lengths/batches, missing/duplicate rows, malformed integers, missing/extra/symlinked frames, frame-size discrepancies, unequal duplicate controls, incorrect decoder identities/configurations, failed byte reconstruction, command failures, timeout handling, frozen-runner rejection and prerequisite-failure receipts.

These tests do not build or execute Kolmo or OpenZL. The actual combined build, real-codec correctness gate, stock cross-decoding, sanitizer gate and fixed two-pass performance matrix remain **NOT_RUN by this continuation**. The gate does not establish vendor sanitizer coverage, fresh-input generalization, an eligible mode winner, or a production-ready codec. No learned ranking, bandit or RL experiment is executed.

## Commands for the next executor

```sh
python3 -m unittest discover -s research/codec/N2-readiness-20261113 -p 'test_*.py' -v
python3 research/codec/N2-readiness-20261113/check.py \
  --repo /absolute/isolated/kolmo \
  --binary /absolute/n2-build/cmake/n2_native \
  --source-receipt /absolute/n2-build/SOURCE_RECEIPT.json \
  --artifact /absolute/Kolmo_R3_Fresh_Validation_43276455708.zip \
  --decoders /absolute/verified-decoders.json \
  --output /absolute/new-n2-correctness-output
```

Build with the existing `N2-matched-native/README.md` procedure first; do not bypass its dependency pins. `verified-decoders.json` has exactly three keys: `kolmo`, `openzl`, `zstd`. Each value contains `argv` (a JSON array with an absolute executable and exactly one standalone `{input}` and `{output}` token) and `sha256` (the actual executable's SHA-256). Obtain decode flags from the actual pinned tools; do not guess or substitute a copy fixture. Commands are executed without a shell. Preserve build/version/source provenance for those independent binaries alongside this config.

Run the genuine instrumented adapter/codec checks separately and record their actual instrumentation coverage. Only after real correctness and sanitizer prerequisites pass should the unchanged `N2-matched-native/run.py` first/confirmation matrix run. This new gate does not dispatch it automatically. Preserve every fixed arm and both OpenZL rivals; do not use a weaker comparator or an uncharged hindsight selector. All comparisons must retain the active type-first, complete-pipeline cost rule.

## Concurrent work and preservation

At preparation, Claude's P2-E23 retry `43381239112` was running at commit `b20835bbecabdbccfd00639cd8d16f8159caa091`. A concurrent N2 source-history refresh was committed at `03e45ed40f3f99986fab796402d702196295db75`. Both are preserved. This directory owns only an additive correctness-readiness gate; it does not claim ownership of the concurrent full N2 execution task.

No experiment workflow is added, dispatched, queued or cancelled by this change. No automation, production source, frozen experiment, format, mode default, other branch, merge or release is changed. Commit these six files only on `fix/planner-record-ownership-isolated-20261112` using a fresh-head fast-forward. Git-tracked code and test evidence are distinct from expiring Actions artifacts; no refreshed source-history artifact covering this addition is claimed here.
