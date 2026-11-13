# 20261113T020655Z — Independent P2-E17 cost/selector audit

Status: **IMPLEMENTED, LOCAL_TEST_PASS, REAL_ARTIFACT_AUDIT_PASS, NO_NEW_BENCHMARK_LAUNCHED**. Authorized branch only. Starting HEAD da639cb5b5ca1bec33b33aff3c191d4c15657175; PR #7 draft/unmerged. Source/checkpoint instructions, P2-E17/E18 and overnight prior records read.

Downloaded artifact 14001980117 from successful workflow 43341163635. Local verification on its actual 61-input, 8-policy, two-pass, three-pair data succeeded. SHA-256 ZIP f2e4c589b9e77c3c1cce609f3df120d6a6fc0be6033cc18f2a3cf8ec00669e1a. Verified 7,842 manifest members, 61 additional prepared inputs, 976 records and 2,928 model-cost pairs with repeat frame/source identities. Tests 5/5 PASS. See research/codec/P2-E17-independent-audit/RESULTS.md and RESULTS.json.

Hindsight 47/61 Kolmo robust archive opportunities vs OpenZL typed-9; single fixed tp_pent 41/61, production-only hindsight 37/61. Not acceptance: current rival reads NPY/WAV headers as numeric values, selector/trial overhead uncharged, existing exposed development inputs. No online selector, production implementation, new holdout or RL tested.

At last active-run inspection, P2-E17 completed successfully; P2-E18 export completed successfully; only ordinary branch CI and validation were still in progress. No newly launched overnight research workflow queued or running. Do not duplicate CI. Runner minutes consumed **0** this continuation, total earlier estimate ~176/480, so ~304 remain as a rough ceiling before actual run reconciliation. No additional workflow launched because the remaining native fair comparator and fresh-source gate need reviewed implementation/protocol; no unbounded training.

Historical protocols intact; no branch other than fix/planner-record-ownership-isolated-20261112 touched. Do not merge PR #7 or release.

Next task: evaluate header-exact OpenZL comparator with **native** typing/framing rather than the Python startup overhead, then preregister and audit source-disjoint data before choosing a deployable deterministic selector. No new release/performance claim.

## Exact commit checkpoint

Research source, unit tests and evidence were first committed as `3f966c792d7a99e24d3d663d5e93d0193e80780d`. An independent collaborator appended an unrelated R4 claim at `68c7ddbbb5d41da6b819fc1510fc04c191fa9085`, preserved unchanged. The audit source was then normalized to the byte-exact locally tested blob at `96963eaae1c880e9be21a1d1f3b9dab135d3141c` (blob SHA-1 `4fcb913b7b7e7f0b224bd1d28a3774076054e993`); the test source matches local SHA-1 `c7f68f725071452ffd4e057762864c8569ce5d6e`. No repo-side CI assertion is made for the corrected commit. The local executed 5-test suite and real evidence audit remain the actual verification recorded above.
