# Isolated Kolmo repair and codec research

## Writable branch
The user's latest instruction supersedes the older approval to write to PR #6's branch.
Only `fix/planner-record-ownership-isolated-20261112` is authorized for new commits and pushes in this continuation.
Do not modify, move, reset, force-push, merge into, or delete any pre-existing branch. Leave PR #6 unchanged.
Before any repository write, inspect the current branch and working tree. Use an isolated worktree and preserve other users' changes.
Detached disposable checkouts may be built and tested, but must not publish to any other ref.
No merge, release, default promotion, or change to release acceptance thresholds is authorized.

## Source identity
- This branch starts from `a64f57fdeb742f4bf57d44c8cfc994ebf48ae121`, preserving the prior repair and corrected real-planner test.
- Original comparison pin: `3c6890a91314b4d71db1911043ec19b87c16a86d`.
- Original core Git blob: `db8b7ad665ab0ff61f6634a2d6cb76ed1b17b70a`.
- Repaired core Git blob already inherited here: `e06467c8872d494b97270811bedf6fd0bac2aa31`.
Do not double-apply the repair or silently substitute a different baseline.
Read the current source and actual workflow status; earlier chat messages may describe a superseded branch state.

## Immediate validation sequence
Read `patches/planner-record-ownership.patch`, `tests/test_planner_ownership.c`,
`tests/test_planner_identity.py`, `tests/validate_planner_fix.py`, both CI workflows,
and `benchmark/accept/TARGETS.md` before changes.
Use the Linux workflow or separate disposable Linux baseline/candidate checkouts to run the validator's
`focused`, `main-baseline`, and `main-candidate` phases, each with a new evidence directory.
The candidate must already contain the exact committed repaired source; do not patch it silently during validation.
Require actual original leak reproduction, a clean full candidate ASan/UBSan/LSan run,
36 matching focused plan cases, 45 identical compressed-frame comparisons, 90 cross-decodes,
and ordinary CI on the committed repaired source. `make check-sanitizers` alone is insufficient.
Do not suppress diagnostics, disable leak detection, relax classifiers, or treat queued jobs as passes.
Preserve logs, source/input/frame hashes and exact tested commit identities.
The isolated workflow uses read-only credentials and a separate concurrency namespace; it must not cancel the old branch's runs.

## Subsequent research
Only after the correctness gate passes, resume the agreed numeric residual experiment within this isolated continuation.
Keep research commits separate from the repair and its evidence. Do not publish another branch without approval.
Keep KG_INT predictor/GCD and segmentation fixed; compare existing layout and global/local bitpacking/patched packing
with stored, existing fixed Huffman, existing fixed rANS and labelled zstd controls.
Count all metadata, tables, exceptions, checksums, selection/trials, copying, time and peak memory.
Preserve the earlier negative packing-before-zstd development result. Prove byte-exact complete-input reconstruction.
Do not add LZ, floating-point, JSON or neural work yet. Do not reinterpret format IDs or promote defaults.
Synthetic and derived development fixtures are not a final holdout; M5 acceptance remains separate.

## Evidence and reporting
Respect enclosing instructions and retain provenance. Do not fabricate missing artifacts or test results.
Report what changed on this branch, what actually ran, pass/fail/pending/not-run states,
evidence references, limitations and one next substantive task. Do not edit another PR to redirect this work.
