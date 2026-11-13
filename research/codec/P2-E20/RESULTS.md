# P2-E20 results: preregistered fresh source-disjoint validation

Run: Actions **43350744793** at `b01b15d`, 02:14–02:30 UTC (~16 runner-minutes).

Integrity:
- The runner's recorded hashes of `PROTOCOL.md`, `prepare_fresh.py`, `run_e20.py` and `analyze_e20.py` equal the
  committed, frozen files. That commit preceded the first fresh read.
- Inputs: **40 ESC-50 WAV** (8 per major category, 441,044 bytes each) and **24 FiveThirtyEight CSV** (101 KB–4.6 MB),
  44.4 MB in all. 68 FiveThirtyEight candidates were size-checked to reach 24 (`results/prepared/manifest.json.xz`).
- Recovered from the job log: 15/15 printed files match `MANIFEST.json`. The frozen analysis reproduces exactly from
  the committed records.
- Sanitizer gate (all research encoders; the production CLI decodes the frames; `mcli` round trip for containers):
  **432/432, 0 failures**.
- N1 `confirm`: **IDENTITIES_MATCH**, 751/752 configurations. The one failure is OpenZL `csv` profile level 9, which
  errors on `classic-rock`. It is reported as missing and not counted as a Kolmo win.

## Preregistered decisions (both runs)

| Hypothesis | Run 1 | Run 2 | Decision |
|---|---|---|---|
| **H1** `tp_phuf_pg16` fits zstd-3 and is smaller on ESC-50 WAV; CP95 lower bound > 0.5 | 25/40 (LB 0.483) | 24/40 (LB 0.458) | **FAIL** (observed 60–62.5%, not significant at the frozen bar) |
| **H2** archive, ESC-50: Kolmo smallest within OpenZL `le-i16`-9 cost, more often than OpenZL | Kolmo 35, OpenZL 5 | 35 / 5 | **PASS** (frontier −10.3% bytes) |
| **H2** archive, FiveThirtyEight CSV vs OpenZL `csv`-9 | Kolmo 0, OpenZL 23 (1 missing) | 0 / 23 | **FAIL** |
| **H3 / P2-D1** research containers are the unique smallest within the rival's cost on ≥ 20% of CSVs | speed 4/24, archive 1/24 | 4/24, 1/24 | **NOT USEFUL** → close P2-D1, production-decodable pipelines only |

### H1 details

Per category (run 1 / run 2):

| Category | Run 1 | Run 2 |
|---|---|---|
| animals | 7/8 | 6/8 |
| natural soundscapes | 4/8 | 3/8 |
| human non-speech | 7/8 | 5/8 |
| interior/domestic | 2/8 | 4/8 |
| exterior/urban | 5/8 | 6/8 |

Other pipelines, fits and smaller (run 1): `tp_phuf` 24, `tb_lazy` 23, `tb_best` 6, `tp_pent` 3. Production
`kolmo` fast, hc and max are **0/40**.

On fresh audio the type-first frame beats zstd-3 within its speed budget on a clear majority. Production Kolmo
never does. The 80% rate on development inputs (P2-E14) did not carry over: fresh clips include near-silent and
noise-like categories, where the guard routes to zstd or the margin is too small.

### H2 details on ESC-50

Archive winners (run 1):
- `tp_arch19` 19, `tp_pent` 9, `tb_best` 3: the production-format research frames take 31/40 between them.
- production `max` 3, `fast` 1;
- OpenZL 5.

## Exploratory (not preregistered; labelled as such)

**Real-world CSVs are a different regime from the development tables.** Production `kolmo hc` and `max` are smaller
than OpenZL `csv`-9 on 22–23/23 CSVs (`max` by 8–43%). However:
- their modelled archive cost is 1.05–2.1× OpenZL's;
- production `fast` is cheaper than OpenZL on most files but 1–26% larger.

Nothing fits inside OpenZL's cost while also being smaller. Under the zstd-3 speed rival, at most 5 CSVs have any
smaller feasible pipeline, and each by under 1%.

This narrows the development finding: P2-E7's large typed-CSV wins on Covertype and CollegeMsg were on regular
numeric tables. Mixed text-heavy CSVs need a cheaper heavy-ratio path, not a different container.

Two further observations:
- OpenZL v0.3.0's `csv` profile failed on 1/24 real CSVs (`classic-rock`).
- Production `kolmo hc` took 1.3 s to encode that 2.5 MB file, versus 101 ms for `max`. That is a performance
  anomaly worth a separate look.

## Consequences

- **P2-D1 is resolved by test**, as the user directed ("if useful keep it; test before deciding"). Research containers
  were not useful on fresh CSVs, so P2 research acceptance should consider **production-decodable pipelines only**.
  Nothing is lost: every type-first array pipeline already exists as a production frame (P2-E11 to P2-E14).
- **Supported claim** on this boundary, from fresh data: in archive mode against OpenZL's typed profile, type-first
  production frames plus Kolmo `max` give the smallest pipeline within cost on 35/40 fresh audio clips. This is a
  research claim, with no format, default or release change.
- **Not supported:** the speed rule clearing a pre-set majority bar on fresh audio, and any archive or speed
  advantage on fresh real-world CSVs.
