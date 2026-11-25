# Atomic archive extraction, sibling staging

The #59 draft creates a private stage directory next to each member, writes one file inside it, closes the file, then replaces the destination with renameat. Retaining a private stage across adjacent siblings reduces metadata operations while preserving that same per-member publication point. A valid archive member may itself have the name of the private stage. The repair retires a matching stage and creates another under a distinct name. It walks each destination parent from the extraction descriptor for every member, checks the parent identity before using a retained stage, and closes stages on parent-path changes, extraction termination, and member errors. The cache uses at most two retained descriptors and one bounded parent path. A completed sibling remains published if a later member fails. Whole-tree transaction and crash durability are distinct features.

The initial implementation cached the first stage unconditionally. On the APFS host it shortened elapsed time on same-parent workloads, but regressed 15–17% on one-member-per-parent and alternating-parent fixtures at both one and four requested threads. The median rose in all four contexts, with 35 of 36 pairs slower. This implementation is rejected and preserved under ungated/. The revised candidate caches only after a second consecutive member under the same spelled parent path. Each first member follows the original one-shot staging sequence.

The source files and diffs target two explicit bases.

| Base | Exact source change |
| --- | --- |
| Isolated #59 | reuse-stage.patch against src/cli/features.c, pre-image SHA256 2a8338d19df4910fc907027e082cf0edefed2cfbf0ad4bab7e87feaff47f5eb0 |
| Integrated #90 | stack90/reuse-stage.patch against src/cli/features.c, pre-image SHA256 d81639d18698c560c52d3a7612fba8b51b269eb86adb8f79b6da0f09a512f742 |

Both build with the ordinary project targets. Neither patch changes the library ABI or bytes written to DDP1. The chart script restores an archive made once per input in all arms. All round trips check every filename, SHA-256, mode and modification time. The tree check also verifies directory membership. test_normal.py exercises seven normal extraction cases, including genuine member and parent names that coincide with the process's own staged directory name, existing file hard links, physical parent transitions, named extraction, ordinary directory replacement failure and successful adjacent siblings. Both branches also passed the standard CLI suite including #77's file-size-limit test.

The referenced host was an M5 Mac under APFS. Existing-main is a direct-write control with a different overwrite-failure contract. Compare the repair to #59 for like-for-like atomic behavior; compare to main only to quantify its remaining cost. The fixture order is shuffled and each restoration uses a new destination on a warm filesystem. Multi-worker settings are CLI requests. The metadata write loop is serial. These results do not establish native Linux filesystem timing, cold disk durability or an atomic commit of an entire archive.

Run the two matrices sequentially on the same quiet host, using separate source trees and the same compiler.

```sh
python3 test_normal.py ./build/kolmo
python3 flat.py --baseline "$MAIN" --original-atomic "$PR59" ./build/kolmo
python3 tree.py "$PR59" ./build/kolmo
python3 report.py
```

The numbers in gated-stage-summary.csv and tree-summary.csv use the median of matched elapsed-time ratios. Raw runs include warmups. Source patch and validation evidence remain draft until the actual PR head passes the repository's platform checks.

## Final measured result

The gated version yielded lower median paired extraction time than #59 on all eight flat-tree contexts spanning 64/256 files, 512 B/32 KiB members and T1/T4. Reductions were 29.5–51.3%; the candidate was faster in 82 of 88 scored pairs. For example, 256 × 32 KiB at T1 was 75.10 → 41.91 ms on this run. The ratio of paired elapsed times for gated vs direct-write main was still 1.297–1.747 across these contexts. Whole archive bytes and compression ratio are unchanged. This measures extraction only, so transfer the measured value into any combined graph using matched create plus extract times for the same workload.

A separate topology replay used 64 files of 1 KiB. Eight adjacent siblings per nested directory improved 36.4% (T1) and 15.8% (T4). The latter was faster in five of nine pairs. Initial alternating-directory T4 looked 7.2% slower, so we ran 15 randomized triplets per context with a same-binary control. Alternating T1/T4 then measured +1.30%/−2.79%, singleton T1/T4 −0.92%/−9.65%. Same-binary controls ranged −4.18% to +3.58%. The earlier 15–17% penalty failed to reproduce after gating. The direction for alternating T1 stayed slightly positive, with five of nine and nine of fifteen individual pairs slower. A small residual cannot be resolved by these noisy timings. Claim the gain for adjacent siblings only. The final measurements include 600 fresh extraction validations across these three matrices. Seven additional valid-archive scenarios on each patch base passed, including a third member hitting a regular file-size limit after the reusable stage was already active. Initial and final receipts are retained.

Draft source is suitable for independent branch review. A second OS/filesystem and the actual release checks remain outstanding. Shipping the new visibility semantics trades time for atomic replacement relative to current main.
