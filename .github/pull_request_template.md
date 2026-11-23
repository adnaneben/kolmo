## What this changes

<!-- One or two sentences. Link the issue if there is one. -->

## Compressed format and output

- [ ] **Decoded bytes:** every existing version-12 frame still decodes to the same bytes (`COMPATIBILITY.md`). Nothing in `tests/v12/` was edited or regenerated.
- Encoder output (pick one):
  - [ ] unchanged (`benchmark/determinism/expected.sha256` untouched)
  - [ ] changed on purpose: `expected.sha256` regenerated with `--write`, and the commit message says so
- [ ] New identifier (codec id, plan type, tflag, node id, model nibble, extension tag)? Claimed in `FORMAT.md` in this PR, and refused by earlier version-12 decoders.

## Tests run

- [ ] `make ARCH= check`
- [ ] `python3 benchmark/determinism/check.py build/kolmo build/determinism`
- [ ] Sanitizers (`check-sanitizers`, `check-rc`) if C code changed
- [ ] Fuzzing run over a changed decoder, and a regression test for any input that used to fail
- [ ] `bindings/*/sync-native.sh` run and committed if `src/` changed

## Evidence

<!-- For a change that claims to be smaller or faster: a table with the method (machine, one thread for every tool,
tool versions and options, runs, outputs decoded and compared), and the files or levels where it loses. -->

## Changelog

- [ ] A line under `## Unreleased` in `CHANGELOG.md`, or not user-visible.
