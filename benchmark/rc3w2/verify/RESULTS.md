# rc3 wave 2, verify lane: the merged rc3 on Linux x86-64

Branch `claude/rc3w2-verify-20261117`, from `claude/release-rc3-20261116` at `1536036` (all rc3 lanes merged). Until
now the merged tree had been tested only on the owner's Apple M5. Fuzzing campaign 3 is reported separately in
`fuzz/campaign3/RESULTS.md`.

## Summary

- **Cross-platform determinism holds.** The manifest `benchmark/determinism/expected.sha256` was written on arm64
  macOS. All 122 frames came out byte-identical on x86-64 in each of these builds:
  - gcc 13.3 `-march=native` (AVX2, AVX-512 CPU);
  - clang 18.1 `-march=native`;
  - gcc with the baseline ISA (`ARCH=`, as in CI);
  - gcc `NO_ASM=1`;
  - after the two fixes below.
  Every frame decodes back, and 1 and 4 threads give the same frame.
- **Every test target passes with gcc and clang**: `make check`, `check-rc`, `check-sanitizers`,
  `experimental-check GPU_CHECK=`, `kfam-check`, the CI-only steps, the Python wheel and its tests, and `cargo test`.
  This holds once the three defects below are fixed.
- **Three defects found and fixed**, each with a portable change that keeps the same bytes:
  1. The bindings' `native/` copies lacked `src/plan/target_table.inc`, so **the Python wheel and the Rust crate did
     not compile**.
  2. `make check-cm-compact` did not link: `kolmo_set_x_tables` was defined twice.
  3. `make check-tar-files` failed: the video route mapped every file input.
- **No shipped artefact links a C++ runtime** (table below). libFuzzer's C++ runtime is in the fuzz binaries only.

## Machine

| | |
|---|---|
| CPU | Intel Xeon Processor @ 2.30 GHz, 4 vCPU (cloud VM); SSE4.2, AVX2, BMI2, AVX-512F |
| Memory | 15 GiB, no swap |
| OS | Ubuntu 24.04.5 LTS, kernel 6.18.44 |
| Compilers | gcc 13.3.0, Ubuntu clang 18.1.3 (with `libclang-rt-18-dev` for the fuzzers), GNU ld 2.42 |
| Others | Python 3.13.18 (venv: build, setuptools, wheel), rustc / cargo 1.97.0 |

The fuzzing campaign ran on the same 4 vCPUs during most of these runs. The wall times below are therefore load-bound
and are not benchmarks.

## Results

| step | gcc 13.3 | clang 18.1 |
|---|---|---|
| `make all` (warnings) | pass, 59 s, 0 warnings | pass, 73 s; 6 warnings: `optimize` attribute ignored (audio.c), `&&` with a constant operand (graph.c) |
| `make check` | ALL PASSED (21 min) | ALL PASSED |
| `make check-rc` | pass (3,221 s) | pass (2,513 s) |
| `make check-sanitizers` | pass (139 s) | pass (154 s) |
| `make experimental-check GPU_CHECK=` | pass (101 s) | pass (74 s) |
| `make kfam-check` | pass (157 s) | pass (150 s) |
| determinism (`benchmark/determinism/check.py`) | 122 frames OK | 122 frames OK |
| determinism, `ARCH=` (baseline x86-64, the CI build) | 122 frames OK | – |
| `make NO_ASM=1` (`ARCH=`): `test-dispatch`, `test-asm`, determinism | 28,182 checks, 0 mismatches; references only; 122 frames OK | – |
| CI: `test-fastint`, `test-fastint-generic`, `test-rc-options` | pass | – |
| CI: `check-frontier-dna`, `check-cm-compact`, `check-tar-files` | pass after fixes 2 and 3 (failed before) | pass |
| CI: streaming round trip (`-m fast -S 2`, 16× the sources) | pass (`NO_ASM` binary) | – |
| CI: `make lib` + `bindings/python/test_kolmo.py` + `tests/test_concurrent_dict.py` | pass | – |
| CI sanitizers job: `kolmotest` under gcc ASan+UBSan | ALL PASSED (4,175 s) | – |
| CI fuzz-smoke: `fuzz_decompress`, `fuzz_roundtrip`, `fuzz_nplane` (gcc ASan/UBSan, 20,000 inputs) | pass | – |
| new fuzz-smoke: `fuzz_codecs`, `fuzz_ext`, `fuzz_profile` (KOLMO_FUZZ_NO_CM) | pass | – |
| Python: `python -m build` (sdist, then wheel from it) | `kolmoc-1.2.0rc2-py3-none-linux_x86_64.whl` after fix 1 (failed before) | – |
| Python: wheel installed in a venv, `test_kolmo.py` and the cibuildwheel test command | ALL PASSED / wheel ok | – |
| Rust: `cargo test` (bindings/rust) | 4 tests + 1 doc test pass after fix 1 (failed before) | – |

Clang ran every step in its own worktree, so the fuzzers' build directory was never touched. Its `check-rc`,
`check-sanitizers`, `experimental-check`, `kfam-check`, cm-compact, tar and frontier steps ran on the branch head with
the fixes.

**Not run here:** the macOS and arm64 Linux CI runners (no such machines); `cibuildwheel` itself (it needs Docker for
manylinux; `python -m build` with the same `setup.py` ran instead, plus the cibuildwheel test command); and the
`planner-ownership-validation` workflow (its test, `check-planner-ownership`, is part of `check-rc`).

**Version strings:** `VERSION`, the wheel (`1.2.0rc2`) and the crate (`1.2.0-rc.2`) still say rc2. This is left to the
release step.

## Defects found and fixed

### 1. The Python wheel and the Rust crate did not build (`79598c8`)

`bindings/python/native` and `bindings/rust/native` are the copies of `src/` that the sdist, the wheel and the
published crate build from. They lacked `src/plan/target_table.inc`, the built-in `--target` speed table that
`plan/target.c` includes. As a result both builds failed with
`fatal error: target_table.inc: No such file or directory`:
- `python -m build`;
- `cargo test`, and the CI step "Rust binding round trip", which uses the bundled copy.

The fix was to run `sync-native.sh` for both bindings. Nothing else differed. CI now diffs both copies against `src/`
and `vendor/` (commit `07348fe`), so a missing or stale file fails `build-test`.

### 2. `make check-cm-compact` did not link (`2bcfb26`)

`tests/test_cm_compact.c` includes `src/codecs/cm.c` twice: once in the reference object and once in the compact
driver, and the two are linked together. Since the xmode lane, `cm.c` defines the public `kolmo_set_x_tables`, so the
link failed with `multiple definition of 'kolmo_set_x_tables'`. The fix is test-only: the reference copy now takes
another name. With macOS's linker the same build would fail too, unless that step was skipped there.

### 3. `make check-tar-files` failed; every file input was mapped twice (`c1e56a4`)

The video route (format2 lane, `src/core/compress.c`) mapped the whole input file to compare its first 10 bytes with
`YUV4MPEG2 `. It did this for every file compressed from a descriptor (`kolmo_compress_file`, the CLI), before the
planner mapped the file again. `tests/test_tar_file.c` counts the mappings of its input and saw 5 instead of 4:
`Assertion 'opened == (fail ? 1 : 4)'`. The mapping was released, so nothing leaked, but it cost one extra whole-file
`mmap` per file.

The fix reads the 10 bytes with `pread` (the descriptor position does not change) and maps the file only when it is a
Y4M. Same bytes:
- the determinism check gives the same 122 frames, including `video.y4m`;
- a Y4M compressed through the file path gives the frame it gave before;
- `test-format2` and `check-tar-files` pass.

### Smaller changes

- The fuzz-only guard `KOLMO_FUZZ_NO_CM` was added to `cm_decompress` and `cm_decompress_img`. `kolmo.o` is
  byte-identical with and without it (gcc -O3, sha256 `1d548a52…`).
- CI's fuzz-smoke job also runs the three new rc3 harnesses.

## No C++ runtime in shipped artefacts

| artefact | dynamic dependencies (`ldd`) | C++ symbols (`nm -u`: `__cxa*`, `_Znw*`, `_ZNS*`, `__gxx*`) |
|---|---|---|
| `build/kolmo` (gcc) | libm, libc | – |
| `build/kolmo` (clang) | libm, libc | – |
| `build/libkolmo.a`, `build/libvendor.a` | – | 0 |
| `libkolmo.so` inside the wheel | libm, libc | – |
| Rust crate: `libkolmo_native.a` (built by `build.rs`) | – | 0 |
| Rust test binary | libm, libgcc_s (Rust's own unwinder), libc | – |
| fuzz binaries (`build/fuzz/*`) | libstdc++ (libFuzzer) | libFuzzer's |

libbsc (C++) left rc3 with the bwt lane. The only C++ left in any build is libFuzzer, linked into the fuzz harnesses
alone by clang's `-fsanitize=fuzzer`.

## Commands

```sh
make -j4 all && make check                              # gcc; clang: make CC=clang ... in a separate worktree
make check-rc check-sanitizers kfam-check && make experimental-check GPU_CHECK=
python3 benchmark/determinism/check.py build/kolmo /tmp/det
rm -rf build && make -j4 ARCH= NO_ASM=1 build/kolmo build/test-dispatch build/test-asm   # then the two tests and determinism
make ARCH= build/test-fastint build/test-fastint-generic build/test-rc-options check-frontier-dna check-cm-compact check-tar-files
python -m build bindings/python && pip install dist/*.whl            # then test_kolmo.py from outside the repo
cargo test --manifest-path bindings/rust/Cargo.toml
ldd build/kolmo; nm -u build/libkolmo.a | grep -E '__cxa|_Znw|_ZNS|__gxx'
```
