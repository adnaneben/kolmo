# Kolmo paper draft (target: DCC 2028)

Status: **draft, not submitted.** IEEE conference layout (`IEEEtran`). It is 8 pages including references, so there
is room under the 8-page limit, which excludes references. Several numbers will be replaced (see "Open TODOs").

## Build

```
make -C docs/paper
```

This needs Python 3 (standard library only) and [tectonic](https://tectonic-typesetting.github.io/)
(`brew install tectonic`). Tectonic downloads the TeX packages it needs the first time it runs. With a TeX Live
install, use `make -C docs/paper ENGINE=latexmk` instead. `make -C docs/paper clean` removes the outputs.

The build runs in two steps:
1. `figures/make_figures.py` reads `data/` and writes `generated/`. That folder holds the Pareto figure as pgfplots
   code, every table, and `numbers.tex`, which defines one macro for each number quoted in the running text.
2. tectonic typesets `kolmo.tex` into `kolmo.pdf`.

The `generated/` folder and the PDF are not committed.

In `kolmo.tex`, `\drafttrue` (the default) prints a grey `[source file]` tag after the numbers in the text, and shows
the TODO boxes. `\draftfalse` hides both.

## Where every number comes from

Each number in the paper comes from one of these places:
- `data/`, through `generated/`. Every generated file starts with a comment that names its data file, and every
  table and figure caption names its source.
- A cited document in the repository, such as FORMAT.md, docs/LEVELS.md or docs/METADATA.md.

`data/` holds the raw results of the 0.0.2 market benchmark of 2026-11-23. The machine was an Apple M5 with
10 cores, on AC power, running macOS. Kolmo was built from tag `v0.0.2` with `ARCH=` (generic flags). Every tool ran
on one thread unless the file name says otherwise. The files were copied from the measurement workspace
(`evidence/mkt-0.0.2/`) unchanged, except that the lzbench `Filename` column was cut to the base name.

| File | What |
|---|---|
| `lzb-fast-*.csv`, `lzb-slow-*.csv` | lzbench 2.4.1 on silesia.tar, enwik8 and canterbury.tar. Fast codecs best of 3 (`-t3,3`), slow ones run once |
| `lzb-silesia-files.csv` | lzbench, Silesia file by file |
| `openzl.json` | Structured panel: Kolmo -3/-5/-7/-9/-U, OpenZL 0.3.0 `zli` (format profile, levels 6 and 9, untrained), zstd -3/-19, xz -9. Command line, best of 3 |
| `mt-result.json` | Silesia on the command line, one thread and all cores, with peak RSS |
| `holdout-panel.json` | Trained-OpenZL rematch (bench/repro, PR #29, 2026-11-24): held-out GHCN CSV, GH Archive JSON, NASA logs and NCEP grids (OpenZL trained and untrained), four Silesia members, three model checkpoints with ZipNN. Workspace paths cut to relative |
| `pr32/` | Kolmo only, before/after the unmerged encoder change (PR #32): lzbench `{fast,slow}-{old,new}-*.csv`, old = 0.0.2 library, new = the branch, paired on one machine. Used for macros in the limitations only; the tables stay on 0.0.2 |
| `x86/claims-*.run{1,2,3}.csv` | x86-64 check (PR #33, GitHub run 45295174961): each Silesia/enwik8/Canterbury claim pair (Kolmo -3/-4/-6/-9 and zstd -12/-19, xz -9, zpaq -5) in one job on an Intel Xeon 6975P-C, lzbench 2.4.1, three runs. `corpora/` prefix cut from `Filename` |
| `x86/cli-silesia.json` | x86-64 command line on Silesia, one thread and 4 vCPUs, AMD EPYC 9V45 (GitHub run 45250622103), unchanged |
| `holdout-dfloat11.json` | DFloat11's published method computed on the CPU for the two bf16 checkpoints (not a run of DFloat11) |

Removing `holdout-panel.json` drops the held-out and weights tables from the next build.

The M5 lzbench run used a small plugin that calls libkolmo; `bench/lzbench/` (x86-bench lane) is its successor in the
repository. The OpenZL panel script is `openzl_panel.py` in the same workspace; `bench/repro/` (bench-page lane)
replaces it.

## Open TODOs (red boxes in the draft)

1. ~~x86-64 rerun~~: done as a check (Table V, "A second platform"). The full x86 fast/slow tables were not used:
   their jobs ran on different CPUs, so speeds across rows are not comparable. The M5 tables stay the main ones.
   The `\todoin` marker for PR #33 stays until it is merged.
2. ~~Trained-OpenZL rematch~~: done (Tables VII and IX). The marker stays until PR #29 is merged.
3. **Tuned levels** (PR #32, open): their effect is quoted in the limitations from `data/pr32/`. Once the next
   release exists, rerun every benchmark on it and regenerate every table and Table II.
4. **Smaller items:**
   - a decision-flow figure;
   - the LOCO-I citation;
   - the fuzzing and OSS-Fuzz status;
   - ~~the frozen-corpus count~~: 350 on main (PR #22 merged);
   - the HDF5 and Blosc2 small-chunk numbers;
   - the author's affiliation.

## References

`refs.bib` has 42 entries. Each was checked against a primary source (DOI record, arXiv, the RFC editor or the
project page) on 2026-11-23. The source URL is in a `% verified:` comment above each entry. These fields could not be
confirmed and were left out:
- the ACM article numbers of the Brotli (TOIS) and ALP papers;
- the printed title of the ZPAQ specification PDF.
