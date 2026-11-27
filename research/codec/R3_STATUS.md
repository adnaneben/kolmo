# R3 current status — validation completed

Updated 12 November 2026. **GitHub Actions run 43276455708 completed successfully; the registered scientific verdict is provisional in one family.** Read [R3_RESULTS.md](R3_RESULTS.md) and [R3_VALIDATION_EVIDENCE.json](R3_VALIDATION_EVIDENCE.json).

The frozen implementation is `06db7f2f2849fa6ccdc207227cfaf4498d17870e`, preregistered in `c72628a6330e224a59a5a7e1d995a2a4439c7b51`. Both new source families have substantial size savings (Covertype 14.38%, CollegeMsg 15.50%; pooled 14.70%). CollegeMsg meets every registered criterion in both passes. Covertype fails the entropy-kernel decode limit (1.55-1.56x), despite full verified decoding staying near 1.06x. The rule is not relaxed.

The downloaded archive digest, all 282 manifest entries, 1,490,225 exact source values, 120 frame hashes, 800 reported medians and the independent decision calculation have been verified. A separate local sanitizer replay reproduces all 40 cloud configuration frames and decisions. No experiment remains queued for this R3 run.

The historical local-development checkpoint and its interruption/fixture history are preserved in commit `46e6f7666cc866e3887cf2f66af4c705b0f112c5` and `R3_EVIDENCE.json`. Its queued state is no longer current; its development results are not relabelled as fresh validation.

Only the isolated branch is writable, PR #7 stays draft/unmerged, and no production code/format/default/release criterion or other branch changes are authorized. Parallel Claude Code graph-plane work is preserved and not pooled with this native-component study.

Next substantive task: the bounded decode-cost-aware codec-choice ablation described in [R3_NEXT_TASK.md](R3_NEXT_TASK.md). Keep source, registration and execution receipts exact; no post-hoc promotion, merge or release.
