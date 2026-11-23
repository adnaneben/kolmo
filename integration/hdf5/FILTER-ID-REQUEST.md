# DRAFT: HDF5 filter ID request for Kolmo

**Status: draft for the owner. Not sent.** Send it from the owner's own address, after filling in the `<...>` fields.

What The HDF Group asks for (registered filter list, [RegisteredFilterPlugins.md](https://github.com/HDFGroup/hdf5_plugins/blob/master/docs/RegisteredFilterPlugins.md), read 2026-11-23):
- the maintainer's contact information: at least an email address, preferably also a website or GitHub handle;
- the plugin's repository;
- a description of the plugin, with the filter parameters (`cd_nelmts` and `cd_values[]`);
- links to documentation and to the licence.

ID ranges on that page:
- 256-511 is for testing. The plugin uses 480 until an ID is granted.
- 512-32767 is assigned by The HDF Group.

The highest ID listed on 2026-11-23 is 32033 (HTJ2K), so the next is probably 32034. Let them choose; the request does not name a number. No filter named "kolmo" is listed.

Once an ID is granted, set `H5Z_FILTER_KOLMO` in `integration/hdf5/H5Zkolmo.h` (and `FILTER_ID` in `tests/test_h5py.py`) to it, add it to the README, and only then write files meant to be kept or shared.

---

**To:** help@hdfgroup.org
**Subject:** Filter ID request: Kolmo (lossless compression filter plugin)

Hello,

I would like to register an HDF5 filter ID for Kolmo, a lossless compressor for structured and numeric data. The HDF5 filter plugin is open source and ready, and it currently uses a testing ID (480).

**Filter name:** Kolmo

**Maintainer:** <full name>, <email address>, GitHub: https://github.com/adnaneben

**Repository:** https://github.com/adnaneben/kolmo. The filter plugin is in `integration/hdf5/` (`H5Zkolmo.c`, `H5Zkolmo.h`).

**Licence:** Apache-2.0 (https://github.com/adnaneben/kolmo/blob/main/LICENSE). The plugin and the library have no third-party codec dependencies.

**Description:**
Kolmo is a lossless, block-based, checksummed compressor written in C. It analyses each input and applies structure-aware transforms before entropy coding:
- byte-plane shuffling and delta prediction for numeric arrays;
- integer and floating-point series models;
- field transposition for delimited text.

On the numeric arrays we measured, at one thread, its output is 1.1-3x smaller than zstd level 3 with the shuffle filter (benchmark in the README linked below). Level 1 is the fastest, level 9 the smallest; level 10 is an archival mode. Each chunk is coded as one self-describing Kolmo frame: header, block index and checksums. Decoding needs no filter parameters.

**Filter parameters** (`cd_nelmts` = 4 after `set_local`; all are optional when the filter is set):

| index | meaning | default |
|---|---|---|
| `cd_values[0]` | compression level: 1 (fastest) to 9 (smallest), 10 = archival; 0 = the library's default (3) | 0 |
| `cd_values[1]` | element size in bytes; filled in by `set_local` from `H5Tget_size` of the dataset type when 0 | 0, then the type's size |
| `cd_values[2]` | threads per chunk; 0 = 1 | 0 |
| `cd_values[3]` | `cd_values` layout version, written by `set_local` (currently 1) | 1 |

`can_apply` rejects levels outside 0-10. The filter is built as a dynamically loadable plugin (`H5PLget_plugin_type` / `H5PLget_plugin_info`, `H5Z_class2_t`). It does not link libhdf5, so the same plugin file works with the C library and with h5py.

**Documentation:**
- Plugin usage and benchmarks: https://github.com/adnaneben/kolmo/blob/main/integration/hdf5/README.md
- Compression levels: https://github.com/adnaneben/kolmo/blob/main/docs/LEVELS.md
- Kolmo's container format: https://github.com/adnaneben/kolmo/blob/main/SPEC.md

We also plan to propose the filter to hdf5plugin (silx-kit) once the ID is registered.

Thank you,
<full name>
