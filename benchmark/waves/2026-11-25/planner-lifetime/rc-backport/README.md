# 0.0.3 rc backport receipts

rc `a2e430fe` vs #112 `4557e49f`. Apple M5, macOS arm64, Apple clang 21 for builds and tests, Homebrew GCC 16.2.0 for the lifetime gate. Everything ran under `nice -n 15`. `$S` is a scratch folder holding clean checkouts `rc/` (a2e430fe) and `rcfix/` (4557e49f).

## Replay
- `gcc-gate.sh $S`: the lifetime gate. It prints the compiler version, then for each of the 6 trees the exact command, its exit code and the warning count (`gcc-gate-*.log` holds the full output). Result, in `rerun.log`: rc 2/2/2 warnings (canonical/Python/Rust), #112 0/0/0, all 6 exit 0.
- `rc-test.sh $S`: builds, `check-planner-ownership`, determinism, `check-features-b` and `make check` on #112 (`summary.log` and the per-step logs). Its v12 step piped output through `tail`, so it has been replaced by the exit-coded runs below.
- `rerun.sh $S`: v12 readback with exit codes (`v12-<tree>-<bin>.log`), the GCC gate, the ledger self-test, then the ledger. Output in `rerun.log`.
- `ledger.sh $S` → `frame-ledger.csv`. It runs one process at a time, with no pipelines. Each row records both encoder exits, both frame SHA-256s, both cross-decoder exits and a byte comparison of each restore. A row passes only if all of them succeed, and the script exits nonzero if any row fails. Result: 342 rows, 0 failed. `ledger.sh $S --self-test` feeds an empty input to a stand-in decoder that exits 1, and checks that the row fails and the harness exits nonzero (`ledger-self-test.log`: SELF-TEST PASSED).
- `planner-trace-floats.txt`: from instrumented builds (`probe-*.diff`). `floats.f64` reaches the changed branch at levels 4–9 with `chosen=8` in both trees.
- `check-rcfix.log`: `make check` on #112, exit 0, ALL PASSED.

Inputs: the `benchmark/determinism/check.py` generator and `tests/test_planner_identity.py` `generate()` (seed 731094). Timing was not measured, because of the load rule.
