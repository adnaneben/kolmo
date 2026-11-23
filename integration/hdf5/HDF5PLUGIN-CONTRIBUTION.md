# DRAFT: adding Kolmo to hdf5plugin

**Status: draft for the owner. Nothing has been opened on silx-kit/hdf5plugin.**

[hdf5plugin](https://github.com/silx-kit/hdf5plugin) ships HDF5 compression filters as a Python wheel, so h5py users get them with `pip install hdf5plugin`. This note lists what a pull request adding Kolmo would contain. It follows the project's `doc/contribute.rst` (read 2026-11-23).

## Before opening the PR

1. **A registered filter ID.** `_filters.py` cites the HDF Group list for every filter ID. See [FILTER-ID-REQUEST.md](FILTER-ID-REQUEST.md); the testing ID 480 must not ship.
2. **A Kolmo release that contains the plugin.** hdf5plugin asks for "a released version of the filter + compression library". By the owner's rule, nothing is released until every adoption phase is done.
3. Optional but useful: open an issue on silx-kit/hdf5plugin first, with the benchmark from [README.md](README.md#benchmark), to ask whether they want the filter.

## What the PR contains

| file | change |
|---|---|
| `lib/kolmo/` | the Kolmo sources of the release: `src/` (the core, no third-party code), `LICENSE` (Apache-2.0), `NOTICE`, and `integration/hdf5/H5Zkolmo.{c,h}`. Copied or `git subtree`, as their guide says. |
| `setup.py` | `HDF5PluginExtension("hdf5plugin.plugins.libh5kolmo", ...)`, with the sources listed below |
| `src/hdf5plugin/_filters.py` | `KOLMO_ID = <registered ID>` and a `Kolmo` helper class (below); add it to `FILTER_CLASSES` |
| `src/hdf5plugin/__init__.py` | export `KOLMO_ID` and `Kolmo` |
| `src/hdf5plugin/test.py` | write and read back with `Kolmo()` at a few levels, for float32, float64, int16 and int64 |
| `test/test.py` | read a small file written by the C plugin (`integration/hdf5/tests` can produce one) |
| `doc/information.rst` | the Kolmo version and a link to its licence |
| `doc/usage.rst` | the `Kolmo` helper class |
| `doc/contribute.rst` | the `compression_opts` layout |

### Extension sources

These are the same files `bindings/python/setup.py` compiles for Kolmo's own wheel:
- `src/kolmo.c`, which includes the rest of the core;
- `src/algo/{lzp,qlfc,par,khuf,krans,klz,klzma,khash}.c` and `src/algo/sais/*.c`;
- `src/codecs/bwt_stream.c`;
- `src/arch/cpu.c` and `src/algo/{predict,planes,mixer,video}.c`;
- the assembly of the target ISA, `src/arch/{x86_64,aarch64}/*.S`. Define `KOLMO_NO_ASM` to build the C references only.

Plus `H5Zkolmo.c`, with `-Isrc` and `-ffp-contract=off`. Kolmo requires the latter so that every build produces the same bytes. Build portable, with no `-march=native`, as in Kolmo's wheel; Kolmo picks faster kernels at run time.

Windows: Kolmo's own CI covers Linux and macOS only, so a Windows build of the core must be checked before the PR. Their wheels cover Windows.

### Helper class (draft)

```python
KOLMO_ID = <registered ID>
"""Kolmo filter ID registered with The HDF Group"""


class Kolmo(_FilterRefClass):
    """``h5py.Group.create_dataset``'s compression arguments for the Kolmo filter.

    :param int level: 1 (fastest) to 9 (smallest), 10 for the archival mode; 0 = Kolmo's default (3).
    :param int threads: threads per chunk (default 1; use more only for chunks of several MiB).
    """

    filter_name = "kolmo"
    filter_id = KOLMO_ID

    def __init__(self, level: int = 3, threads: int = 1):
        level = int(level)
        if not 0 <= level <= 10:
            raise ValueError("level must be 0-10")
        # cd_values: level, element size (0 = from the dataset's type), threads
        self.filter_options = (level, 0, int(threads))

    def _init_from_filter_options(self, *options):  # read back from a dataset's cd_values
        ...
```

`compression_opts` / `cd_values` layout: `(level, element_size, threads, layout_version)`. Every value is optional. The plugin fills in the element size and the layout version when the dataset is created.

## Notes for the PR text

- The benchmark table in [README.md](README.md#benchmark), at one thread against deflate, zstd (their filter ID 32015) and shuffle.
- The filter does not link libhdf5: it uses the HDF5 symbols of the process, as their plugins do through `lib/hdf5_dl.c`. If their build needs it, list the functions the plugin calls in `hdf5_dl.c`: `H5Pget_filter_by_id2`, `H5Pmodify_filter`, `H5Tget_size`, `H5Epush2`, `H5allocate_memory`, `H5free_memory` and `H5Zregister`.
- Licence: Apache-2.0, which is compatible with hdf5plugin's MIT licence and with the other bundled filters.
