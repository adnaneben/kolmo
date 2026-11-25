# Contributing to Kolmo

Thank you for helping. Kolmo is a lossless compressor, so a change has to get one thing right before anything else:
**every file anyone has compressed must still decompress to the same bytes.** Most of this guide is about how to show
that your change keeps that true.

Kolmo has one maintainer today (see [GOVERNANCE.md](GOVERNANCE.md)), so reviews may take a few days. Small, focused
pull requests with their evidence attached are reviewed fastest.

## Before you start

- **Bugs:** open an issue with the template that fits. If Kolmo returned wrong bytes or failed to decode a file it
  wrote, use **Wrong output or decode failure**: that is the most serious kind of bug this project has.
- **Security issues** (a crash, hang or out-of-bounds access on crafted input) follow `SECURITY.md`, not a public
  issue.
- **Larger changes** (a new transform, a new coder, a new level setting, anything that touches the format): open an
  issue first and describe the data it helps and how you will measure it. That saves you writing code that cannot be
  merged for format or benchmark reasons.

## Build

You need a C compiler (clang on macOS, gcc on Linux, as the Makefile picks), GNU make, and Python 3 for the tests. There are no third-party libraries:
every coder is Kolmo's own.

```sh
make -j            # build/kolmo, the static library and the test programs; tuned for this CPU (-march/-mcpu=native)
make -j ARCH=      # a generic build, as CI uses; use it for anything you will compare or report
make lib           # build/libkolmo.so or .dylib, for the Python and Rust bindings
```

Everything is compiled with `-ffp-contract=off`. Prediction that the encoder and decoder must agree on is integer
arithmetic or plain IEEE doubles without fused multiply-add, which is why x86-64 and Apple-silicon builds write the
same bytes. Keep it that way: no `-ffast-math`, no FMA, no floating-point result whose rounding depends on the CPU.

## Test

Run these before you open a pull request. CI runs all of them on Linux x86-64, Linux ARM64 and macOS for every push
and pull request (`.github/workflows/ci.yml`).

```sh
make -j ARCH= check                    # round trips at every level, the CLI tests, the frozen v12 corpus, hardening
python3 tests/check_release_docs.py build/kolmo   # no removed option shown as current; level tables agree
python3 benchmark/determinism/check.py build/kolmo build/determinism   # same frames on every platform and thread count
python3 benchmark/ledger/ledger.py build/kolmo     # size ledger per file x level, level-order gate (make check-ledger)
```

**Sanitizers.** ASan and UBSan builds of the core tests and of the component tests:

```sh
make -j ARCH= build/libkolmoarch-asan.a
make -j ARCH= check-sanitizers         # levels, v12 corpus, hardening, resource and concurrency tests
make -j ARCH= check-rc                 # component tests (decoders, transforms, coders) under ASan/UBSan
```

**Fuzzing.** The targets are in `fuzz/`. `fuzz/build.sh` is the OSS-Fuzz build script (libFuzzer). Where no libFuzzer
runtime is available (Apple's clang ships none), `fuzz/run_local.c` drives the same targets under ASan/UBSan with
seeds and mutations; CI runs it like this:

```sh
gcc -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -ffp-contract=off -Isrc -Ifuzz \
  fuzz/fuzz_decompress.c fuzz/run_local.c src/kolmo.c build/libkolmoarch-asan.a -lpthread -lm -o fuzz_decompress
./fuzz_decompress 20000                                   # seeds + 500 mutations each, then 20000 random inputs
./fuzz_decompress -300 fuzz/corpus/v12/* tests/v12/*.kolmo # a 300-second mutation campaign over real frames
```

A change to a decoder should come with a fuzzing run over it, and with a regression test in
`tests/test_decoder_hardening.c` for any input that used to crash, hang or allocate without bound.

**Bindings.** If you change anything under `src/`, refresh the copies the packages build from, and commit them in
the same change (CI fails when they differ):

```sh
(cd bindings/python && ./sync-native.sh)
(cd bindings/rust && ./sync-native.sh)
make lib && python3 bindings/python/test_kolmo.py
cargo test --manifest-path bindings/rust/Cargo.toml
```

## The format rules

The compressed format is container version 12. Its specification is [FORMAT.md](FORMAT.md) and the promise made to
users is [COMPATIBILITY.md](COMPATIBILITY.md). The short version, which every change must follow:

1. **Never change the decoded bytes of an existing frame.** A change that makes any version-12 frame decode
   differently is not accepted, even if the frozen corpus still passes. That includes a change to a coder's decoder.
   If a decoder has a bug that returns wrong bytes, the fix restores the original bytes, and the changelog says so.
2. **Claim a new identifier in FORMAT.md first**, in the same change that writes it: codec id, plan type, tflag,
   graph node id, CM model nibble or extension tag.
3. **A new identifier must be one every earlier version-12 decoder refuses**, so an older release stops with an
   error rather than returning wrong bytes. Reserved header and index bytes are ignored by decoders, so they must never
   carry something a decoder needs.
4. **Never edit or regenerate a frame in `tests/v12/`.** The corpus only grows: each release adds its frames.
   `tests/v12.sha256` pins every file there (`make check` and CI); a new release directory is added with
   `python3 tests/check_v12_pinned.py --write`, which never changes an existing line. `MANIFEST` is the one file
   that grows: lines are added at its end only, and each release pins its new length.
5. **A new container version is the last resort**, for a change that cannot be expressed as a refused identifier.

The encoder is free to change: a later release may write different (usually smaller) bytes for the same input.
Within a release, output must stay deterministic. If your change is meant to change encoder output, regenerate
`benchmark/determinism/expected.sha256` with `check.py ... --write` and say so in the commit message. If it is not
meant to, that file must not change.

**Size ledger.** `benchmark/ledger/sizes.tsv` records the compressed size (and SHA-256) of every committed corpus file
at levels 1-9 and ultra, plus a generated multi-block input. CI fails when an entry grows, or shrinks, by more than
0.1% and 16 bytes against it, and prints the change per data kind and level in the job summary (also against the
base branch's ledger). A change meant to change sizes refreshes it in the same PR with
`python3 benchmark/ledger/ledger.py build/kolmo --write`; the diff of `sizes.tsv` is the record of its impact. The
same run checks that no level is larger than the level below by more than 0.5% and 16 bytes, except the inversions
listed with a reason in `benchmark/ledger/level_order_exceptions.txt`.

## Benchmark rules

A change that claims to be smaller or faster must show it. The rules below are the ones the project's own reports
follow.

- **One thread for every tool, Kolmo included.** The command-line defaults of zstd, xz, lz4 and kolmo all use several
  threads. Pass `kolmo -T1`, `zstd -T1`, `xz -T1`, `lz4 -T1`, `bzip3 -j 1`, `7zz -mmt=1`, `zpaq -t1`, and check that
  CPU time does not exceed elapsed time. Multi-threaded numbers are welcome as a separate table, labelled as such.
- **Fair flags.** The same compiler and flags for every tool you build, and the generic build (`make ARCH=`) for
  Kolmo when comparing against tools built without `-march=native`. Name each tool's version and options.
- **Every output decoded and compared byte for byte.** A result whose round trip was not checked is not a result.
- **State the method:** machine and OS, input files (public data, or say why not), number of runs and whether you
  report the best or the median, and units (MiB/s, ratio = input size / output size).
- **Report losses too.** If a change makes some file larger or some level slower, show it. Inputs that were used to
  tune a change are not evidence for it; use held-out files where you can.
- **Measure the level you change, and its neighbours.** Each level must stay smaller than the one before it on the
  screening corpus (`docs/LEVELS.md`); a level's engine may improve, but its place on the scale does not move.

`benchmark/TRADEOFFS.md` describes how the project weighs a ratio gain against a speed loss.

## Commits

Look at `git log` for the style. In short:

- **Subject:** `area: what changed`, lower case after the colon, no final period. The area is the part of the code
  or the document: `klzh2`, `klzma`, `khuf`, `cli`, `api`, `bindings`, `decoders`, `levels`, `fuzz`, `tests`, `ci`,
  `docs`, `format`, `build`. Examples from the history:
  - `klzh2: binary-tree match finder split by hash bucket across threads (levels 5-7, same bytes)`
  - `decoders: stop on exhausted input; bound graph leaves; bounded Y4M parse; IDX1 chain check`
- **Say whether the output changes.** "same bytes" or "encoder only" when frames are unchanged; the measured effect
  when they are not.
- **Body:** why, and the evidence (numbers, the method, the files). Plain sentences.
- **One logical change per commit.** A commit that changes the encoder should not also reformat unrelated code.
- If an AI tool wrote part of the change, a `Co-Authored-By:` trailer naming it is welcome.

## Pull requests

- Base your branch on `main` and keep it rebased; the maintainer merges with a merge commit
  (`Merge PR #N: title`).
- Fill in the pull request template: what changed, whether frames change, how you tested, benchmark evidence.
- Add a line under `## Unreleased` in `CHANGELOG.md` for anything a user would notice. If that section conflicts
  with another pull request, rebase and keep both entries.
- CI must pass. A pull request that changes `src/` without refreshing `bindings/*/native` will fail.
- Release-facing text (README, the bindings' READMEs, `kolmo -h`) must describe the current behaviour;
  `tests/check_release_docs.py` checks part of this.

## License

Kolmo is licensed under the Apache License 2.0 (`LICENSE`). By submitting a contribution you agree that it is
licensed under the same terms (section 5 of the license). Do not submit code you cannot license that way, and name
the origin and license of any code you adapted, as `NOTICE` does for the code Kolmo already adapts.
