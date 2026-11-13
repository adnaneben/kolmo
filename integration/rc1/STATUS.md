# RC1 status — 13 November 2026 UTC

Authorized writes: integration/best-features-rc-20261113, draft PR #10. No main merge, tag, release, package publication or other-branch modification.

- Actual selective integration: ae3957ad80d134506ce37b59857af25fe8826062.
- Combined correctness CI PASS at b627c1171d821deca1484f023c7eeda6a1e5bde1: run 43472625541. Linux x86/ARM/macOS, full sanitizer and fuzz gates passed; wheels 43472625565 passed. Historical ownership-only workflow skipped, not passed.
- Rebuilt focused inverse/option checks passed; 53 archived default frames match exactly.
- Matched native pilot EXECUTED: 31 configurations, 35 observed inputs, 842 successful cases, 5052 timed batches, 1684 memory batches. 88 ultra input-cap exclusions and one OpenZL CSV allocation failure remain visible.
- Read perf/RESULTS.md, perf/SUMMARY.json and perf/PACKAGE.json. Readable benchmark driver, runner, preparation, build helper and audit tests are committed in perf/source/ and match the locally tested/staged files. Raw data and workbook are delivered separately with digests.
- General joint typed-column planner, trained OpenZL, full-scale corpus, M5, multithread/file-mapping performance, and release acceptance remain NOT_ESTABLISHED. KST1 layered framing is not silently promoted.

Next: matched pre-integration-vs-RC feature ablations on audio and CSV regressions, then freeze routing before fresh-source and target-hardware validation. Keep native leaf, CSV router and CM level 2 opt-in. No further exhaustive compression search is justified by this pilot.
