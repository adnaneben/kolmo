# Isolated Kolmo repair and codec research

## Writable branch
Only `fix/planner-record-ownership-isolated-20261112` is authorized for new commits and pushes in this continuation.
Do not modify, move, reset, force-push, merge into, or delete any other pre-existing branch. Leave PR #6 unchanged.
Before repository writes inspect HEAD and the working tree; use isolated worktrees and preserve concurrent changes.
No merge, release, default promotion or silent format change is authorized.

## Latest user decision: competitor-bounded acceptance (12 November 2026)
The user explicitly replaced the improvement threshold. Read `benchmark/accept/ACTIVE_POLICY.json` and
`research/codec/competitor-policy-v2/DECISION.md` before new research. This supersedes earlier instructions forbidding
prospective acceptance-rule changes. Choose the smallest COMPLETE legal byte-lossless pipeline whose measured cost
fits the mode competitor: zstd-3 for rt/fast (speed/balanced), OpenZL for eco/hc/max/ultra (ratio aliases max).
Use the recorded existing speed round-trip and archive CPU/storage models; freeze rival/profile/cost parameters.
Type columns BEFORE graph, leaf and cross-column planning. Count typing, sampling, inference, rejected trials,
lexical reconstruction, metadata, models, checksums and all shared work exactly once. Retain layered candidates and
combination ablations; small standalone gains can be enabling, overlapping or context-dependent.
Historical TARGETS.md, accept.py, registered protocols and verdicts remain frozen for reproducibility. They are not
the active prospective policy, and a legacy ACCEPTED result is not acceptance under P2. Do not relabel old failures.
R3 component timings do not establish a zstd-3/OpenZL whole-file win. Missing/incomparable evidence is not a pass.
RL is now an allowed research option, not an implementation prerequisite: compare deterministic bounded planning,
supervised ranking and contextual bandits before sequential RL. Keep learned selection encoder-only, with hard
compatibility/resource admission outside its reward, and account for training/model/inference costs.
No production C planner or learned model has been integrated by the reference-policy commit.

## Source identity and historical correctness
- Branch origin: `a64f57fdeb742f4bf57d44c8cfc994ebf48ae121`, preserving the repaired planner and regression test.
- Original comparison pin: `3c6890a91314b4d71db1911043ec19b87c16a86d`.
- Original core blob: `db8b7ad665ab0ff61f6634a2d6cb76ed1b17b70a`.
- Repaired core blob: `e06467c8872d494b97270811bedf6fd0bac2aa31`.
Do not double-apply the repair or silently change the source reference. Read actual source/workflow evidence.
The repaired reference `2f3308a3380a140ad0f6873ae228de7715e5eef5` passed the recorded production correctness gate:
original leak reproduction, full candidate sanitizers, 36 planner cases, 45 frame pairs and 90 cross-decodes.
Preserve exact logs/hashes and tie results to their executed sources. A new change is not covered automatically.
Do not suppress diagnostics, disable leak detection or treat queued work as passed.

## Research preservation and next task
Keep completed E02C/D/E/F/G, E03, R1, R2 and R3 evidence/protocols intact. Preserve concurrent Claude work.
Implement new experiments in separate research paths, not by rewriting frozen implementations.
All previously observed inputs are development for new rules; audit the full benchmark/holdout/source registers.
Keep native/hybrid dependencies and byte-exact versus typed-value boundaries explicit. Reuse existing tbl2/graph
facilities before replacing parsers. A joint cross-column graph is not the sum of independent column choices.
Read `research/codec/competitor-policy-v2/NEXT_TASK.md`: establish complete typed-pipeline and designated-competitor
measurements under P2 before new selector training or production integration. No new LZ/float/JSON engine is required
for that step. New wire semantics still need separate review; never reuse an old ID for a different inverse.
Run the reference tests with `python -m unittest discover -s research/codec/competitor-policy-v2 -p 'test_*.py'`.

## Reporting
Commit source, tests, decisions and compact evidence on this branch. Distinguish Git-tracked content from expiring
Actions artifacts and separately attached historical archives. Preserve provenance; never invent missing results.
Report exact changes, tests and commit, pass/fail/pending/not-run states, limitations and one next substantive task.
