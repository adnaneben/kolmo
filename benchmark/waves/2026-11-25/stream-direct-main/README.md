# Bounded stream index backport

The standalone candidate starts at main 4db0425b361012523d8074ca6f2a760d9466e9f4. Source-only assembly contains the native API, decode and stream files from #72 (fbce2ade149072003e09c93baf3639b7ec3ea3aa) plus #79 (e1397b0169e659a3ef5dbd5f3019221830c8d411), mirrored to both vendored copies. The relevant #58 files match pinned main, and the #72 nested source files match the corresponding #73 base files. Replacing these files picks up no archive, CM, GPU or policy changes from either wave. The only additions beyond those PRs are header clarification and an automated check in the default Makefile.

The new check generates four valid dense streams from 2,046, 2,047, 4,095 and 4,096 blocks, crossing the regular-file and in-memory metadata thresholds. It exercises a CZN1 member plus a CZNS member in sequence, exact and one-byte-less admission, stream decode with one and four threads, positional file output and range reads. A KLZH2 frame also tests the valid optional declaration-free representation and checks round trips under the codec's admitted budget with one and four threads. The additional 357 pinned frames passed the repository's native conformance suite.

#79 cannot land independently of #72's admission and accounting APIs. Measured as a follow-up to #73 alone, #79 restores throughput that #72's earlier tight-cap implementation had lost. A clean assembly of #72 and #79 compared with main yields a much smaller improvement and uses the new accounting semantics. Publish the direct-main comparison for promotion and retain the Wave 2 comparison to explain the algorithmic cause.

The compressed input/member allocation, large retained worker buffers, some codec scratch and nested video buffers remain distinct from the per-frame governor. An actual process RSS guarantee requires system enforcement and further work. The candidate decodes streams in O(B+W) index work for B blocks and W windows before any optional scheduling sorts. Main scans the index for each window, O(BW). No frame-byte change is required.

Portability review covered 64-bit bounds before size_t conversion, zero-length span edges, pointer resets after reallocation and worker lifetime through join. Native testing here covered Apple M5 arm64 only. The pinned passing suites do not constitute native x86/Linux or 32-bit validation of the assembly.

## Direct-main benchmark

Two 32 MiB prefixes, 4 KiB blocks, warmed I/O on the shared Apple M5. Seven shuffled paired trials after warmup per cell. Every timed stream decoded to the source SHA-256. Wall and CPU use process launch through write completion. The paired median can differ from a ratio of separate median times.

| Source | Cap | Threads | Main median wall ms | Backport median wall ms | Paired wall delta | Paired CPU delta | Faster pairs |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| silesia-prefix | 2M | 1 | 18.659 | 17.687 | -4.58% | -4.08% | 6/7 |
| silesia-prefix | 2M | 4 | 18.244 | 17.733 | -3.47% | -3.07% | 6/7 |
| silesia-prefix | max | 1 | 22.691 | 23.170 | +0.76% | -0.79% | 3/7 |
| silesia-prefix | max | 4 | 17.775 | 18.170 | +2.74% | +2.11% | 2/7 |
| enwik8-prefix | 2M | 1 | 21.890 | 21.295 | -2.36% | -2.10% | 4/7 |
| enwik8-prefix | 2M | 4 | 21.203 | 20.298 | -6.06% | -4.11% | 7/7 |
| enwik8-prefix | max | 1 | 25.759 | 25.008 | -2.84% | -2.46% | 5/7 |
| enwik8-prefix | max | 4 | 19.054 | 19.162 | +0.88% | -1.59% | 3/7 |

## Unrestricted-memory cross-size retest

Fifteen shuffled A/A/B triplets plus one warmup triplet per cell. A and A2 both invoke pinned main. B invokes the backport. All 576 decodes matched the raw source, in addition to the 128 arms above. Each entry below is the median of its corresponding within-triplet ratios. The final column counts trials in which B took more wall time than the mean of the two controls.

| MiB | Corpus | Threads | A2/A wall | B/A wall | B / mean(A,A2) wall | B / mean(A,A2) CPU | Wall slower |
|---:|---|---:|---:|---:|---:|---:|---:|
| 4 | silesia-prefix | 1 | +2.27% | -0.52% | -0.69% | +12.57% | 7/15 |
| 4 | silesia-prefix | 4 | +2.64% | +6.24% | +1.26% | +0.85% | 9/15 |
| 4 | enwik8-prefix | 1 | -3.85% | -0.29% | +2.85% | +2.86% | 9/15 |
| 4 | enwik8-prefix | 4 | +4.68% | +4.24% | +0.02% | +0.16% | 8/15 |
| 16 | silesia-prefix | 1 | +1.32% | -1.18% | +5.70% | +5.00% | 9/15 |
| 16 | silesia-prefix | 4 | -5.22% | -5.39% | -4.44% | -4.19% | 5/15 |
| 16 | enwik8-prefix | 1 | +18.09% | -3.00% | -8.13% | -8.20% | 4/15 |
| 16 | enwik8-prefix | 4 | +0.00% | +0.91% | +1.22% | -2.62% | 8/15 |
| 32 | silesia-prefix | 1 | -5.66% | -3.31% | -3.32% | -0.54% | 6/15 |
| 32 | silesia-prefix | 4 | -10.26% | -1.83% | -5.46% | +1.38% | 6/15 |
| 32 | enwik8-prefix | 1 | +1.76% | -0.06% | -0.24% | +0.40% | 6/15 |
| 32 | enwik8-prefix | 4 | -2.46% | -1.62% | +0.57% | -0.36% | 8/15 |

The original 32 MiB unrestricted wall warnings did not recur against the same primary control in the 15-triplet replay. Wall deltas relative to both controls ranged from −8.13% to +5.70% by cohort. No cell was slower than the mean A/A wall in more than nine of 15 repetitions. Silesia 4 MiB at T1 showed a CPU warning despite mixed wall time (10 of 15 CPU comparisons slower). Its direction changed by file, size and thread count. Preserve the full matrix and keep this as a review candidate. Broad default throughput and RSS claims require a quieter independent host.

Pinned executable SHA-256 values are main cf23250a9d1fd43f6fb0328f26803ebb4a47ba6332ce98a9053b70728c8abfdf and candidate 9982977120b36656c91fc00f3c1a1a47cac4c57f156c94b26496604e8be9b458. The decode timings do not claim an encode or ratio improvement. The original two archives were read by identical SHA in both arms. Inputs derive from the existing 32 MiB Silesia and enwik8 lane. The 4 and 16 MiB frames were encoded by pinned main with 4 KiB blocks, level 1, no analysis, one encoder thread and 64 MiB segments. The archived replay scripts retain the exact original host paths. Replace those paths when replaying on another host.
