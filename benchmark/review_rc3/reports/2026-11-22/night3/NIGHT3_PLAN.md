# Night 3 plan — 2026-11-22 night (+03), Apple M5

Owner instructions (before bed, about 23:30, 2026-11-21):
- Remove every third-party codec library (zstd, Brotli, LZ4, liblzma, xxHash) and use only Kolmo's own codecs.
- Do not read old files (owner's choice). eco uses KLZH2 level 7.
- Label the result **0.0.1-rc1**.
- Open a **draft PR** once every check passes.
- Keep working all night and follow the plan.

Standing rules:
- Never delete outside the working branches.
- One timed workload at a time (lock `locks/timing`), strict T1, byte-verified decodes.
- No merge to main, no tag, no publish or yank. Push new branches only (`github` remote; `origin` is the preservation bundle).
- Check in every 30 minutes; pause expensive work above 90% of the 5-hour window.

## Track A — library-free build (`review/rc4-native-J62kQD`)
1. Removal and rewiring (subagent, running), decisions D12–D15:
   - container version 12 and `KOLMO_ERR_LEGACY`;
   - khash64 replaces xxHash;
   - dictionaries and profiles become `KOLMO_ERR_UNSUPPORTED`;
   - native size probes.
2. Verify it myself:
   - build and tests;
   - `nm` and grep proof that no library symbols or headers remain;
   - round trips on all presets;
   - spot-check the legacy error.
3. Label 0.0.1-rc1 (VERSION, pyproject `0.0.1rc1`, Cargo `0.0.1-rc.1`, CHANGELOG).
4. Phase E (17 steps).
5. Timed holdout vs rc4 and the rivals (lz4-1, zstd-3, zstd-9, zstd-19, xz-9).
6. Push, CI, draft PR.

## Track B — close the gaps the native build opens (encoder-side tuning, each measured)
In order of expected cost to users:
1. **eco: KLZH2 level 7 vs Brotli** (about +8%). Options:
   - KLZH2 level 8 or 9 for eco (slower encode);
   - a literal context model;
   - KLZMA for text blocks only.
2. **hc: +0.55%**, mostly nci and mozilla. Options: entropy-stage detail and larger windows.
3. **balanced: about +0.7%.** Options: the KLZH2 low-level parse and retuning the planner thresholds now that the probes are native.
4. **Planner tunables** measured with zstd probes: retune against KLZH2 probes.

## Track C — reporting
- `NIGHT3_LOG.md`, every 30 minutes.
- `NIGHT3_SUMMARY.md` in the morning: decisions with their alternatives, the benchmark table, open items.
