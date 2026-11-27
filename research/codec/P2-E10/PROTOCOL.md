# P2-E10 — frozen type-first policies on a broader development cohort (N1 complete-CLI boundary)

Registered before any P2-E10 measurement. Development only: every source was observed in E02E/E02F/E02G.

Inputs (`prepare_e10.py`, rules fixed here): all derived `.npy` inputs written by the unchanged E02E, E02F and E02G
`prepare.py` scripts from their registered downloads (hash-checked against their manifests); whole original files:
FSDD speaker `jackson` take 0 of digits 0–9 (10 16-bit PCM WAVs as published inside the pinned zip) and the Backblaze
daily CSV `data_Q1_2023/2023-01-01.csv` (as published inside the pinned zip).

Policies: N1's zstd-3 and Kolmo controls (fast quick / fast / hc) plus `kolmo -m max`; OpenZL at level 9 with the
typed profile matching each file's declared type (`le-i16`, `le-i32`, `le-u32`, `le-i64`, … from the NPY descr; WAV
`le-i16`; CSV `csv`) **applied to the whole original file** (headers become a few extra values; lossless; a mild
handicap for OpenZL to be noted with any result), and `serial` 9 as a generic control; frozen research policies
`tf_planes_huf`, `tf_planes_huf_pg16`, `tf_prod19`, `tf_planes_ent19` (tfcli) and `mcsv_planes_huf`, `mcsv_prod19`
(mcli). OpenZL level 6 is omitted to bound runtime.

Rivals: zstd-3 CLI (speed); OpenZL typed level 9 on the whole file (archive). Gate: ASan/UBSan/LSan round trip of
every research policy on every input before timing. Two seeded runs + N1 `confirm`; N1's per-process limits (90 s,
6 GiB) apply and any hit is a recorded failure. Reporting: per family, never pooled across families; derived NPY and
whole original files reported separately.
