# Planner pointer-lifetime warning, 25 November 2026

Handoff task 1 (`CLAUDE-CODE-HANDOFF.md` on `codex/ppp-audit-and-results-20261125`). Base: `516e1dca02ac8165ed91ef5068af84e44335a109`
(`codex/ppp-fastq-repetition-guard-20261125`, confirmed with `git rev-parse FETCH_HEAD`).

## Change

`src/plan/planner.c:727`, in the large-input structured planner, releases a record plan that a later candidate displaced:

```c
/* before */ if (owned_rec && chosen != 3) { free(owned_rec); if (keep.rec == owned_rec) keep.rec = NULL; }
/* after  */ if (owned_rec && chosen != 3) { if (keep.rec == owned_rec) keep.rec = NULL; free(owned_rec); }
```

The comparison read only the pointer value, never the memory, so it was not a dereference. But using a pointer value after
`free` is indeterminate in ISO C, and GCC is entitled to flag it. Ownership is unchanged: `owned_rec` is the only owner, and
`keep.rec` is a borrowed alias that is cleared before the release. The same one-line change is applied to the canonical source
and to both vendored mirrors through `bindings/python/sync-native.sh` and `bindings/rust/sync-native.sh`.

## Diagnostic before and after (GCC 16.2)

Toolchain: Homebrew `gcc-16 (Homebrew GCC 16.2.0) 16.2.0`, macOS arm64 (Apple M5). This is the GCC version the finding
named, so no substitute was needed. Command: `cd src && gcc-16 -O1 -Wuse-after-free=3 -I. -c kolmo.c` (`planner.c` is
compiled as part of `kolmo.c`).

| Tree | Warnings |
|---|---|
| base, `src/` | 1: `plan/planner.c:727:61: pointer 'owned_rec' used after 'free' [-Wuse-after-free=]` ([gcc16-before.log](gcc16-before.log)) |
| base, `bindings/python/native/src/` | 1 (same) |
| candidate, `src/` | 0 ([gcc16-after.log](gcc16-after.log) is empty) |
| candidate, `bindings/python/native/src/`, `bindings/rust/native/src/` | 0 and 0 |

## Mirrors

After both sync scripts, `git status` shows only the three `planner.c` files changed. `diff -r src bindings/{python,rust}/native/src -x experimental`
is clean. All three `planner.c` files have the Git blob `d257b86cee8f51cd8815e4124f5100346aff9075`.

## Tests (candidate, Apple clang 21, macOS arm64)

* `make check-planner-ownership` (ASan/UBSan): `cases=72 record_allocations=48 graph_cases=72 leaked_cases=0 outstanding=0`, PASSED ([log](planner-ownership.log)).
* `python3 benchmark/determinism/check.py build/kolmo …`: `152 frames checked; OK` against `expected.sha256`.
* `build/test-v12-corpus` and `build/test-v12-corpus-asan`: both PASSED (357 frozen frames each).
* Planner identity corpus (the nine seeded inputs from `tests/test_planner_identity.py`), levels 1 to 9 at `-T 1`: all 81
  base/candidate pairs are byte-identical, and each frame cross-decodes with the other build.
  `tests/test_planner_identity.py` itself cannot run on this tree. It still passes `-m speed|balanced|hc|max`, which was
  removed in 0.0.2, so it exits on the baseline binary before comparing anything. It is left unchanged here because it is
  a separate follow-up.

## Stock input through the affected path

A temporary build (in scratch, never committed) printed a line whenever the changed branch ran. Of the 10 stock
determinism inputs at levels 1 to 9 and the nine identity inputs, the branch ran only for `floats.f64` (the
`benchmark/determinism/check.py` generator, 320,000 bytes, sha256 `284fcaca…a105`) at levels 4 to 9. There, a record plan
is displaced by the DEC64 segment plan (`chosen=8`). `keep.rec` had already been overwritten (alias = 0), so the plan
decision cannot change. The alias-equal case is covered by the ownership test above.

Frames from base and candidate on that input, at 1 and 4 threads ([full table](stock-floats-f64.txt)):

| Level | Engine | Bytes | Frame SHA-256 (base = candidate) |
|---|---|---:|---|
| 4, 6, 7 | hc: graph | 57,961 | `54ea7bb4…02f2` |
| 5 | eco: graph | 57,824 | `d3d16451…eb97` |
| 8 | max: bwt | 57,650 | `e9132fcd…f593` |
| 9 | max: bwt | 57,650 | `10ea1cf5…1afa` |

All 12 pairs are identical, and every frame decodes back to the input with the other build.

## Codegen

Apple clang 21 `-O3` CLI binaries differ: the candidate is 4 bytes shorter. Once branch addresses are normalized, the only
instruction-level difference is in `make_plan`: the pointer compare and `csel` now come before the `free` call and reuse
its argument register, and a few spills nearby are reordered. Every other function is identical. No size, ratio or
speed claim is made.

## Toolchain limitations

* GCC 16.2 was used only for the diagnostic. All tests and frames come from Apple clang 21 builds; GCC was not used to
  build or test the full CLI on macOS.
* Linux x86 and hosted CI were not run locally. Codex owns those.
