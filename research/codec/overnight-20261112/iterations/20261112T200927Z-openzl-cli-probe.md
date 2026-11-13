# 20261112T200927Z — N1 independent OpenZL reference-preparation

Status: **PASSED_LOCAL_MOCK_TESTS / BLOCKED_REAL_COMPETITOR_MEASUREMENT**.

- Starting authorized branch HEAD: `20f550cdc844e2c3199105a2eeb686f60e805f56`, preserving Claude's P2-E1 implementation `b4a47a5` and follow-up receipt `20f550c`. PR #7 remained draft, unmerged. No other branch touched.
- Prior policy source checkpoint workflow `43286007814` completed successfully and published a Git bundle; this is not a comparator test.
- At inspection 20:03 UTC, PR workflows `43295838500` (wheels), `43295838654` (CI), `43295838388` (isolated validation), and push CI `43295827009` were queued at `b4a47a5`; existing earlier CI was queued. No new overnight experiment workflow launched and none cancelled.
- N1 task: verify the strong OpenZL v0.3.0 tag SHA and prepare a bounded full-file CLI comparator with hard exactness/version/binary-hash/profile/level controls, including level-7 numeric Compression Transformer option. The 0.3.0 tag resolves to `32246b48faee46807f84183dac4db479089f5445`. Release: https://github.com/meta/openzl/releases/tag/v0.3.0 .
- Implemented `n1-openzl/openzl_cli_probe.py`, regression tests and `PROTOCOL.md` on this authorized branch. No production C sources changed.
- Executed locally: `cd /mnt/data/kolmo_n1 && python3 -m unittest discover -s . -p 'test_*.py' -v` — **10/10 passing**, including 3 exact mock round-trips, frozen binary/input pins, rejection gates, bound checks. The mock zli uses Python/zlib, NOT OpenZL. Python 3.13.5.
- Real `zli` executable is absent in this execution environment and the container cannot resolve github.com to clone/compile it. Therefore OpenZL measurements = **NOT RUN**, archive threshold = **BLOCKED**. Actual OpenZL build identity and memory/thread budget remain unverified.
- The CLI record is explicitly not in-memory comparable to Claude P2-E1; no competitor cost or compression win is claimed.
- Runner-minutes used in this session: **0**. Overnight budget remaining: **480/480** runner-minutes.
- Next prerequisite: matched original-byte competitor and candidate on the same in-memory or CLI boundary, with profiles chosen before results; avoid running redundant jobs while current CI is queued.
