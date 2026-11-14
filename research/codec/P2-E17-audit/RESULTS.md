# P2-E17 evidence-regression results

**Status: PASSED_EVIDENCE_CONSISTENCY_ONLY. Codec acceptance: NOT_EVALUATED.**
Local implementation/tests completed; no remote commit or new experiment launched.

The exact local auditor verified the externally pinned 32,474,563-byte artifact,
7,842 manifest members and 61 additional input files. It reconciled 976 records
(488 configurations per pass), 5,856 raw encode/decode timing events and 2,928
paired model-cost samples. All 488 recorded compressed-frame identities match
across passes. It checked the recorded 183-case sanitizer-gate log; no sanitizer
or decoder was rerun here. Compressed frames and binaries are absent from the
artifact, so independent re-decoding/rebuilding is NOT RUN.

Inputs are 50 NPY, 10 WAV and one CSV, all previously observed development.
Largest input: 82,741,337 bytes. Family labels do not establish independent sources.

## Joint six-pair offline diagnostic

For each input, require one fixed configuration to satisfy the rival's cost in
all three pairs of both passes. The rival's aggregate size is 24,877,083 bytes.

| Candidate subset | Strict smaller-byte cases | Selected bytes | Byte-weighted saving |
|---|---:|---:|---:|
| all_codec_candidates | 47/61 | 22,247,449 | 10.570508% |
| production_only | 37/61 | 22,334,859 | 10.219140% |
| research_only | 43/61 | 23,572,012 | 5.246077% |

These are alternatives, not sequential stacking gains. Do not sum percentages.
The all-candidate offline frontier selects max on 17, tp_pent on 15, tb_best on 7,
fast on 6 and hc on 2; OpenZL retains 14. Two cases change their cheapest feasible
winner between passes: E02F__pageviews_London and FSDD__8_jackson_0 move from max
to tb_best. The strict-win count remains 47 under the joint six-pair rule.

The research-only subset retains tp_arch19 on eight inputs even though that
configuration never wins in the larger portfolio. This is contextual evidence,
not a new cumulative byte gain and not proof of production eligibility.

## Scope limits that prevent acceptance

- Cross-configuration selection is an offline oracle. Its combined search/trial
  costs are not an actually timed online selector. Individual configurations may
  include their own leaf trials; that does not pay for the outer oracle.
- Existing production-mode controls do not establish mandatory type-first planning.
- OpenZL's numeric profiles consume entire NPY/WAV headers as numeric values.
  These measurements are not a native header-separated, strong matched frontier.
- Stored frames and full reconstruction are recorded by the original CLI harness;
  its external hash postverification is outside the measured CLI timer. Audit
  consistency does not retroactively repair measurement boundaries.
- No fresh source-disjoint validation, codec implementation, learned model,
  generalization, production integration, release or competitor win is established.
- The source/binary hashes are execution receipts, not independent rebuilt binaries.

## Actual local test outcomes

42 regression tests PASS on the exact supplied source. They reject missing or
duplicate events, bad costs/medians, wrong profiles/flags/input boundaries,
changed environments, bad source/frame receipts, partial sanitizer evidence,
unsafe members and incomplete paired comparisons. The 3% CPU override and
strict byte-win/tie distinction are explicitly tested. See TEST_LOG.txt and
TEST_RECEIPT.json. AUDIT_RESOURCES.txt describes this local read-only audit only,
not codec performance or GitHub billed runner time.

An initial real-artifact attempt failed safely at the auditor's 64 MiB member
limit. The actual CSV is 82.7 MB; the read-only auditor's own limit was raised to
128 MiB, still bounded by 1 GiB total expanded bytes, before the exact final tests
and real audit were rerun. No codec/workflow limit was changed or bypassed.
