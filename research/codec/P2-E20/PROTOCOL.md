# P2-E20: preregistered fresh source-disjoint validation (frozen before any fresh data was read)

The user approved this on 13 November 2026 at 05:03 Asia/Riyadh: fresh sources, wire review, and "P2-D1: test before
deciding". This protocol, `prepare_fresh.py`, `run_e20.py` and `analyze_e20.py` are committed **before** the
workflow runs. The workflow records their SHA-256 hashes in `ev/source-hashes.txt`.

## Sources (source-disjoint)

Neither source, nor any path from it, is referenced in the repository, the benchmark registers or
`benchmark/holdout/inputs.json` before this commit. A grep over the repository for "esc-50", "esc50", "karolpiczak"
and "fivethirtyeight" found only `P2-SUMMARY/README.md`, where they are named as candidates.

| Source | Pinned commit | Licence | Selection |
|---|---|---|---|
| ESC-50 (`karolpiczak/ESC-50`) | `33c8ce9eb2cf0b1c2f8bcf322eb349b6be34dbb6` | CC BY-NC 3.0, research use, not redistributed | 40 whole WAV clips, 8 per ESC-50 major category |
| FiveThirtyEight (`fivethirtyeight/data`) | `4c1ff5e3aef1816ae04af63218015066e186c147` | CC BY 4.0 | 24 whole CSV files, one per dataset directory, 100 KB–5 MB |

Selection is deterministic and uses **paths only** (exact rule in `prepare_fresh.py`). The inputs are not
committed; the workflow re-fetches them by pinned commit and path, and their hashes are recorded.

**Integrity note.** While choosing the pins, a local `git ls-tree -l` on a blob-less clone triggered lazy fetching of
some ESC-50 and FiveThirtyEight objects into a temporary local object store. That attempt was looking for sizes, not
content. No file was opened, printed, decoded or measured, and the clone was deleted within minutes
(`rm -rf /tmp/fresh`). The selection rule does not depend on anything seen there: only the pinned commits and path
names were used.

## Boundary, rivals and policies

These are frozen in `run_e20.py`. The boundary is N1's complete-CLI harness, unchanged, with pinned zstd 1.5.7 and
OpenZL v0.3.0. There are two runs (seeds 20261112 and 20261113) and N1 `confirm`.

| Role | Pipelines |
|---|---|
| Speed rival | `zstd_3` |
| Archive rival | `openzl_<profile>_9`: `le-i16` for whole WAVs, `csv` for CSVs |
| Production | `kolmo` fast, hc, `-m max` |
| Production-format research frames (production CLI decodes) | `tp_phuf_pg16`, `tp_phuf`, `tb_lazy`, `tb_best`, `tp_pent`, `tp_arch19` |
| Research containers, CSV only (the P2-D1 test) | `mcsv_planes_huf`, `mcsv_prod19` |

Sanitizer gate before timing: every research pipeline on every input. If any pipeline fails to round-trip, the job
fails, and that failure is itself the finding.

## Hypotheses and decision rules (`analyze_e20.py`)

- **H1 (speed rule, primary).** On the 40 ESC-50 WAVs, the fixed rule `tp_phuf_pg16` fits zstd-3's complete-CLI cost
  on every pair and is smaller. **Pass** if, in both runs, the one-sided 95% Clopper–Pearson lower bound on the
  success proportion is above 0.5. Per-family counts and every other pipeline's counts are reported, not tested.
- **H2 (archive).** Per source (ESC-50, FiveThirtyEight): count the inputs where the smallest pipeline inside
  OpenZL-9's archive cost on every pair is a Kolmo pipeline (production modes or production-format frames;
  containers excluded). **Pass** if the Kolmo count exceeds OpenZL's in both runs. A missing OpenZL result is
  reported and not counted as a Kolmo win.
- **H3 (P2-D1: are research containers useful?).** On the FiveThirtyEight CSVs, a research container is "useful" for
  an input if it is the **unique** smallest pipeline inside the rival's cost (zstd-3 for speed, OpenZL csv-9 for
  archive), counting every pipeline in the run. **Useful** if this holds on at least 20% of CSVs in either mode, in
  both runs. The recommendation follows mechanically:
  - useful: keep P2-D1 open and propose admitting research containers as `complete_pipeline` candidates;
  - not useful: recommend that P2 acceptance considers production-decodable frames only.

No other hypotheses are tested. The results are reported per source, with no pooling across sources. Any exploratory
look after this run is labelled exploratory. A pass is evidence for a research claim on this boundary only: it does
not change defaults, the format or release status.

Budget: one workflow, timeout 110 minutes. It is started before the 04:00 UTC cut-off.
