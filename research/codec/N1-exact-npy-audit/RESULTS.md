# N1 exact-NPY comparator: independent audit, 13 November 2026

Status: **audited development controls, NOT Kolmo P2 acceptance**. New research-only auditor and regression tests, no production source/wire/default changes.

Source run: [43334484927](https://github.com/adnaneben/kolmo/actions/runs/43334484927), completed **success**, source commit 2e5d51ca6d537a2a2711a33e358bdc4441a3c985. Artifact 13998596519, ZIP SHA-256 5e0f1960b5cc9a841cc6ce4d1d53b103f3a287fdb4c7cd9c4f08e071e5a7dcff, expiration 13 December 2026.

Independently audited 1,430 top-level manifest hashes, all 10 input bytes via their separate prepared source manifest, 88 complete configurations in two passes, 1,056 recorded child timing events, all 528 encoded/decode cost pairs and matching reported frame identities (88/88). All 88 configurations were reported verified and successfully round-tripped; actual compressed frames were NOT retained in the ZIP, so this audit cannot independently decode them. First and confirmation seeds differ, executable digests match. Recomputed costs use P2's 125 MB/s transfer, 3.3 reads, $3.89/72 per vCPU-hour and $0.023 per GB-month.

**Key comparator limitation:** OpenZL v0.3.0 typed numeric levels 6/7/9 were successfully exercised on eight NPY inputs, with original NPY headers preserved. The adapter is a Python subprocess/temporary-file CLI wrapper, however: OpenZL whole-file decode takes about 45-50 ms on these small inputs, versus roughly 3-5 ms for the native Kolmo executable. This is a significant asymmetric wrapper penalty. Reported old Kolmo quick control has lower modeled archive cost in 8/8 NPY inputs but is smaller in bytes than the best-byte-size typed OpenZL level in only **2/8**. Neither demonstrates an admissible P2 victory: the Kolmo control is not column-first, the comparator is disadvantaged by implementation, and data are already observed. The lowest-cost OpenZL profile per input in the audit is a retrospective oracle, not a deployed selector.

Command for the downloaded ZIP:

    N1_EXACT_ARTIFACT=/path/to/n1.zip python -m unittest discover -s research/codec/N1-exact-npy-audit -p 'test_*.py' -v
    python research/codec/N1-exact-npy-audit/audit_exact.py /path/to/n1.zip --output /tmp/n1-audit.json

The concurrent P2-E17 archive workflow was queued when work began, so no duplicate runner was launched. **Next:** independently audit completed P2-E17, then implement a native typed-NPY OpenZL comparator with the same original-byte and actual complete-cost scope before any archive promotion. No fresh holdout, trained selector, RL, merge or release.
