# Night 2 plan — 2026-11-21, 15:15 → 23:15 (+03), Apple M5

Owner instructions (2026-11-21, ~15:00):
- Keep tuning the native codecs for 8 hours.
- For every choice, go with my recommendation and report it at the end.
- Keep the owner updated.
- Say when it is the right time to merge to main and push.
- Plan around renumbering rc4 to 0.0.1 at release.
- Write a plan forward for image, video, audio and the other weak areas.

Standing rules:
- Never delete or overwrite; move aside instead.
- One timed workload at a time, strict T1, byte-verified decodes.
- No merge to main (the owner reviews first). No tag, no publish, no yank.
- Push new branches only.
- Pause expensive work when 5-hour usage is above 90%.
- Check in every 30 minutes (usage, NIGHT2_LOG.md, next step).

## Track 0 — finish rc4 (`review/rc4-J62kQD`, 5a3ebe2c)
1. Phase E 17/17 (running).
2. `tools/rc4_queue.sh`: compat matrix with rc4 plus an image input; kimg compatibility; native version check; byte identity vs C2 / C3r3 / ecoalt.
3. GitHub CI run 44899989986.
4. Timed holdout rc4 vs C2, plus image domain rc4 vs C2 and cjxl, so the PR carries rc4's own numbers.
5. Open the PR rc4 → main. **Do not merge.**

## Track 1 — native codec tuning (`review/native-tune-J62kQD`, from rc4)
Measured gaps (benchmark/rc3w2/native/RESULTS.md, component bench, T1):

| role | native vs library | gap |
|---|---|---|
| speed | klz1 vs lz4 | −6.9% size, but encode 0.79×, decode 0.77× |
| balanced | klzh2/3 vs zstd-3 | +2.4–4.0% larger; struct +3–10% |
| hc / eco (strong) | klzma6 vs zstd-19 | −7.4% size, decode 0.11× |
| ratio | klzma8 vs lzma 9e | +0.08% size, decode 0.76× |
| huf | khuf vs HUF | +0.16% size, decode 0.97× (T1) |

Order of work (format-neutral first: decoder speedups keep every existing frame readable and are mergeable without a compat question):
- **N1. KLZMA decoder speed.** Range-decoder and literal-loop fast paths, matching liblzma first (target ≥ 1.0× of liblzma decode). This lifts hc+strong, eco strong and ratio-native decode.
- **N2. KLZ decoder and encoder speed.** Wild copies, token fast path, match-finder hash quality (target: decode ≥ lz4, encode ≥ 0.9× lz4 at the same −6.9% size).
- **N3. KLZH balanced parse.** Encoder only, same format: minimum match chosen per block, cheaper lazy parse at levels 2–3, repeat-offset checks (target: ≤ zstd-3 size at ≥ 0.9× its speed).
- **N4. khuf decode.** Wider tables (decoder only).
- **N5, if time allows.** Planner awareness of the native coders; today the planner's thresholds are tuned for zstd.

Measurement:
- `natbench`, one thread, component level, on the silesia-like and structured sets in `$V/corpus`.
- Then end to end with `audit.py` on the holdout for anything kept.
- Every kept change: `make check` plus the determinism check, with decoders run against old frames.

## Track 2 — the forward plan (written last)
Image, video, audio, FASTQ, the general presets, and the renumbering to 0.0.1, as `evidence/PLAN_FORWARD.md`.
