# FORMAT_COMPATIBILITY_MATRIX — RC3 review, 2026-11-19 (Apple M5, arm64)

Full-container results: every encoder generation's frames decoded by every decoder generation (`compat_matrix.py`,
evidence `compat-matrix-1/`). Cell = exact decodes / frames; R = the rest were rejected cleanly (`corrupt or unsupported
input`, exit 1). **0 SILENT_MISDECODE, 0 CRASH, 0 TIMEOUT in 7,344 decodes.** Inputs: 9 synthetic (text, CSV, log,
int32, float64, PCM16 WAV, Y4M, random, 1 byte) + 3 real (xml, population.i64, one ESC-50 WAV).

## Binaries

| generation | source SHA | binary sha256 |
|---|---|---|
| v1.0.1 | a1d26f207512 | ef7084a42461f72a… |
| main-1.1.0 | 3c6890a91314 | 2fc0e02c7143fee4… |
| rc2 | b2f0c0034da7 | ceabe65f02ed79ce… |
| rc3 | 2293c857158e | b76733e44f97eb72… |
| w3-decmap-int | a17d66579007 | 36a230e3c0959792… |
| w3-prior | ae00b26ff68c | f5261e90135a10a7… |
| w4-bwt | 5ce318ad50e3 | 2e3e0d091a849378… |
| w4-cm | ca81739efdc3 | 8c6b664c7eca218d… |
| w4-docs | fba285584a8f | 7b7531a3f8e2559f… |
| w4-entropy | 5f24d6aeffe3 | 46191948355ceee0… |
| w4-lz | 3a0278052859 | 036912ef470b0815… |
| w4-multi | fdb997607229 | 8905136122d1651c… |

## Matrix (rows: encoder:setting; columns: decoder)

| enc:setting | v1.0.1 | main-1.1.0 | rc2 | rc3 | w3-decmap-int | w3-prior | w4-bwt | w4-cm | w4-docs | w4-entropy | w4-lz | w4-multi |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| main-1.1.0:fast | 8/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| main-1.1.0:max | 9/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| main-1.1.0:rt | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| main-1.1.0:ultra | 3/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| rc2:fast | 8/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| rc2:max | 9/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| rc2:rt | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| rc2:ultra | 3/12 R | 7/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| rc3:balanced | 8/12 R | 9/12 R | 9/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| rc3:eco | 8/12 R | 9/12 R | 9/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| rc3:nat-all-balanced | 2/12 R | 2/12 R | 2/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 3/12 R | 12/12 | 12/12 |
| rc3:nat-all-hc | 2/12 R | 2/12 R | 2/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 3/12 R | 12/12 | 12/12 |
| rc3:nat-all-speed | 4/12 R | 4/12 R | 4/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 4/12 R | 12/12 |
| rc3:nat-allstrong-balanced | 2/12 R | 2/12 R | 2/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 10/12 R | 12/12 | 12/12 |
| rc3:ratio | 6/12 R | 7/12 R | 7/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| rc3:speed | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| rc3:x | 2/12 R | 3/12 R | 3/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| rc3:x-l2 | 2/12 R | 2/12 R | 2/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| v1.0.1:fast | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| v1.0.1:max | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| v1.0.1:rt | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| w3-decmap-int:balanced | 8/12 R | 9/12 R | 9/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| w3-decmap-int:hc | 8/12 R | 9/12 R | 9/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| w3-decmap-int:nat-all-balanced | 2/12 R | 2/12 R | 2/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 3/12 R | 12/12 | 12/12 |
| w3-decmap-int:nat-all-hc | 2/12 R | 2/12 R | 2/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 3/12 R | 12/12 | 12/12 |
| w3-decmap-int:nat-all-speed | 4/12 R | 4/12 R | 4/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 4/12 R | 12/12 |
| w3-decmap-int:speed | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| w3-prior:ratio | 6/12 R | 7/12 R | 7/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| w3-prior:x | 2/12 R | 3/12 R | 3/12 R | 10/12 R | 10/12 R | 12/12 | 10/12 R | 10/12 R | 10/12 R | 10/12 R | 10/12 R | 10/12 R |
| w4-bwt:ratio | 6/12 R | 7/12 R | 7/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| w4-bwt:x | 2/12 R | 3/12 R | 3/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| w4-cm:ratio | 6/12 R | 7/12 R | 7/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| w4-cm:x | 2/12 R | 3/12 R | 3/12 R | 5/12 R | 5/12 R | 5/12 R | 5/12 R | 12/12 | 5/12 R | 5/12 R | 5/12 R | 5/12 R |
| w4-docs:balanced | 8/12 R | 9/12 R | 9/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| w4-docs:ratio | 6/12 R | 7/12 R | 7/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| w4-entropy:balanced | 8/12 R | 8/12 R | 8/12 R | 10/12 R | 10/12 R | 10/12 R | 10/12 R | 10/12 R | 10/12 R | 12/12 | 10/12 R | 10/12 R |
| w4-entropy:hc | 8/12 R | 8/12 R | 8/12 R | 10/12 R | 10/12 R | 10/12 R | 10/12 R | 10/12 R | 10/12 R | 12/12 | 10/12 R | 10/12 R |
| w4-entropy:nat-all-balanced | 2/12 R | 2/12 R | 2/12 R | 3/12 R | 3/12 R | 3/12 R | 3/12 R | 3/12 R | 3/12 R | 12/12 | 3/12 R | 3/12 R |
| w4-entropy:nat-all-hc | 2/12 R | 2/12 R | 2/12 R | 3/12 R | 3/12 R | 3/12 R | 3/12 R | 3/12 R | 3/12 R | 12/12 | 3/12 R | 3/12 R |
| w4-entropy:nat-all-speed | 4/12 R | 4/12 R | 4/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 4/12 R | 12/12 |
| w4-entropy:nat-huf-balanced | 2/12 R | 2/12 R | 2/12 R | 10/12 R | 10/12 R | 10/12 R | 10/12 R | 10/12 R | 10/12 R | 12/12 | 10/12 R | 10/12 R |
| w4-entropy:speed | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| w4-lz:balanced | 8/12 R | 9/12 R | 9/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| w4-lz:hc | 8/12 R | 9/12 R | 9/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| w4-lz:nat-all-balanced | 2/12 R | 2/12 R | 2/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 3/12 R | 12/12 | 12/12 |
| w4-lz:nat-all-hc | 2/12 R | 2/12 R | 2/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 3/12 R | 12/12 | 12/12 |
| w4-lz:nat-all-speed | 4/12 R | 4/12 R | 4/12 R | 4/12 R | 4/12 R | 4/12 R | 4/12 R | 4/12 R | 4/12 R | 4/12 R | 12/12 | 4/12 R |
| w4-lz:nat-lz-speed | 4/12 R | 4/12 R | 4/12 R | 4/12 R | 4/12 R | 4/12 R | 4/12 R | 4/12 R | 4/12 R | 4/12 R | 12/12 | 4/12 R |
| w4-lz:speed | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| w4-multi:balanced | 8/12 R | 9/12 R | 9/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |
| w4-multi:ratio | 6/12 R | 7/12 R | 7/12 R | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 | 12/12 |

## Findings

1. **Backward reads hold.** Every new decoder (rc3 and all 8 branch builds) decodes all 132 v1.0.1 / 1.1.0 / rc2 frames.
2. **Older readers refuse newer frames safely.** rc2 and earlier reject v10/v11 frames (refused version), never misdecode.
3. **W4 entropy reuses codec 27 (khuf) and changes KLZH 29 with new layouts.** Its DEFAULT balanced/hc output now
   writes codec-27 leaves (ints.i32), which rc3 rejects; its decoder rejects rc3's opt-in KHUF/KLZH frames
   (2/9 exact). Rejection is by validation luck, not by version: id-collision probe (`idcollision/`) 670/671 rejected,
   1 frame valid in both layouts. **Required before integration: a new codec id (or a khuf version byte) for the new
   layout, and either a legacy decoder for rc3's 27/29 or an explicit statement that rc3 opt-in frames are not
   supported (rc3 was never released).**
4. **W4 lz reuses KLZ 28 with a new layout** (opt-in): its decoder rejects rc3 KLZ frames (4/9 exact); same remedy.
5. **W4 cm `-x` writes CM model 4 (nibble 9)**: only W4 cm reads it; rc3 and others reject. W3 prior writes a
   different "model 4" (nibble 15 + bit 7): rc3 rejects 1/9 of its -x frames, and W4 cm rejects them too.
   The two must not both ship under overlapping internal ids; renumber one.
6. **KOLMO_NATIVE bumps the container to version 11 even when no native codec is selected** (C5 matrix: 13–49 of 104
   frames per setting), making such frames unreadable by rc2/1.1.0 for no benefit.

## Not covered
- x86-64 decoders reading arm64 frames and vice versa: no Rosetta on this Mac; CI (fixed workflow) is required.
- Real historical archives from users: none supplied; only frames generated by rebuilt release binaries plus the
  repository's own `tests/legacy_frames.h` (run inside `make check`).
- Migration tool: not needed for released formats (all readable); none written.


---

# Addendum, 2026-11-20 — candidate C1 (302e31a6, release build badac9ea…)

`compat-matrix-c1/rows.jsonl` holds 9,516 decodes:

| outcome | decodes |
|---|---:|
| exact | 7,960 |
| refused | 1,556 |
| silent misdecode | **0** |
| crash | **0** |

The C1 decoder was added to every generation, and C1 encoders were added to the encoder set.

- **C1 reads like rc3.** For every frame in the matrix, C1's decode outcome is the same as rc3's. C1 decodes exactly all v1.0.1, main-1.1.0, rc2, rc3, decmap, W4 bwt, W4 docs and W4 multi frames.
- **Refusals are the same as rc3's.** Both refuse W4 entropy (24 frames), W4 lz (16), W4 cm (7) and W3 prior (2). These are the codec-id collisions described in findings 3–5.
- **C1 writes rc3's bytes.** All 120 C1-encoded frames are byte-identical to rc3's frames for the same input and setting. Every decoder generation treats them exactly as it treats rc3 frames.
- **Policy.** No migration tool is needed: C1 introduces no new format. The codec-id collisions in W4 entropy, W4 lz and W4 cm are excluded from C1 and must get new ids before any merge (INTEGRATION_PLAN).

---

# Addendum, 2026-11-21 — candidate C2 (25bedda8, release build 31b9e558…)

`compat-matrix-c2/rows.jsonl` holds 14,580 decodes: 12,174 exact and 2,406 refused. There were **0 silent misdecodes, 0 crashes, 0 timeouts and 0 encode failures**.

**C2 as a reader.** C2 decodes every frame exactly as C1 does. The only differences are the CM model-4 `-x` / `-x -l 2` frames written by W4 cm, C2-cm and C2: C2 reads them exactly, and C1 refuses them (16 + 16 + 7 frames).

**C2 as a writer** (120 frames over 10 settings):

| reader | frames read |
|---|---|
| C2, C2-cm, W4 cm | 120 / 120 |
| rc3, C1, W4 bwt/docs/multi, W3 decmap/prior | 104 / 120 (the 16 `-x` frames are refused, safely) |
| rc2, 1.1.0 | 50 / 120 (newer generations refused) |
| v1.0.1 | 46 / 120 |

The W4 cm model-4 frames are refused by explicit checks, not by chance. The reserved nibble rules are in `SPEC.md` at a555c05d, and the probe is `tools/cm_xprobe.py` (280 inputs, 0 misdecodes).

**KOLMO_NATIVE frames without a native id** are now v3/v7 and readable by rc2/1.1.0 (see `native-version-c2-2`).
