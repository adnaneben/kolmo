# E02C baseline evidence: planner record-ownership repair

Scope: correctness of the already-committed planner ownership repair. This is **not** release or performance
acceptance, and it says nothing about the E02C residual experiment (whose checks are separate; see `PROTOCOL.md`).

## Source identities

| Item | Value |
|---|---|
| Branch | `fix/planner-record-ownership-isolated-20261112` |
| Validated reference commit (= branch HEAD when E02C started) | `2f3308a3380a140ad0f6873ae228de7715e5eef5` |
| Repaired `src/kolmo.c` blob at the reference | `e06467c8872d494b97270811bedf6fd0bac2aa31` (verified with `git rev-parse 2f3308a:src/kolmo.c`) |
| Original pre-repair comparison commit | `3c6890a91314b4d71db1911043ec19b87c16a86d` |
| Original `src/kolmo.c` blob | `db8b7ad665ab0ff61f6634a2d6cb76ed1b17b70a` |

The repair was not re-applied. E02C commits add files under `research/codec/` (and, in the parallel commit `c880659`, the research-only
workflow `.github/workflows/e02c-research.yml`); `src/`, the production Makefile, tests and
`benchmark/accept/TARGETS.md` are unchanged, so the repaired blob above is also the blob at every E02C commit.

## A. Reported CI runs (NOT independently verified in this session)

The task brief reports these runs as successful for the validated reference:

| Run | Workflow | Reported result |
|---|---|---|
| 43187874283 | Isolated planner ownership validation | success |
| 43187874421 | ordinary CI | success |
| 43187874238 | wheels | success |

Reported findings: original known leak reproduced; committed repaired source passed the full main sanitizer;
36 focused planner cases matched; 45 compressed-frame pairs identical; 90 cross-decodes exact; ordinary CI passed.

`BASELINE.md` (committed in parallel on this branch, `4d33976`) records that GitHub reported these runs as
completed/success and lists the dedicated run's artifact ids (focused 13929272548, main-baseline 13933582138,
main-candidate 13932569145); it also states that artifact download and hash verification had not yet been executed.

**Verification status: pending.** This session could clone the public repository but had no GitHub API access
(the request to attach the repository with credentials was declined, and the bundled `gh` token was invalid), so
the run head SHAs, conclusions, `status.json`, `identity.json`, source hashes, command logs and the
focused / main-baseline / main-candidate artifacts could not be downloaded or inspected. No artifact hash from
those runs is recorded here because none was seen. They should be checked and their artifact digests added
before this record is relied on.

## B. Focused phase re-run locally (independent observation, this session)

Because the CI artifacts were unreachable, the cheap `focused` phase was re-run once; the expensive
`main-baseline` / `main-candidate` full-sanitizer phases were **not** re-run.

Command (separate disposable detached worktrees at the two pinned commits):

```
python3 tests/validate_planner_fix.py --baseline <wt>/baseline (3c6890a) --candidate <wt>/candidate (2f3308a) \
    --evidence <local>/evidence/focused-local --phase focused
```

Result `status.json`: `passed_targeted_ownership_and_byte_identity_NOT_RELEASE_ACCEPTANCE`
(original SHA-256 `8356478b04b2534e888634668815194015298bad85fae450e92921794c7f416d`; the copy in
`evidence/focused-local/` has local paths redacted).

| Check | Observed |
|---|---|
| Candidate core blob / head | `e06467c8…` / `2f3308a3…` (validator checks it equals the pinned original plus exactly the three edits) |
| Baseline (original) ASan run | exit 1; `PLANNER_OWNERSHIP cases=36 record_allocations=36 graph_cases=36 leaked_cases=15 outstanding=15`; `SUMMARY: AddressSanitizer: 10800 byte(s) leaked in 15 allocation(s)`, traces in `make_plan` at `kolmo.c:2015` — the **expected** original failure |
| Candidate (repaired) ASan/UBSan/LSan run | exit 0; `leaked_cases=0 outstanding=0`; `PLANNER OWNERSHIP PASSED` |
| Selected-plan identities | 36 baseline lines == 36 candidate lines |
| Frame identity (`tests/test_planner_identity.py`) | `passed_45_frame_pairs_90_cross_decodes_NOT_RELEASE_ACCEPTANCE` (45 identical frame pairs, 90 exact cross-decodes) |

The baseline job "passing" means the original leak was reproduced as expected; it does **not** mean the original
implementation is leak-free.

Toolchain: gcc 13.3.0 (Ubuntu 24.04), Intel Xeon @ 2.10GHz, 2 vCPU; vendor library built with `make -j4 ARCH=`.

Retained in `evidence/focused-local/`: `status.json`, `identity.json`, `candidate.diff` (paths redacted),
`SHA256SUMS` (hashes of the local command logs 000–009, the identity command log and the 200 corpus/frame files;
the 97 MB of frames and logs themselves are not committed).

## C. What this evidence does not cover

- The full main sanitizer suites and ordinary CI on the reference: reported only (section A).
- Any E02C code: the new experimental C paths have their own checks (`PROTOCOL.md`, "Correctness").
