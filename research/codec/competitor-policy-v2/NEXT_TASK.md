# Codex / Claude Code continuation: competitor-bounded typed planner

Work only on `fix/planner-record-ownership-isolated-20261112`. Preserve other work and
all branches; leave PR #7 draft/unmerged. The user's new rule authorizes prospective
acceptance changes, not a merge, release, force-push or silent format change.
Read AGENTS.md, benchmark/accept/ACTIVE_POLICY.json and this directory's DECISION.md.

1. Verify current HEAD, complete diff and active instructions; inspect existing tbl2,
   graph and columnar decode facilities before writing a duplicate parser. Preserve
   historical TARGETS.md, old experiments and their registered verdicts.
2. Run `python -m unittest discover -s research/codec/competitor-policy-v2 -p 'test_*.py'`.
   These are reference-policy and typing tests, not a measured compression win.
3. Freeze exact zstd-3 and OpenZL build/profile/level/graph configurations and the
   existing mode cost parameters. Declare trained/untrained and amortization regimes.
   Do not accept a leaf timer or a derived-input result as a whole-original-file win.
4. Export typed-column descriptors, null/lexical escapes, row-order reconstruction,
   and bounded cross-column candidate DAGs from the existing C path. Typing must happen
   before candidate/leaf choice and its cost must be included even on fallback paths.
5. Compare full layered pipelines with the retained R3 native path, the original
   complete Kolmo path, exact raw fallback and both designated competitors. First use
   observed development inputs for plumbing, then preregister new source-disjoint
   validation. Keep layering and candidate interaction ablations explicit.
6. Use the reference selector as an executable acceptance contract, NOT as a free
   online oracle. Add actual search/inference/trial overhead to measured encodes.
   Refuse missing or incomparable measurements. Small improvements may qualify when
   cost and replication are sound; do not silently restore the old % gain thresholds.
7. Evaluate deterministic bounded search before learned ranking/bandits. Consider RL
   only for a demonstrated sequential graph-search gap; keep ML encoder-only and
   enforce cost/memory/compatibility outside its reward. Train only on development.
8. Commit source/tests/protocol/results on this same branch. Preserve compact raw
   measurements and hashes in Git; large reproducible data/frame artifacts may stay
   external with explicit retention limits. Never label an expiring artifact as a
   permanent source backup. Report exact commits, tests, unresolved evidence and one
   next task. No whole-codec or release improvement until matched evidence exists.
