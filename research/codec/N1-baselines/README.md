# N1: pinned designated-rival baselines

Status: implementation and local infrastructure checks, before cloud execution. This is the N1 owner task left open by Claude's P2-E1; do not duplicate P2-E2.

Protocol/claim: `overnight-20261112/iterations/20261112T201500Z-N1-claim.md` at `f08c92bb8c76a20c285e56287ee5b3c28d04057c`. No production changes. All current data is observed development.

OpenZL v0.3.0 was verified as the latest stable official release on 12 November 2026, pinned to `32246b48faee46807f84183dac4db479089f5445`. Use official unmodified CMake build, custom parsers/numeric model retained. Its default CMake CLI also builds XGBoost training support, but no training is run. Pin resolved dependencies and flags in the evidence. zstd-3 is built from the same dependency's 1.5.7 source. Existing Kolmo controls must match the repaired src/vendor tree at `2f3308a`.

Compare the eight hash-frozen R3 NPY inputs and two exact deterministic CSV views. `numpy`/`csv` profile levels 6 and 9 are distinct OpenZL configurations; serial levels 6 and 9 are generic controls. Primary archive reference is typed profile level 9, untrained per source (built-in pretrained models are part of the pinned release). Unsupported configurations are reported, not silently swapped. Comparators are matched COMPLETE CLI processes, with one assigned CPU and thread environment, startup and I/O, independent encode and decode phases, child-specific CPU and max RSS, 3+3 randomized repetitions and full byte reconstruction. Hardware/system-level cache noise and per-process startup are limitations. No in-memory or leaf timer is interchangeable with this measurement boundary.

The original source rows were converted by R3; this phase reconstructs its complete NPY bytes and the fixed CSV bytes, not the original publishers' text. NPY and CSV views are correlated and must be reported separately. No fresh-policy acceptance or whole-Kolmo superiority is claimed; a reference to the active cost formula does not prove a column-first planner or new-data generalization.

## Execution

```
python -m unittest discover -s research/codec/N1-baselines -p 'test_*.py' -v
python research/codec/N1-baselines/baselines.py prepare R3.zip EVIDENCE/prepared
python research/codec/N1-baselines/baselines.py run EVIDENCE/prepared EVIDENCE/first --zstd ZSTD --openzl ZLI --kolmo KOLMO
python research/codec/N1-baselines/baselines.py run EVIDENCE/prepared EVIDENCE/confirmation --zstd ZSTD --openzl ZLI --kolmo KOLMO --seed 20261113
python research/codec/N1-baselines/baselines.py confirm EVIDENCE/first EVIDENCE/confirmation
```

The job uploads success and failure logs and a frozen competitor source checkout so local reproduction can continue even when shell DNS is unavailable. Artifacts expire after 30 days; source/compact receipts remain in Git. No credentials, git configuration or remote checkout tokens are included. Reserve 60 runner-minutes from the overnight 480-minute budget; reconcile actual elapsed time after completion. Do not relaunch a queued job.

Primary sources: `https://github.com/meta/openzl/releases/tag/v0.3.0`, pinned `CMakeLists.txt`, `build-scripts/cmake/openzl-deps.cmake`, `tools/ml_selector/CMakeLists.txt`; CLI usage `https://openzl.org/getting-started/quick-start/`. No third-party performance numbers are treated as measured Kolmo results.
