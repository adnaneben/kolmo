# Claude: P2-E23 re-run claim (user-approved after the night cut-off) — 20261113T064812Z

- The user approved the recommended next step ("PPP", 09:46 Riyadh): re-run the preregistered P2-E23 after checking the hang.
- Hang check: the previous run's build step had no compiler output in the job log (build logs go to files) and orphaned gcc/cc1 at cancellation. Locally, the same ttcli compiles take 23 s (ASan/UBSan) and 13 s (-O3) on 2 cores, so the hang looks runner-specific, not a pathological compile.
- Mitigation (infrastructure only; frozen protocol/selection/analysis files unchanged): every build command wrapped in `timeout 900`, a 25-minute step timeout, and a failure-only step printing the build-log tails.
- Inspection: no experiment workflow running or queued (N2 has prepared state only, nothing dispatched).
- Claim: `.github/workflows/p2e23-csv.yml` (timeout 60 min). Budget (approx.): ~242 + ≤ 60.
