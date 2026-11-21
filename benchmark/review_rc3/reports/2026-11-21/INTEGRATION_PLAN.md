# INTEGRATION_PLAN — RC3 review, 2026-11-20

## Recommended candidate: C1 = 302e31a6a0c9be45f49201bfb1a45221b186bbf1

C1 is on local branch `review/rc3-integration-J62kQD` in `$V/integration-c1`. It has not been pushed or merged.

| step | commit | content |
|---|---|---|
| base | 5071f388 | rc3 (pushed) |
| + review fixes | 54fe7b3f, 2293c857, 16f17c84 … fb58d3d6 | CI YAML quoting (C1); 1.2.0-rc3 identity labels; review harness; dated errata; the fuzz_codecs fix |
| + W3 decmap/phrase | a17d6657, merged as 07647d3c | decoder-only, format-neutral |
| + W4 bwt | 5ce318ad, merged as 302e31a6 | byte-identical frames; lower decode peak RSS. The CHANGELOG conflict was resolved by keeping both Unreleased sections. |

## Excluded or deferred

| item | why | what would unblock it |
|---|---|---|
| W4 entropy (5f24d6ae) | reuses codec 27, changes 29; gain +0.02–0.11% | new codec id (or khuf version byte); legacy decoder or explicit no-support statement for rc3 opt-in frames |
| W4 lz (3a027805) | reuses codec 28; speed+native is −1.31% (larger) | new codec id; fix the speed-route size regression |
| W4 cm (ca81739e) | new -x generation; "model 4" collides with W3 prior (ae00b26f) | owner picks the id scheme, renumbers, reruns the compat matrix → KEEP_OPT_IN. The gain is large: 2× speed and 13× lower peak memory vs rc3 -x, for 0.28% larger output. |
| W4 docs (fba28558) | 2 threads at -T1 decode | make the async writer obey -T1 |
| W4 multi (fdb99760) | ABI change to `kolmo_params`; governor research | separate multithread scoreboard and ABI versioning |
| GPU=1 | unavailable in rc3 by design (C7) | separate research scoreboard |

## Compatibility decision

- **C1 adds no format change:** it decodes everything rc3 decodes with identical outcomes, and it writes rc3's bytes.
- No migration tool is needed.
- Finding 6 (KOLMO_NATIVE bumps the container to v11 even when no native codec is selected) is pre-existing in rc3. Fix it before the release so plain frames stay readable by 1.1.0/rc2.

## Regressions and trade-offs (C1 = rc3 for these, from holdout and screening)

| preset | trade-off | detail |
|---|---|---|
| eco | slow and memory-hungry vs zstd-9 | 0.36× encode, 0.66× decode; up to 1.08 GiB encode peak |
| balanced | slower and heavier than zstd-3 | 0.72× encode; 137 MiB vs 7 MiB peak |
| speed | no size gain over lz4-1 | a size tie with lz4-1; 44.6% larger than zstd-1 |
| ultra -x | high memory | about 3.0 GiB peak |

## Release and version proposal

- Keep `1.2.0-rc3` labels (2293c857) for C1 as a release candidate. Do not tag or publish until the gates below are green.
- A later rc4 could carry W4 cm under a new id.

## Evidence gates before merge to main or release

| gate | status |
|---|---|
| correctness: Phase E, sanitizers, native matrix, determinism on C1 | PASS (arm64 macOS) |
| format compatibility on C1 | PASS (0 misdecodes, 0 crashes) |
| CI on the final code (C1) | **PENDING**: needs the integration branch pushed (owner approval). CI is green on fb58d3d6 only. |
| x86-64 execution (AVX2 decmap, Intel/Linux) | **PENDING**: CI on C1 covers ubuntu-latest |
| Mac evidence for all shipped presets vs general rivals | PASS (screening plus holdout, strict T1) |
| claimed competitive domains (FLAC, JPEG XL, FFV1, CRAM, Pcodec/OpenZL) | **PENDING**: tools unavailable; claims marked NEEDS_RETEST |
| `planner.c:158` -Wnonnull memmem warning | **OPEN** (review) |
| KOLMO_NATIVE needless v11 bump | **OPEN** (fix recommended) |
| preservation verified | PASS (PRESERVATION_RECEIPT) |

**Recommendation:** C1 is the right integration content. It is **not yet release-ready**: final-code CI, x86 execution and domain-specialist verification are still pending.

---

# Addendum, 2026-11-21 — recommended candidate is now C2 = 25bedda80b7320268e3c35963a3e5f8b9be2cecb
Branch `review/c2-integration-J62kQD` (pushed, CI green; not merged, no PR, no tag).

| step | commit | content |
|---|---|---|
| base | 302e31a6 | C1 (rc3 + review fixes + decmap + W4 bwt) |
| + | 91ae8cee | KOLMO_NATIVE: container v11 only when a native id is written (finding 6 fixed) |
| + | b902e04a | W4 docs encoder tuning (balanced 8 MiB window, eco text→BWT, hc text level 19 / 128 MiB window, table prefetch); the async decode writer is excluded |
| + | 69f391f8 | brotli encoder: large buffers mmapped (eco encode peak −78%), bytes identical |
| + | a555c05d | W4 cm for -x (CM model nibble 9, explicit reserved-nibble refusals, SPEC.md) |

| gate | C2 status |
|---|---|
| Phase E (17 steps, portable + C-reference + sanitizers + bindings) | PASS |
| compat matrix (14,580 decodes) | PASS: 0 misdecodes or crashes; reads everything C1 reads |
| CI on final code (x86-64, arm64 Linux, macOS, sanitizers, fuzz) | PASS (run 44818901610) |
| timed holdout vs C1 | eco −4.39%, hc −1.04%, balanced −0.14%; speed/ratio identical. Trade-off: eco decode 0.72× (A6). |
| domain specialists | measured (PERFORMANCE_REPORT addendum). Image ratio is 20.5% behind JPEG XL; work in progress. |
| `-x` timed on C2 | PASS (`report-ultra-c2`: 1.98× enc, 1.87× dec, peak 226 vs 3,046 MiB, +0.28% size vs C1; 7.6% smaller than zpaq) |
| owner decisions | A6 (eco text rule), A7 (native KLZ as the speed default), W4 cm table −2.7% (cm section of ALTERNATIVES) |

Version proposal: C2 changes default output bytes (docs tuning) and adds a new -x generation, so call it **1.2.0-rc4**, not rc3. Not applied; it is the owner's call.
