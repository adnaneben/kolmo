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

**Held-out panel (pending).** When the trained-OpenZL rematch is final, copy `bench/repro`'s `results/panel.json` to
`data/holdout-panel.json`. The table (Table VII) and its counts then appear on the next build; without the file
they are left out.

The M5 lzbench run used a small plugin that calls libkolmo; `bench/lzbench/` (x86-bench lane) is its successor in the
repository. The OpenZL panel script is `openzl_panel.py` in the same workspace; `bench/repro/` (bench-page lane)
replaces it.

## Open TODOs (red boxes in the draft)

1. **x86-64 rerun.** Replace the lzbench tables, Fig. 1 and Table VII. Keep the M5 as a second platform.
2. **Trained-OpenZL rematch** on public held-out data (GHCN-Daily, GH Archive, NASA logs, NCEP reanalysis, three
   model checkpoints). Also add ZipNN and DFloat11 (method computed on the CPU) on the model weights. This replaces
   Table VI and its paragraph.
3. **Tuned levels** from the weak-levels lane: the new level-1 parse and a faster KLZH2 decode. Regenerate every
   table and update Table II and the limitations.
4. **Smaller items:**
   - a decision-flow figure;
   - the LOCO-I citation;
   - the fuzzing and OSS-Fuzz status;
   - the frozen-corpus count (341 after the format-prose PR);
   - the HDF5 and Blosc2 small-chunk numbers;
   - the author's affiliation.

## References

`refs.bib` has 42 entries. Each was checked against a primary source (DOI record, arXiv, the RFC editor or the
project page) on 2026-11-23. The source URL is in a `% verified:` comment above each entry. These fields could not be
confirmed and were left out:
- the ACM article numbers of the Brotli (TOIS) and ALP papers;
- the printed title of the ZPAQ specification PDF.
