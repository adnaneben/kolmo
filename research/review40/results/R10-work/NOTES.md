# R10 working notes (for stage 2; written at the end of stage 1, 23 Nov 2026)

Claims: #32 xp-store-trial-skip (execute, primary), #51 xp-plan-memo (execute); planned #52 plan-dead-trials,
#53 xp-target-calibration, #54 xp-content-sketch-key, #58 xp-param-learn.

Working clone: /home/user/kolmo-review40-wt/R10 (git clone of the base, branch review40/R10-xp). It holds ONLY the
research instrumentation so far (uncommitted): `KOLMO_XPSTAT` prints in src/codecs/codec.c (codec_compress) and
src/core/blocks.c (before `uint8_t *e = J->idx ...`). Binaries: build/kolmo-xpstat (rc3 + prints),
build/kolmo-xpstat-norace (same with -DCM_RACE_MAX=0). The vendored objects were rebuilt with the same flags.
Rebuild only kolmo.o: `touch src/kolmo.c && nice make build/kolmo CFLAGS='-O3 -march=native -fno-plt ...'` (about 1 min).
For the shipped patch, start from a clean tree (git stash or a second clone) so the prints are not in it.

Hook sites found (line numbers of 5071f388 src/core/blocks.c):
- 392-401  ratio, block > 8 MiB: slice gate BWT vs LZMA2-1 (slice_verdict), then `cl = codec_compress(...)` at 402
- 408-424  ratio: BWT attempt after LZMA2 (35% rule, second slice gate, keep at <= 97%)
- 425-445  ratio: plain-bytes retry after a transform (BWT slice, full BWT, LZMA2 up to 2 MiB)
- 446-...  -x: CM vs LZMA2 / BWT / plain alternatives (xtrial_take), image blocks: graph codings then codec 17 (around 268-280)
- cm.c 873 cm_compress_raced: 4 presets on the first 256 KiB (whole stream when <= 256 KiB), none above 16 MiB
- plan/parallel.c plan_worker: `else if (!j->prof || !kp_plan_pinned(...)) pl = plan_for(...)`, then ultra_keep_transform
- plan/profile.c: kp_key, kp_band, kp_fields_fit, kp_pinnable, kp_pin_from_plan (reuse for the plan memo)
- compress.c: snapshot before run_pool(plan_worker...), flush in cleanup; J.* carries the snapshot to workers

Facts measured in stage 1 (see reviews/R10.md): ratio discarded coder time 25.7% (Silesia), -x 17.6% (8 files; osdb
97.6%), images at -x 25.6%, balanced planning 27.5% (14 files), -x text planner CM passes 9.7-13.7% (5-10 MB) and
about 40% at 1 MiB, race about 28% at 1 MiB for 0.45% (dickens.1m), lc/lp/pb 0.27-0.40%.
A dropped word plan leaves plan byte 8 (tflag 0) in the frame: the only byte that differs from `-n` on dickens.1m.

Derived test inputs: /home/user/kolmo-review40-data/R10 (README.txt, SHA256SUMS-chunks.txt).
Measurement scripts here: run_tax.sh, tax_parse.py, members.py, run_lclppb.sh, run_race.sh.
