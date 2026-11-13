# 20261113T030758Z — publication of bounded P2-E17 audit regression

Status: **LOCAL_TEST_PASS; REAL_ARTIFACT_EVIDENCE_AUDIT_PASS; ADDITIVE_BRANCH_PUBLICATION_ATTEMPT**. No competitor acceptance, compression improvement, native OpenZL comparison, or new codec experiment is claimed by this record.

## Inputs and guardrails

Authorized writable ref only: `fix/planner-record-ownership-isolated-20261112`. Refreshed base HEAD before publication: `b718bbf40d3978f8816bb5ba9abe29f5f24f12e9` (Claude P2-E21 preregistration). PR #7 was OPEN/DRAFT/UNMERGED. Read current `AGENTS.md`, `CLAUDE.md`, `benchmark/accept/ACTIVE_POLICY.json`, `research/codec/overnight-20261112/PLAN.md`, policy NEXT_TASK/DECISION and recent E17, R4, N1-native and P2-E20/E21 iteration records. No remote checkout/working-tree mutation; local staging was a partial extracted tree, not a Git clone.

An earlier isolated session prepared `/mnt/data/kolmo-e17-evidence-regression.patch` and its 11 new-file additions. This continuation independently checked that the two new remote paths did not exist at the inspected HEAD; it preserved all existing `P2-E17-independent-audit/`, `R4-singleton/`, `P2-E20/` and `P2-E21/` records. The original `20261113T021934Z-P2-E17-evidence-regression-local.md`, README and TEST_RECEIPT describe the *earlier local-unpublished checkpoint*; they are intentionally preserved byte-for-byte as historical evidence. Reachability of the new Git commit, rather than those historical labels, determines current publication status.

## Exact executed local commands

```sh
cd /mnt/data/kolmo-continuation
python -m unittest discover -s research/codec/P2-E17-audit -p 'test_*.py' -q
python research/codec/P2-E17-audit/audit_e17.py /mnt/data/p2e17-cli-43341163635-1.zip --output /mnt/data/p2e17-rechecked-20261113.json
sha256sum /mnt/data/p2e17-rechecked-20261113.json research/codec/P2-E17-audit/AUDIT_RESULT.json
sha256sum -c research/codec/P2-E17-audit/SHA256SUMS
```

PASS: 42/42 synthetic evidence-auditor regression cases; original frozen real artifact rechecked with 7,842 manifest entries, 61 separate inputs, 5,856 timing events, 2,928 paired modeled costs and 488 matching recorded frame identities. Recomputed full audit result SHA256 `fff032f2be4ebbdec6695a96a4f43a17b82e04ae4e019218a38f7f7984df7db8` matched the locally archived result; every `SHA256SUMS` entry passed. Verified source SHA256: `audit_e17.py` `684553c0861f02be29c4aafeff01e8522e28a719a72013b57b1125a0ebf0b277`; `test_audit_e17.py` `14769e39ca8d6ee602c066ed12360dbda58fbdfee14b75e413a1a695216d49e6`. Each imported GitHub blob SHA was checked against the original additive patch index before assembling the Git tree.

Previously attempted 64 MiB input-member limit: FAIL-SAFE, fixed to 128 MiB in the exact tested source; this only modifies the read-only auditor, never codec resource limits. The 47/61 strict-smaller archive opportunities are an *offline hindsight frontier*, not a timed online selector. Entire-frame post-verification is outside the original harness timer, and OpenZL header treatment is not a native fair match. All development data were already exposed; no holdout used or tuned.

## Concurrent work and compute

At the pre-publication checkpoint, research run `43355382170` (P2-E21) completed with a successful *workflow execution* around 03:07 UTC; its hypothesis and artifacts were **NOT AUDITED** in this continuation. No other contributor's job was duplicated, cancelled or modified. No overnight experiment launched. This continuation used **0 new experiment runner-minutes**, with **0 new experiment reservations**. Historical workstream estimate before E21 was approximately 190/480 runner-minutes; the E21 job ran approximately 12.2 minutes, so approximately 277.8 minutes remain *by this approximate ledger only* (not verified billing or full cross-workstream reconciliation). Normal auto-triggered CI on a branch write is not a newly launched research experiment.

NOT RUN: rebuilt codec binaries, independent frame re-decoding, new sanitizer executions, native header-exact OpenZL measurements, fresh source-disjoint validation, full-pipeline selector charging, ML/RL and production integration. The prior real artifact lacks recorded compressed frames; evidence consistency must not be labelled P2 acceptance.

## Publication and next prerequisite

The patch is additive to `research/codec/P2-E17-audit/` and the original dated local receipt, plus this new record. Git objects are committed with a single parent at the refreshed authorized branch HEAD; branch advancement must use normal non-force compare-and-swap and be checked after update. If another contributor advances the branch, refresh/reconcile, not force-push.

**Next substantive task:** inspect the already claimed `N1-native-exact/` native OpenZL comparator once ready and audit P2-E21's source-disjointness and complete-cost evidence before a deployable type-first selector is accepted. No further nighttime run or recurring automation authorized.
