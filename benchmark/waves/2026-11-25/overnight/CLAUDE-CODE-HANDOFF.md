# Claude Code handoff for Kolmo

## Start here

For a new worktree, use `codex/ppp-fastq-repetition-guard-20261125` at **`516e1dca02ac8165ed91ef5068af84e44335a109`** ([draft #110](https://github.com/adnaneben/kolmo/pull/110)). It contains pinned main `a1d39559686a9ec17c8614b9154a64d7b5b98672`, the integration through #106, the optional Ultra path (#107), the corrected C-only build (#108), the synchronized native sources in Python and Rust (#109), and the tested repetitive-input fallback (#110). If Claude has already begun on the previously supplied #109 commit `258ca7b60db43cfdfba308c703b2e576cb628fa1`, keep that worktree. Both Claude assignments are independent of the #110 change. The evidence resides on a separate branch under the same directory as this brief. [Engineering results](README.md) · [Open gate register](audit/current-gates.csv)

The user's main checkout may already contain Claude's own branch and untracked work. Use the original checkout for inspection and create a separate worktree for each task. If a matching Claude patch already exists, review and transfer that delta rather than writing it again. A fresh start for the planner task is:

```sh
git fetch origin refs/heads/codex/ppp-fastq-repetition-guard-20261125
git rev-parse FETCH_HEAD
# Expected 516e1dca02ac8165ed91ef5068af84e44335a109
git worktree add -b claude/planner-lifetime-followup ../kolmo-claude-planner 516e1dca02ac8165ed91ef5068af84e44335a109
```

**Codex owns the optional record-block follow-up.** The final stack includes a fix for a 63.7× size regression under a 256 MiB cap: the repetitive input now produces the identical 29,050-byte stock frame, and both favorable source windows kept their smaller frames. The optional route still has a measured 2.55% size penalty on an unrelated varied-width control. [Integration #109](https://github.com/adnaneben/kolmo/pull/109) · [Tested follow-up #110 at 516e1dca](https://github.com/adnaneben/kolmo/pull/110) · [Final guard evidence](fastq/guard/README.md)

## 1. Fix the outstanding planner lifetime warning

At the investigated revision, GCC 16.2 with `-O1 -Wuse-after-free=3` diagnosed a pointer comparison after the associated allocation had been freed in `src/plan/planner.c` around line 727. Inspect current ownership, do any required alias comparison or clearing while the allocation is live, then free it. The historical PR #6 repair is already in main. The later indexed-query repair is also in the integration stack. This warning is a distinct small follow-up. [Finding, compiler and prior hosted receipts](planner/RESULTS.md)

Deliver one focused draft with the before/after diagnostic, the normal planner and deterministic frame tests, and a clean canonical/Python/Rust source comparison after running the repository sync scripts. Reuse a stock input that goes through the affected planner path and preserve its decision and frame bytes. Record any toolchain limitation explicitly. This work can start immediately and independently of the hosted jobs.

## 2. Decide whether the Claude-owned Ultra model is worth implementing

Recover Claude R16's existing LSTM mixer / neural codec work (historical ledger claims 68 and 66). Inventory actual code, measured results and format dependencies. Read the existing CM4, alternate-mask, lexer, prior-pack, GR6+ and reset results before choosing one experiment. Keep the current release candidate and stable writer out of the experiment. If the proposal requires a new model identifier, write down its opt-in wire contract and deterministic reference-decoding behavior before treating it as a production candidate. Historical ratio estimates are hypotheses. [Research and codec ownership](ultra/README.md) · [Other retained and rejected routes](prior-routing/README.md) · [Claim-by-claim audit](audit/legacy-claim-audit.csv)

Start with one distinct text or source producer for development and one sealed independent producer for the decision. Compare stock Ultra, the experimental model and the strongest practical competitor under identical settings on each input. Report whole compressed bytes, decoder/model side data, initialization time, encode/decode MB/s, combined MB/s, peak RSS in each phase, exact input/frame/restored hashes, rounds and run order. Apply the recorded archival acceptance thresholds. Keep an explicit negative if the model misses them. Submit a separate draft or a reproducible negative-result report. A leaderboard claim needs its actual corpus and decoder-inclusive score. [Acceptance gates](prior-routing/README.md) · [Large Text Compression Benchmark rules](https://mattmahoney.net/dc/text.html)

## After those two tasks, the next self-learning experiment

Start from the validated learner and the whole-file target oracle. The existing size selector chose the smallest frame on the three recorded inputs, but on one GMP manual it missed three end-to-end speed floors for which another preset succeeded. Test whether a startup-aware estimate for small files improves floor attainment on **new, sealed producers**. Charge selection time and peak memory, include the unchanged stock path and a duplicate timing control, and preserve the user's speed/ratio objective. Use the prior 186-round learner evidence and 24 paired floor cases as references. The outcome can be a qualifying policy or a documented rejection. [Observed floor misses](target-floors/README.md) · [Whole-file oracle and probe cost](selection-regret/README.md) · [Existing learner](https://github.com/adnaneben/kolmo/pull/99)

## Already measured or assigned

| Work | Where to pick up the evidence |
|---|---|
| Historical, pinned-main and integrated performance; all 80 ordinary settings and 12 opt-in workloads | [All levels](tip-all-levels/README.md) · [Sizes, encode/decode and combined rates](tip-benchmark/README.md) |
| ARM64/x86 Gear, typed tables, PCM, archive bounds, workers and cgroups | [Decision record](README.md) · [#104 integration](https://github.com/adnaneben/kolmo/pull/104) · [#105 follow-ups](https://github.com/adnaneben/kolmo/pull/105) |
| Public target reporting; fast record-block option; C-only controls; bindings | [#106](https://github.com/adnaneben/kolmo/pull/106) · [#107](https://github.com/adnaneben/kolmo/pull/107) · [#108](https://github.com/adnaneben/kolmo/pull/108) · [Final bindings commit](https://github.com/adnaneben/kolmo/commit/258ca7b60db43cfdfba308c703b2e576cb628fa1) |
| Self-learning and dictionary competition | [#99](https://github.com/adnaneben/kolmo/pull/99) · [Cost of native empty-policy export](fast-levels/empty-policy-export.md) · [161 training and 72 holdout files](small-dictionary/README.md) |
| Exact main/RC verification of the rows and corrected BGL results | [96 timed trials and 24 cross-reader checks](prior-routing/README.md) |
| Historical agents with missing outputs and subsequent disposition | [Reviewer records](current-reviewers.md) · [123 assignments](audit/assignment-audit.csv) · [31 reopened gates](audit/current-gates.csv) |
| Hosted benchmark and release-candidate checks | Codex owns the [existing issue #57 entry](https://github.com/adnaneben/kolmo/issues/57#issuecomment-7170272660), [claims benchmark](https://github.com/adnaneben/kolmo/actions/runs/45651729800), [exact RC #96](https://github.com/adnaneben/kolmo/pull/96/checks), [native x86](https://github.com/adnaneben/kolmo/actions/runs/45675425154) and [Windows](https://github.com/adnaneben/kolmo/actions/runs/45677729010). The full enwik8 PAQ8PX comparison resumed separately after an interrupted first attempt. Codex owns its worker and receipts. |

Check issue #57 and the corresponding source PR before starting. Preserve a separate draft and raw evidence for each decision, including negative findings and changes from prior baselines. The user retains merge and release authority.
