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

## Effect map ([docs/ACCEPTANCE.md](../docs/ACCEPTANCE.md))

<!-- Required for any change that can alter bytes, speed or memory. Produce it with benchmark/effectmap
(`effectmap.py run` in sizes mode, then `queue` for the timed part) and paste the lines below from effectmap.md.
A pure correctness fix with identical frames (determinism check) and cost within noise may write "not applicable". -->

- Builds: A = `<commit>` (sha256 `…`), B = `<commit>` (sha256 `…`); mode: sizes / timing; levels; threads 1/4/10
- **Verdict:** KEEP / PASS / ROUTE / TRADE / FAIL, and per tier (fast 1-3, middle 4-7, high 8-9, ultra)
- Never allowed: level inversions A → B (added / removed), thread-dependent bytes, decode slower at 1-7, RSS > 1.10×
- Per-type table (bytes and encode per level), worst cell and worst file (never averaged away)
- Stacking: partners measured; amplifiers found (kept and routed)
- Rules found on dev and their holdout score; **suggested gate**, and whether this PR implements it (a gate that
  changes bytes never depends on thread count)
- For a TRADE: new mode (ultra+, archive, video-archive with decode fps ≥ source fps) or rebase, and its point on the
  perf graph against the world leaders
- [ ] Timed part queued in `QUIET_WINDOW_QUEUE.txt` or done (rows path)

## Changelog

- [ ] A line under `## Unreleased` in `CHANGELOG.md`, or not user-visible.
