# 0.0.3 rc backport receipts

rc `a2e430fe` vs #112 `4557e49f`, Apple M5 macOS arm64, Apple clang 21, everything run under `nice -n 15`. `$S` is a scratch folder with clean checkouts `rc/` and `rcfix/`.

- `rc-test.sh`: exact invocations for GCC 16.2, ownership, v12, determinism, frame pairs, features-b and `make check`. Results in `summary*.log` and the per-step `*.log` files.
- `ledger.sh` → `frame-ledger.csv`: 342 rows (input, sha256, level, threads, both frame sha256, cross-decode). 0 rows differ.
- `planner-trace-floats.txt`: instrumented builds (`probe-*.diff`); `floats.f64` reaches the changed branch at levels 4–9 with `chosen=8` in both trees.
- `check-rcfix.log`: `make check` on #112, exit 0, ALL PASSED.

Inputs: the `benchmark/determinism/check.py` generator and `tests/test_planner_identity.py` `generate()` (seed 731094).
