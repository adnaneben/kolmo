# kolmo (Python binding)

Python bindings for [kolmo](https://github.com/adnaneben/kolmo), a structure-aware lossless
compressor. Thin `ctypes` wrappers over the native core; the shared library is built and
bundled at install time, so there is no runtime build dependency.

```bash
pip install kolmoc   # the distribution is "kolmoc" ("kolmo" was taken on PyPI); it imports as kolmo
```

```python
import kolmo
blob = kolmo.compress(open("data.csv", "rb").read(), level=7)  # 1 (fastest) .. 9 (max), or "ultra"
data = kolmo.decompress(blob)                                    # whole file
part = kolmo.read_range(blob, 1_000_000, 4096)                   # decode only the overlapping blocks
```

Levels: one scale from `1` (fastest) to `9` (max), `3` (light) the default, and `"ultra"` (context mixing,
outside the scale: about as slow to decompress as to compress). Names work too: `"fastest"` (1), `"light"` (3),
`"balanced"` (5), `"compact"` (7), `"max"` (9). A level gives the same bytes as `kolmo -N` on the command line.
`threads=0` (the default) uses all cores; `memlimit` takes bytes or `"512M"`, `"8G"`, `"50%"`, `"max"`.
`decompress`, `original_size` and `read_range` read everything the command line writes: one frame, frames one after
another, and `kolmo --stream` output (also what the CLI writes for pipes over 256 MiB). `decompress` checks every
frame before it allocates the output, and the output holds only the pages actually decoded; for untrusted input pass
`max_output_size` (bytes), since a valid frame can claim well over 100,000 times its own size. Apache-2.0.

**New in 0.0.3: a data-loss fix.** Earlier 0.0.x releases could write a file that they could not read back: a
table (CSV, TSV, or a space-separated log) with one field longer than 65,535 bytes, at levels 3 to 9. `compress`
succeeded and `decompress` then raised "corrupt or unsupported input". The data in those files is intact. This
release reads them, and it no longer writes such blocks, so the earlier releases read its output. If you compressed
data with an earlier release, upgrade and run `decompress` on those files before you delete the originals. Details
are in the [changelog](https://github.com/adnaneben/kolmo/blob/main/CHANGELOG.md).

**Since 0.0.2:** `mode=` was replaced by `level=` (`speed` is now `1`, `balanced` `3`, `ratio` `9`,
`ultra` `"ultra"`; `eco` `5`, `hc` `7`), and the dictionary calls (`train_dict`, `dict=`) were removed.

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
