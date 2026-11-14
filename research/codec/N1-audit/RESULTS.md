# N1 independent complete-CLI evidence audit (development only)

**Scope**: read-only offline audit of GitHub Actions run [43299261465](https://github.com/adnaneben/kolmo/actions/runs/43299261465), artifact 13980664053. No additional benchmark was run. Do not combine with P2-E1/P2-E2/P2-E4 in-memory measurements or interpret a completed workflow as P2 codec acceptance.

## Verified artifact

- Original GitHub artifact ZIP SHA-256: \`7f14230393e38e8ed790db70f0de4f6ad30b7e09ca708418562d0dfda440e357\`.
- A local independent audit streamed and verified **all 1,229 manifest-listed files**, with no missing/unlisted members, against the manifest and ZIP digest.
- Two recorded timing passes contained 80 identical input/configuration keys; **64 configurations completed** with exact matching retained-frame hashes and sizes across passes, **16 failed**.
- Complete measured configurations contributed **384 individually cost-recalculated timing samples** (three encode/decode pairs per pass). The stored-byte, speed-cost and archive-cost calculations used the existing P2 parameters.
- Tool SHA-256: zstd \`3f76153c8e8b10e70349edac4dd384ce49dff23c0d5d56aabe9a48bd71a0079c\`; OpenZL \`932b011b776c9241a0bef141dfeb6f4de1162c72903f5a46040ad53e3d209712\`; Kolmo \`7c9996e2d73a39ef1af4b749db78fcad8784092ccd68d547fa8d6d563c08370d\`.
- Competitors: zstd 1.5.7 at level 3, OpenZL v0.3.0 tag commit \`32246b48faee46807f84183dac4db479089f5445\` using serial 6/9 and CSV 6/9. Original R3 inputs plus two derived table views are **observed development data**, not untouched validation.

## Important comparator coverage defect (not a codec failure)

All **16 failures** were \`openzl_numpy_6\` / \`openzl_numpy_9\` on the eight NPY inputs. Their retained CLI logs say: **\`Profile not found: 'numpy'\`**. The pinned OpenZL \`zli list-profiles\` lists \`le-i16\`, \`le-i32\`, \`le-i64\`, corresponding unsigned variants, \`csv\`, and \`serial\`, but **not \`numpy\`**. Do not treat these 16 as measured compression failures or silently substitute the generic \`serial\` profile for a competitive typed numeric profile.

For the NPY boundary, a future applicable OpenZL typed comparator must parse and retain the original NPY header *byte for byte*, select the correct endianness/width/sign from the header, compress only the verified raw numeric payload with the matching OpenZL numeric profile, and reconstruct the exact original header and payload. All parsing/validation, wrappers, stored metadata, training (if any), process and decode costs must be charged; declare this as an **external exact typed adapter**, not native OpenZL NPY support. Keep generic \`serial\` as a separately labelled control.

## Whole-file control diagnostics (not P2 acceptance)

Using the median over six complete-CLI samples per input, the unchanged \`kolmo_fast_quick\` control was smaller than zstd-3 in **8/10** inputs, but met the 1-Gbit/s round-trip cost in **1/10**. The one file meeting both was the derived Covertype CSV table: **72.6% fewer bytes**, **10.3% lower total round-trip cost**. The CollegeMsg CSV table was **38.2% smaller** but **2.9% higher total cost**. These are *observed-development controls*, **not** validated column-first P2 proposals.

On the two correlated CSV views, the pinned OpenZL CSV level 9 comparison did execute. Against that typed comparator, unchanged Kolmo \`fast_quick\` showed:

| Derived view | Bytes relative to OpenZL CSV-9 | Archive modeled cost relative to OpenZL CSV-9 |
|---|---:|---:|
| Covertype CSV | approximately **31.7% fewer** | approximately **44.4% lower** |
| CollegeMsg CSV | approximately **27.8% fewer** | approximately **35.7% lower** |

The existing Kolmo controls are not shown to satisfy the mandated type-before-planning rule, and this is not source-disjoint validation. Neither row is a P2 acceptance or release decision.

## Why this matters

The successful N1 job unblocks credible **zstd-3** full-CLI controls and OpenZL **CSV** controls, not the eight intended typed-NPY archive comparisons. The absent \`numpy\` profile is the concrete N1 gap to repair. It would be misleading to count the serial-9 profile as the strongest applicable archive rival without an exact numeric comparator.

The latest P2-E3 whole-CLI research workflow was inspected as queued/running during this independent audit. **No new experiment workflow was launched**, no other collaborator's work was cancelled, no production source/format/default or historical verdict was altered, and RL was not trained.

## Reproduction

\`\`\`sh
python -m unittest discover -s research/codec/N1-audit -p 'test_*.py' -v
python research/codec/N1-audit/audit.py N1_ARTIFACT.zip --output audit.json
\`\`\`

The audit script accepts the original ZIP, verifies its full SHA-256 manifest and exact frame identities, and recalculates every reported cost. The artifact expires on 12 December 2026; its hash and compact findings are Git-tracked here. Local development checked an equivalent independent audit against that actual artifact, and 14 synthetic infrastructure tests passed; the committed minimized variant needs its own exact-source CI verification before its test suite can be claimed as passed.

**Next substantive task:** implement and correctness-test the external exact NPY-to-OpenZL numeric-profile adapter without modifying production wire IDs. Execute matched full-CLI controls only after the current P2-E3 run completes, and only after checking the shared runner budget and source provenance. Freeze a new preregistration before fresh source-disjoint validation.
