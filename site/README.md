# Benchmark page

`index.html` is generated; do not edit it by hand.

```sh
python3 site/build.py                              # re-render from results.json
python3 site/build.py --results WORK/results       # merge a bench/repro run (sh bench/repro/run.sh WORK does this)
python3 site/build.py --import-report EVIDENCE     # merge the 0.0.2 report's raw files (lzb-*.csv, mt-result.json, openzl.json)
```

- `results.json` holds every number the page shows, with the source (machine, date, Kolmo version) of each
  section. A merge replaces whole sections, so a page never mixes two runs within one section.
- Wins and losses are computed by `build.py` from the data: a Kolmo setting is listed as a loss when another codec
  is at least as small and decodes faster, or beats it on ratio and both speeds; the OpenZL tables count wins, ties
  (within ±1%) and losses per file.
- The page is one self-contained HTML file (inline SVG, no external scripts or fonts), so GitHub Pages can serve
  this directory as is. Publishing is the owner's decision; nothing here enables Pages.
