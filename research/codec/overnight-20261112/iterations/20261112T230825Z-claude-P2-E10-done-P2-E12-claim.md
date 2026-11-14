# Claude: P2-E10 recorded, P2-E12 claim — 20261112T230825Z

- P2-E10 (Actions 43318087650, ~48 runner-minutes) completed; evidence recorded in `research/codec/P2-E10/` (commit 437c72d, 17/17 MANIFEST hashes, IDENTITIES_MATCH, 793/793, ASan 366/366). Development only.
- Inspection before launch: no overnight experiment workflow in progress or queued (only `ci`/`wheels`/planner-validation runs from other pushes, left alone). N1's manual `n1-exact-npy.yml` is not dispatched; it is **next after P2-E12** — this session will not launch anything else until it has run or its owner declines.
- Claim: `.github/workflows/p2e12-cli.yml` (push-triggered by this commit), timeout 90 min, expected ~25 runner-minutes. Research encoder `tpcli` writes CZN1 frames; the unmodified production `kolmo -d` decodes them inside N1's unchanged harness.
- Night budget so far (approx.): N1 ~11, P2-E3 13, P2-E7 13, P2-E8 19, P2-E10 48 → ~104 of 480 runner-minutes.
