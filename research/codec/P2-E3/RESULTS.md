# P2-E3 results — type-first research pipelines on N1's complete-CLI boundary (development)

Run: Actions **43302008497** (job 131475341390), 21:05–21:18 UTC, ~13 runner-minutes (10 of them building).
Workflow `.github/workflows/p2e3-cli.yml` at `bae4b03`; protocol registered before measurement (`PROTOCOL.md`).
Evidence recovered from the job log; all 13 recovered files match the run's `MANIFEST.json` (`results/`).

Toolchain (`results/toolchain.txt`): OpenZL CLI 0.3.0 (`zli` sha256 `932b011b…`), zstd CLI 1.5.7 (`3f76153c…`),
Kolmo CLI at the repaired source (`7c9996e2…`), `tfcli` built with N1's flags. N1's 12 unit tests passed on the runner.
Sanitizer gate: every research policy round-tripped every prepared input under ASan/UBSan/LSan before timing.
Two seeded runs; N1's `confirm`: **IDENTITIES_MATCH** (150 configurations, 134 complete, 16 failed).

The 16 failures are OpenZL `-p numpy`, which pinned v0.3.0 does not implement (`Profile not found`, see the parallel
N1 audit `research/codec/N1-audit/`). They are invalid invocations, not compression results.

## Speed mode — rival zstd-3 CLI, complete processes (startup + I/O), cost = encode + decode + bytes/125 MB/s

The 8 typed NPY inputs (Covertype columns, CollegeMsg columns), both runs identical in sizes; costs from each run:

| Input | `tf_planes_huf` bytes / cost vs zstd-3 | fits every pair | production `kolmo_fast` bytes / cost |
|---|---|---|---|
| covertype elevation | −41.8% / −33%, −31% | yes, yes | −48.9% / +148%, +146% |
| covertype road_distance | −24.0% / −11%, −15% | yes, yes | −44.8% / +197%, +200% |
| covertype fire_distance | −20.7% / −12%, −16% | yes, yes | −45.1% / +194%, +199% |
| covertype hillshade_control | −25.7% / −30%, −27% | yes, yes | −37.1% / +143%, +143% |
| covertype cover_control | **+46.6%** / −3%, −3% | no (bigger) | +0.3% / +326%, +325% |
| collegemsg source_id | −0.5% / −15%, −19% | yes, yes | +0.1% / +65%, +61% |
| collegemsg target_id | −5.8% / −19%, −21% | yes, yes | −9.4% / +50%, +62% |
| collegemsg unix_timestamp | −59.8% / −16%, −18% | yes, yes | −67.6% / +34%, +41% |

Fits every pair AND smaller than zstd-3, of 8 NPY inputs (first run, confirmation run):
`tf_planes_huf` **7, 7**; `tf_planes_huf_pg16` (16 KiB guard) **7, 7** and fits on 8/8 (cover_control falls back
to parity); `tf_planes_ent19` 5, 5; `tf_huf` 5, 5; `tf_prod3` 3, 3; production `kolmo_fast` / `kolmo_fast_quick` /
`kolmo_hc` **0, 0**.

**CSV views (2 inputs): parity, not wins.** `tfcli` treats CSV as opaque and writes whole-input zstd; its −0.2% /
0.0% "smaller" is only that the zstd CLI adds a frame checksum the research container omits. Not counted.

## Archive mode — rival OpenZL typed profile level 9

- NPY inputs: **BLOCKED** (no `numpy` profile in OpenZL v0.3.0; the `le-u32` profile needs a header-preserving
  adapter, claimed by the parallel N1 workstream).
- CSV views vs `openzl_csv_9`: production `kolmo_hc` is −41.1% (Covertype table) and −29.9% (CollegeMsg table) with
  archive cost below OpenZL's on every pair in both runs. The research `tf_*19` policies are opaque on CSV
  (whole-input zstd-19) and lose (+76% / −11.5% bytes, cost above). Production `kolmo_hc` is not P2-admitted
  (untyped planning, observed data) — this is a control measurement.

## Interpretation (development; no P2 acceptance)

1. The in-memory development result (P2-E2) **reproduces on an independent boundary**: complete CLI processes,
   real pinned zstd-3 CLI, different inputs (Covertype, CollegeMsg), runner hardware. Type-first + identical fast
   integer path + per-plane Huffman is smaller than zstd-3 and inside its round-trip cost on 7/8 typed arrays;
   production fast settings are inside it on none.
2. The guard matters exactly where expected: cover_control (a categorical column) is +46.6% unguarded and parity
   with the 16 KiB guard.
3. These inputs are observed (R3). A claim needs a frozen, source-disjoint validation with whole original files.
