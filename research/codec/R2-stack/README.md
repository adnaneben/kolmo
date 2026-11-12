# Reproduce R2-stack

Read PROTOCOL.md, RESULTS.md, COMBINATIONS.md and EVIDENCE.json first. All sources and data are frozen observed development controls. No production files are edited by this runner.

From the repository root, with GCC, Clang and Python 3 on Linux:

```sh
python3 research/codec/R2-stack/build.py --source "$PWD" --output /absolute/new/path/build
python3 research/codec/R2-stack/run.py --archive /absolute/path/E03_selection_evidence_43250751948.zip --probe /absolute/new/path/build/stack-release --output /absolute/new/path/first --rounds 7 --seed 20261112
python3 research/codec/R2-stack/run.py --archive /absolute/path/E03_selection_evidence_43250751948.zip --probe /absolute/new/path/build/stack-release --output /absolute/new/path/confirmation --rounds 5 --seed 20261113
python3 research/codec/R2-stack/run.py --archive /absolute/path/E03_selection_evidence_43250751948.zip --probe /absolute/new/path/build/stack-asan --output /absolute/new/path/sanitized --rounds 1 --seed 20261114 --correctness-only
python3 research/codec/R2-stack/analyse.py --first /absolute/new/path/first --confirmation /absolute/new/path/confirmation --sanitized /absolute/new/path/sanitized --archive /absolute/path/E03_selection_evidence_43250751948.zip --output /absolute/new/path/analysis
python3 research/codec/R2-stack/test_analysis.py
```

Use actual new absolute paths. The E03 archive is artifact 13961345681 from run 43250751948, SHA-256 `5d02cee72dbbbc8a396e85e29c2cb3f5bf91f26abd2e808995b9a54205660e82`. A hash-matching local source distribution was used for the executed run because this environment could not clone GitHub; its core and graph blobs match the repository. It is not misrepresented as a checkout with Git history. Research source hashes in EVIDENCE.json identify precisely the tested files.

`entropy_calls` counts full-leaf policy trials; an aligned trial can invoke the existing encoder separately for several planes and chunks. All that runtime is included. No per-policy memory result is implied. The raw evidence bundle retains all frames, timings, command logs, compiler receipts and hashes; no new cloud CI pass is claimed by these local results.
