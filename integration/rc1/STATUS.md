# Status of release candidate 1.2.0-rc1

Written 14 November 2026. Branch `claude/release-rc1-20261114`, local only. Build commit `831b980`. The commits
after it add the benchmark harness, results and documents; they change nothing under `src`, `vendor` or `bindings`.

What the candidate contains and where each part came from is in `FEATURES.json` beside this file.

## State

| Item | State |
|---|---|
| Merge of the two integration branches | done on this branch |
| Output bytes | identical to `claude/integration-20261113` (753 frames, 24 settings) |
| Local verification on macOS (Apple M5) | passed; see below |
| Fuzz and leak run | clean; see below |
| Workflow on GitHub (`.github/workflows/ci.yml`) | **not run**: the branch is not pushed |
| Linux, x86 | **not built, not run** |
| Acceptance script (`benchmark/accept/accept.py`) | **NOT ACCEPTED**, 2 open items; see below |
| Single-thread benchmark against the leaders | done on this Mac; `benchmark/rc1/` |
| Pushed, tagged, published, merged to `main` | **no** |

## Verified locally

| Check | Result |
|---|---|
| `make check` | passed |
| Image model tests | passed (24 images, 40 damaged copies each) |
| `make check-sanitizers` | passed (12 injected allocation failures, 144 concurrent round trips) |
| Component tests of the merged features under ASan and UBSan, stopping at the first report (image model, DEC64, fast integer inverse, line classes, planner ownership, command-line options) | passed |
| Extra CI targets (DNA frontier, compact context mixing, tar files) | passed |
| Python binding tests | passed |
| Rust binding test | passed |
| Streaming round trip | passed |
| Reading across versions | rc1 reads all 255 frames of 1.0.1 and all 255 frames of `main`. 1.0.1 refuses 177 of 580 rc1 frames and `main` refuses 33, each with "corrupt or unsupported input", exit status 1 and no output file. |
| lzbench wrapper against the command line | 126 size comparisons, all equal |
| Fuzz step of the CI (20,000 random inputs per target; targets: decompress, round trip, numeric planes), under ASan and UBSan and under the macOS leak checker | 6 jobs, all clean, 0 leaks |
| Fuzz targets on 166 seed inputs (46 frames, 72 round-trip inputs, 48 plane inputs), each with 500 damaged copies, under ASan and UBSan and under the macOS leak checker | 332 jobs, all clean, no sanitizer report, 0 leaks, none stopped at the time limit |

The fuzz runs are bounded runs on fixed seeds. They are not a coverage-guided campaign: no libFuzzer or AFL session
ran.

## Acceptance script

The script compares the candidate with 1.0.1 and with the rivals of its acceptance set. Its verdict on the bytes of
this build (the same bytes as the base branch) is "NOT ACCEPTED: 2 open item(s)":

1. No setting is smaller than paq8px -8 on the acceptance set. The closest is `-x -l 2`: ratio 5.95 against 6.01.
2. `-m hc -l 9` decompresses one file (`mr`) at 0.94 times the speed of 1.0.1.

The thresholds were not changed.

## Format

No container-version change was made. Frames that use block codec 17, graph node 7 or context-mixing model 3 can
carry a container version that `main` knows (3 or 7); `main` then stops part-way with "corrupt or unsupported
input" instead of refusing at the header. Whether such frames should carry a new container version is the owner's
decision.

## Not authorised, not done

Merging to `main`, pushing, tagging, publishing (GitHub release, PyPI, Homebrew, crates.io), closing or changing
other branches and pull requests, changing a default, changing the format, changing acceptance thresholds.
