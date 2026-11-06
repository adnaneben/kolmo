# kolmo (Python binding)

Python bindings for [kolmo](https://github.com/adnaneben/kolmo), a structure-aware lossless
compressor. Thin `ctypes` wrappers over the native core; the shared library is built and
bundled at install time, so there is no runtime build dependency.

```bash
pip install kolmoc   # the distribution is "kolmoc" ("kolmo" was taken on PyPI); it imports as kolmo
```

```python
import kolmo
blob = kolmo.compress(open("data.csv", "rb").read(), mode="hc")  # rt|fast|eco|hc|max|ultra
data = kolmo.decompress(blob)                                     # whole file
part = kolmo.read_range(blob, 1_000_000, 4096)                    # decode only the overlapping blocks
```

`threads=0` (the default) uses all cores. Apache-2.0.

Linux (x86-64 and aarch64) gets prebuilt wheels from PyPI, so no toolchain is needed. Other
platforms, macOS included, build the sdist, which needs a C compiler and takes about a minute.
liblzma is optional. If it is found (`brew install xz`, `apt install liblzma-dev`), `max` and
`ultra` use it; otherwise they use the other coders. Set `KOLMO_NO_LZMA=1` to always build
without it.

For the command-line tool, use `brew install adnaneben/kolmo/kolmo`. The Rust crate is
`cargo add kolmo`.
