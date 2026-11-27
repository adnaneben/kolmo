# 2026-11-13T00:08:30Z — N1 exact-NPY OpenZL comparator bootstrap

**Status:** IMPLEMENTED / GITHUB ACTIONS QUEUED, **not** measured, no P2 acceptance. This is one bounded continuation of the overnight work, not a new independent future task.

## Before work

- Verified PR #7 is draft, open and unmerged; checked isolated branch at `470c3e0616b87fa320243b35c83856e5f94c213e`. Read `AGENTS.md`, `CLAUDE.md`, `benchmark/accept/ACTIVE_POLICY.json`, `research/codec/competitor-policy-v2/DECISION.md` and the overnight plan. Inspected iteration records and P2-E12/E13/E14 results.
- No overnight **experiment** was queued or running. Background `ci`, `wheels`, and planner-validation jobs were left untouched.
- P2-E12 experiment **43326268053** succeeded; its recorded `tp_phuf` and `tp_prod3` type-first production-format frames are development-only. P2-E13's in-memory leaf-choice trials are cost-negative; P2-E14 remains pending. None substitutes for NPY numeric-profile OpenZL measurements.
- Prior overnight budget estimate in the Claude receipt: **~115 of 480 runner-minutes consumed** before this task (approximate, not an independent billing audit).

## Implemented and dispatched

- Updated **only** `.github/workflows/n1-exact-npy.yml` on `fix/planner-record-ownership-isolated-20261112`. Original blob `5ae5b2c6627b74d0d2bde5f4c322b44d787a69a4`, new blob `72e467c544e6d5915c80c8b83c5f363190fd5679`.
- Added one bootstrap `push` trigger restricted to **this exact branch and workflow path**; retained manual dispatch. This was necessary because GitHub does not register branch-only workflow_dispatch workflows until the workflow is otherwise registered (earlier observation: 404). Research-file pushes cannot trigger it again.
- Source/workflow commit **`2e5d51ca6d537a2a2711a33e358bdc4441a3c985`**. GitHub automatically queued N1 exact-NPY run **[43334484927](https://github.com/adnaneben/kolmo/actions/runs/43334484927)** (job **131587099353**) on that commit.
- Last observed job state **queued**, no runner assigned and no steps completed. No second experiment was launched, no queued/running job was cancelled or retried.
- Read-only workflow checks exact adapter tests, builds pinned OpenZL v0.3.0 `32246b48faee46807f84183dac4db479089f5445`, pinned zstd-3 and Kolmo, prepares eight already-observed NPY development inputs, then measures complete original-file encode/decode with exact header preservation and identical-frame confirmation, at most **65 runner-minutes**. The workflow may fail compilation, profile invocation, or verification; do not interpret a queue or successful workflow alone as a rival victory.

## Tests, evidence and limits

- **PASSED (prior committed evidence only):** 5 exact-NPY adapter regression methods / 144 endian/version/lexical combinations and 64 scalar/shape cases in earlier local mock validation (`f701ffc`); no new local tests were run in this dispatch step.
- **PENDING:** Exact committed adapter test step and real OpenZL run 43334484927; paired measurements, archive cost results, malformed-input gate and any P2 claims.
- **NOT RUN:** P2-E14 workflow, new holdouts, ML/RL training, production integration. Frozen historical verdicts unchanged.
- The previous valid OpenZL `serial` generic profile is **not** accepted as an NPY typed comparator. The exact-NPY wrapper adds Python startup/temp-I/O overhead; preserve this distinction when reporting costs.

## Compute and next action

- Actual new experiment runner usage observed at this checkpoint: **0 minutes (queued)**.
- Conservative reservation: **65 minutes** against the approximate night consumption **115/480**. Approximate unreserved balance **300 runner-minutes**; reconcile actual usage across contributors before further launches.
- **Next:** inspect run **43334484927**, collect its artifact and exact input/provenance hashes, audit profile/level validity and paired complete-cost measurements. Only after it completes and the runner slot is free should a new experiment be considered. Do not launch another N1 or P2 runner while N1 is queued/running.
- All changes remain on the isolated branch, PR #7 draft and unmerged. No production/format/default, secrets, release, other branch or PR #6 changes.
