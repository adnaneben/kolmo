# NIGHT_PLAN — overnight run 2026-11-20/21 (owner asleep; approvals given 2026-11-20 ~20:25Z)

Approvals given in chat on 2026-11-20:
- Install missing rival tools from official sources into a **run-local prefix** (`$V/rivals/`). Global Homebrew is not touched.
- New codec ids and format versions are allowed **on new branches only**, with compat-matrix proof. No merge to main, no release, no tag.
- Push new work branches to GitHub for CI. No PR merges.
- Proceed without asking. Alternatives go to `ALTERNATIVES_FOR_REVIEW.md`; progress goes to `NIGHT_LOG.md`.

Standing rules (from the runbook):
- Preserve everything. Fresh output directories only.
- One timed workload at a time, strict T1, byte-verified decodes.
- No builds or other heavy work while a timed suite runs.

## Steps
1. Record CI on C1 (runs 44810049864 and 44810046960) in TEST_RESULTS and INTEGRATION_PLAN.
2. Build the rivals into `$V/rivals/prefix`: flac, ffmpeg (FFV1), libjxl (cjxl/djxl), htslib+samtools (CRAM), pcodec CLI. Record versions and hashes in TOOL_INVENTORY.
3. Branch C2 = C1 + fixes, from 302e31a6:
   - a. KOLMO_NATIVE: do not bump the container to v11 unless a native codec is actually written (finding 6).
   - b. `planner.c:158` memmem -Wnonnull.
   - c. W4 cm under a renumbered, collision-free -x CM id (opt-in, -x only).
   - d. W4 docs async writer obeys -T1 (then it may join the T1 board).
   - e. If time allows, W4 lz / W4 entropy under new codec ids, opt-in only, plus a fix for the speed+native size regression.
4. Validate C2: Phase E, native matrix, determinism, compat matrix (C2 vs every older generation), fuzz smoke.
5. Benchmark C2:
   - targeted screening of the changed paths vs C1;
   - domain suites vs the specialists (audio vs FLAC, images vs JPEG XL, video vs FFV1, FASTQ vs CRAM, arrays vs Pcodec);
   - final holdout of C2 vs C1 vs rivals.
6. Morning report: PERFORMANCE_REPORT addendum, FEATURE_DECISIONS update, ALTERNATIVES_FOR_REVIEW, and the pushed branches with CI.
