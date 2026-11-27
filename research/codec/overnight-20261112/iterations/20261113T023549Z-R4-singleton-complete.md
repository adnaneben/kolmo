# R4 interactive continuation — 13 November 2026, 02:35 UTC

Status: DEVELOPMENT_VERIFIED; targeted decoder optimization, not fresh validation or P2 competitor acceptance. Claimed in 68c7ddbbb5d41da6b819fc1510fc04c191fa9085. Source/report directory: research/codec/R4-singleton/. Exact verified directory Git tree: 88ebc3bf3d978a9bf69353421668186b2909d2db (17 files; 16 manifest entries plus manifest). The published tree equals the local tested source/report tree byte-for-byte.

## Executed work

- Staged original and research-only singleton-fast rANS decoders from pinned full core e06467c8872d494b97270811bedf6fd0bac2aa31 and graph 91ad067dc583b9d9e6f060f214a0f7c402bb4a31. Production source unchanged. The shortcut checks existing terminal states and exact coded length before repeated-byte output; it adds no wire identifier or encoder change.
- Built GCC original/candidate and Clang ASan/UBSan/leak candidate. Per build: 2663 valid leaf cases, 10748 truncations, 12805 leaf mutations, 107 complete-frame/range/limit tests, 1536 full-frame mutations, one direct allocation fault. Matching decision digest 4f59cfd2bd87566c. Vendor archive uninstrumented.
- Corrected one ineffective test-allocation hook (upstream undefines malloc); retained initial failure, reran all suites with identical direct hooks. No decoder condition or test case removed.
- Verified all 282 archived R3 evidence entries. Rewrapped all 8 unchanged R3 candidate payloads as ordinary CZN1/KG_INT frames; all match across builds and decode with the separate unchanged production CLI. Eight additional golden CLI decodes pass.
- Ran 504 decode-only timed batches on the 7 RANS inputs (7 + 5 rounds; warm and displaced-before-batch; original, candidate, duplicate-original). The HUF input is correctness-only. Raw compact timings and every summary are committed, not only held in an expiring artifact.
- Eleven Python tests pass. Independent verify_saved.py confirms all 504 timing rows, 224 saved summary values and all source/evidence hashes.

## Result and decision

CollegeMsg timestamps contain one constant 59,833-byte plane. Warm entropy-kernel duration is 0.798x / 0.777x the original, and complete verified CZN1 decode is 0.901x / 0.914x, with identical 70,850-byte frames. Keep as a conditional implementation layer, NOT an additional compression-ratio gain.

Seven-input pooled warm full decode is 1.015x / 1.006x: NO aggregate speedup established. Nonconstant controls and identical-baseline timing noise remain visible. Covertype's 2/18-symbol regressions are not fixed by this shortcut. No general throughput, memory reduction, M5, new-source, release or rival-cost pass claimed.

## Resource/preservation

New Actions runner-minutes: 0. No remote experiment launched, no existing job cancelled, no new automation. Local bounded C builds and replay only. Previous overnight budget entries remain authoritative; this iteration does not reset the shared budget. All writes remain on fix/planner-record-ownership-isolated-20261112; PR #7 stays draft/unmerged.

Publication is based on refreshed head 10de223d90e7f757eca2b3c0eb4e00776e7e761c, preserving seven concurrent commits after the R4 claim: P2-E20 fresh validation, independent E17 audit correction and Claude's wire review. P2-E20 reports an audio archive signal but failed CSV cost comparisons; its report and registered verdicts remain untouched. That report was read, not independently re-executed here.

## Next substantive task

Use the current production-frame candidates and already implemented header-exact OpenZL NPY comparator on a matched complete-pipeline cost boundary, with R4 only a separate decoder ablation. Do not transfer archive win counts from whole-file le-* comparisons to the exact-NPY comparator. Read P2-E20 and the wire review before proposing further fresh sources or an order-1 codec; the latter was downgraded by its own evidence. No default integration or another overnight schedule is authorized by this receipt.
