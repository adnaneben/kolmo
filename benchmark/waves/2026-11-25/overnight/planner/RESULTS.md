# Planner ownership and indexed-query review, 25 November 2026

## Existing planner repair

[PR #6](https://github.com/adnaneben/kolmo/pull/6) merged on 13 November as b8ffcd17f34b6ea9fafa7ac2e266344f132a2664. Its text retains an earlier draft instruction; Git ancestry and the live PR merged flag establish the later disposition. The historical baseline was 3c6890a91314b4d71db1911043ec19b87c16a86d. The repair head a64f57fdeb742f4bf57d44c8cfc994ebf48ae121 contains the reviewed core edit 83425e1 and repaired test instrumentation. The original test had failed to compile and observe the planner path. The subsequent commit repaired the test, which ran in CI.

Independent static replay of the three approved edits produced the exact committed repaired blob, from db8b7ad665ab0ff61f6634a2d6cb76ed1b17b70a to e06467c8872d494b97270811bedf6fd0bac2aa31. The graph and targets files matched. Baseline, repair, test and merge commits are ancestors of pinned main a1d39559686a9ec17c8614b9154a64d7b5b98672. The canonical, Python and Rust planner sources have identical Git blob e4a513ba2d8b10f9f2c74a7bc76a780f0f95ae6e.

The [original hosted run](https://github.com/adnaneben/kolmo/actions/runs/43146126876) passed all three jobs against the exact repair SHA. Downloaded all three official artifacts on 25 November (they expire 26 November), independently matched each complete ZIP to GitHub's artifact digest and all 20 contained command logs to the status records. For each of 45 pairs in the historical nine-input, five-profile matrix, compared the two stored frame hashes and sizes, then compared all 90 archived decoded payloads byte-for-byte against the hashed source. Historical candidate full-suite sanitation passed. Its development-era frame format is distinct from current v12. See hosted-evidence.json, the three *-status.json files and historical-frame-pairs.json. The GitHub archives are preserved locally as separate ZIPs.

Fresh current-main tests on macOS Apple M5 with Apple clang 21 passed. The ASan/UBSan planner suite exercised 72 cases, including 48 record allocations and 72 graph cases, with zero leaked cases or outstanding allocations. The ASan/UBSan v12 corpus suite passed 357 frozen frames via direct decode, frame validation and range reads. These are scoped checks. Current test logs are main-ownership.log and main-v12-sanitizer.log.

## Claude-owned planner follow-up

GCC 16.2 at -O1 -Wuse-after-free=3 reports a lifetime diagnostic at src/plan/planner.c:727 for inspecting pointer equality after releasing the associated allocation. Evaluate and clear the matching alias before release; make the same change in both vendored mirrors, then repeat the planner, mirror and compiler gates. The clean regression tests above do not rule out the portability issue. Source in this lane is unchanged.

## Independent indexed-query correction

Current open PR search for ix_block, IDX1 and index.c returned no matching PR. Checked the patches for #59, #65, #72, #79, #93, #94, #95, #97 and #98. None touches src/core/index.c. The historical author at 4ed33288 was Claude; the current recorded planner assignment and DDP1 archive work cover separate modules.

GCC also warned at src/core/index.c:498 about a post-release newline-pointer test in the cross-block IDX1 query path. The main-based branch codex/ppp-idx1-newline-lifetime saves the boolean before releasing the temporary buffer; tests can inspect the result without inspecting an expired pointer. Canonical, Python and Rust copies match at b395cd96619f46dfa4bf30a6053dc5305a9a8760. Scope is the same change in three files. The planner code stays with Claude.

GCC 16.2 -O1 no longer warns for the index, and compiled the baseline and candidate core objects byte-identically. Apple clang 21 -O3 produced byte-identical full CLI binaries; the SHA-256 of both is b67bbddb88d78d26313ae9ec11718e4cfb2e546f2aa0fd75faa36ee95ad9106f. This source portability correction has no machine-code, compression size or throughput change for that release build. See index-object-parity.txt, gcc-static.log, index-gcc.log and index-features-check.log.

Index commit 84641dfb66bcf919a85be1edd9bede0809840694 passed make check-features-b, including 3,712 optimized assertions, 3,742 ASan/UBSan assertions and CLI feature checks. Both codegen parity comparisons used pinned-main source before the fix. The three official hosted ZIP archives have been downloaded and digest checked; only the small metadata and per-frame digests are included in the compact receipt bundle.
