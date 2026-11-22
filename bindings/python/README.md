# kolmo (Python binding)

Python bindings for [kolmo](https://github.com/adnaneben/kolmo), a structure-aware lossless
compressor. Thin `ctypes` wrappers over the native core; the shared library is built and
bundled at install time, so there is no runtime build dependency.

```bash
pip install kolmoc   # the distribution is "kolmoc" ("kolmo" was taken on PyPI); it imports as kolmo
```

```python
import kolmo
blob = kolmo.compress(open("data.csv", "rb").read(), mode="balanced")  # speed | balanced | ratio
data = kolmo.decompress(blob)                                           # whole file
part = kolmo.read_range(blob, 1_000_000, 4096)                          # decode only the overlapping blocks
```

Modes: `speed` (fastest), `balanced` (the default), `ratio` (smallest at practical speed).
Advanced settings: `rt`, `fast`, `eco`, `hc`, `max`, `ultra`. `threads=0` (the default) uses all
cores. Apache-2.0.

Every coder is Kolmo's own (KLZ, KLZH2, KLZMA, khuf, BWT, context mixing): no third-party codec
library is built, bundled or needed.

**From 1.x:** since 0.0.1, kolmoc writes container version 12 and cannot read files written by kolmoc 1.0.x
(`decompress` raises `ValueError`: "written by an earlier Kolmo (1.x or a release candidate)…"; it
never returns wrong data).
Dictionaries and trained profiles are removed. `pip install -U` does not move from 1.0.1 to the
0.0.x line; use `pip install "kolmoc<1"`. To convert old files, see "Files written by Kolmo 1.x" in the
[project README](https://github.com/adnaneben/kolmo#files-written-by-kolmo-1x).

Prebuilt wheels cover Linux (x86-64 and aarch64) and Apple silicon Macs running macOS 11 or
later. Other platforms, Intel Macs included, build the sdist, which needs a C compiler and takes
about a minute.

For the command-line tool, use `brew install adnaneben/kolmo/kolmo`. The Rust crate is
`cargo add kolmo`.
