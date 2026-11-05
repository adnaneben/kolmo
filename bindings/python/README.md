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

Installing from source needs a C toolchain and `liblzma`. Distributable wheels (one per
platform, no toolchain needed) are produced in CI with `cibuildwheel` / `python -m build`.
