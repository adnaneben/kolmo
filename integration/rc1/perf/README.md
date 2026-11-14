# RC1 matched performance pilot

Read PROTOCOL.md (frozen before measurements), RESULTS.md, ../FEATURES.json and ../STATUS.md. Current source is the actual combined codec at ae3957ad80d134506ce37b59857af25fe8826062, not an offline selection oracle. PR #10 is draft, not a published release.

The source/ directory contains the exact executed driver, runner, preparation, audit and tests, plus a portable source-stage-validated build helper. Its Git tree was checked against the local files. Full original inputs, frames, timing rows, RSS traces and logs are in the delivered evidence archives whose digests appear in PACKAGE.json. They are not all Git-tracked. The accompanying source ZIP additionally retains the absolute-path historical build commands and generated adapter/header snapshots.

With complete local RC/release/OpenZL checkouts and distinct new output directories:

```sh
python3 source/build.py --repo /path/rc --release /path/released --openzl /path/openzl --output /path/new-build
python3 source/prepare.py --archives /path/delivered-archives --output /path/new-inputs
python3 source/run.py --binary /path/new-build/build/rc1_perf --inputs /path/new-inputs --output /path/new-results
python3 source/audit.py --results /path/new-results --inputs /path/new-inputs
python3 -m unittest discover -s source -p 'test_*.py' -v
```

Requires GCC/G++, CMake, liblzma development files and Linux procfs. Build helper reads pinned source and does not fetch/push or overwrite source files. Timing runner intentionally uses single-core affinity, separate time and memory runs, and explicit resource caps. All original inputs in this pilot are development/diagnostic data. No source-trained OpenZL, fresh final acceptance, M5 or full-Silesia claim.
