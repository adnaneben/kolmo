# P2-E23: NOT RUN (build step hung; cancelled by its owner; no inputs were read)

Actions **43358797920** at `4c78ea9` started at 03:25 UTC. The step "Build pinned rivals, Kolmo and ttcli" never
finished: after 47 minutes it was still in `gcc`/`cc1`, where the identical step took about 5 minutes in P2-E21 and
P2-E22. The cause is unknown. The log has no compiler error; `cancelled-run-log-tail.txt` holds the end of the
step.

This session cancelled its own run at 04:12 UTC to stop it wasting runner minutes; nobody else's job was touched.
It used about 47 runner-minutes and produced no result.

- **The input selection step never ran.** No new TidyTuesday file was read, so the preregistration (`PROTOCOL.md`,
  `prepare_e23.py`, `analyze_e23.py`) remains valid for a future run.
- It was **not relaunched**, because of the overnight plan's 04:00 UTC (07:00 Riyadh) cut-off for new jobs.
- The claim that `tt_zstd12` beats OpenZL `csv`-9 within cost stays **unvalidated**. P2-E22 gave 17/23 there, but as
  a secondary result.
