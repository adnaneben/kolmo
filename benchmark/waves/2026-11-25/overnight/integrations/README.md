# Installed integrations on Apple silicon

The local build used combined review candidate `ac10ce80048ab0664074f7ba031634055f4d2225`. It followed the native source synchronization used by the hosted wheel workflow. All Python imports came from isolated environments outside the project. The workflow used locally cached third-party packages, plus tag-pinned Zarr 2 sources. Hosted wheel qualification and public package registries are separate.

| Runtime | Operation | Result |
| --- | --- | --- |
| CPython 3.9.6 and 3.12.8, native `kolmoc` 0.0.2 | Install the same arm64 wheel into two isolated environments and run the binding tests | Both imported their installed library and passed. |
| HDF5 2.2.0 C loader | Create a chunked file, write datasets, close, reopen, inspect filter metadata and restore values | Passed the native filter suite. |
| h5py 3.14.0 / HDF5 1.14.6 / NumPy 2.0.2 and h5py 3.16.0 / HDF5 2.0.0 / NumPy 2.5.5 | Load the same macOS plugin, store five dtype families and check values, dtype, filter settings and partial reads | Passed on both bundled HDF5 ABIs. Separate processes reopened an HDF5 file and verified that the filter was applied to its first chunk. |
| NumCodecs 0.17.1 and adapter 0.0.1.dev0 | Discover codec ID `kolmo` through the installed entry point, write a frame to disk and decode in a fresh process | Recovered all 190,836 input bytes. |
| Zarr 3.4.2 / NumCodecs 0.17.1 | Write stores in formats 2 and 3, then reopen using only persisted metadata | Both passed complete-array and slice checks. |
| Zarr 2.16.1 / NumCodecs 0.12.1 on CPython 3.9.6 | Write and reopen a v2 store in the v2 runtime. Read its store in Zarr 3.4.2 and read the Zarr 3.4.2 v2 store in Zarr 2.16.1 | All three reads recovered the same dtype and full byte sequence. |
| c-Blosc2 3.8.0 | Static registration, shared plugin loading through the library path, and codec lookup through the Python locator | All passed. Separate processes wrote and reopened a 1,023-byte file with codec ID 244 and recovered all six chunks (196,608 bytes). |

The deterministic NumCodecs, h5py and Zarr cross-process fixture had SHA-256 `6a5a9aee8637d33e35ff78d7c8fe2a094bab214b8f475f694f8ba426ebe13508`. `disk-inventory.json` records complete file sizes and hashes. Each directory-store hash covers a sorted list of its relative filenames and individual file hashes. Zarr 2.16.1, fasteners 0.19 and asciitree 0.3.3 came from verified upstream Git blobs. The build-time version module contains the checked Zarr tag. See `zarr2-source-manifest.json`.

## Wheel payload repair

The original arm64 wheel contained 18 compiler objects in `_kolmo_build/`. A separate review commit, `eecf57cba456e35e70100d44110a89f91ec802b4`, moves intermediate files to an automatically cleaned temporary directory and adds a CI check against build objects in wheel artifacts. The fixed build reused the original build directory, exercising cleanup of old in-package objects.

| Wheel | Tag | Complete wheel bytes | Compiler object bytes before ZIP compression |
| --- | --- | ---: | ---: |
| Original kolmoc 0.0.2 | py3-none-macosx_11_0_arm64 | 1,585,075 | 1,612,128 |
| Fixed kolmoc 0.0.2 | py3-none-macosx_11_0_arm64 | 772,246 | 0 |
| NumCodecs adapter 0.0.1.dev0 | py3-none-any | 7,682 | 0 |

The wheel shrank 812,829 bytes (51.28%). All retained members had the same SHA-256, apart from the packaging inventory `RECORD`. The installed arm64 Mach-O library has SHA-256 `cb90c0168bfefd583243c82fa78dc1a4043bf31bf365c4f110b46db727eea0c5`, minimum deployment target macOS 11.0 and one system dependency, libSystem. The version tag and load commands were inspected. The executing host was Apple M5. `wheel-inventory.json` contains the wheel hashes, member hashes, tags and object totals.

The release workflow has separate Linux architectures. This run exercised macOS arm64 only. HDF5 filter ID 480 and Blosc2 codec ID 244 are project placeholders. The runtime for the python-blosc2 distribution was unavailable in the local package cache. The Python locator used by c-Blosc2 was exercised. These tests produced private-ID local files.

## Canonical project notices

A second packaging commit, `323049a37f888d8c64780da8a6223dfee3e944fd`, synchronizes the repository LICENSE and NOTICE into the standalone Python project and checks artifact contents in CI. A clean wheel build from its isolated 691,024-byte source archive produced a 777,110-byte wheel. Each installed notice matched the canonical source. The Mach-O library retained the digest shown above. Both archive paths and file digests are listed in `notice-inventory.json`. The archive checks correctly failed on the original wheel and on individually altered notices in test archives.

The adapter and Zarr reference suites passed all 207 tests on Python 3.12. Stock CLI and wheel frames matched on all ten levels, including Ultra. On the 788,907-byte structured CSV binding fixture, the opt-in all-options CLI used different frames at L1 and L2. Both builds recovered both sets of frames. A stock wheel compared to an all-options CLI is a different build-settings comparison. `qualification.log` contains both the initial mismatch and the matched-settings rerun.

## Replay

Build the Python packages using the exact source revision, an isolated Python 3.12 build environment with setuptools 84.2.1, wheel 0.48.0, and pip 26.2.2. On macOS arm64, the environment included `MACOSX_DEPLOYMENT_TARGET=11.0`, `ARCHFLAGS="-arch arm64"` and `_PYTHON_HOST_PLATFORM=macosx-11.0-arm64`.

```sh
bash bindings/python/sync-native.sh
python -m pip wheel --no-deps --no-build-isolation -w wheelhouse bindings/python integration/numcodecs-kolmo
make -C integration/hdf5 test
make -C integration/blosc2 test
```

Install the two wheels into each isolated runtime. Run `file-check.py write FAMILY PATH` and `file-check.py read FAMILY PATH` in separate invocations with `python -I` outside the source tree. Supported test families are `numcodecs`, `hdf5`, `zarr2` and `zarr3`. Supply the plugin build directory as `HDF5_PLUGIN_PATH` to the HDF5 processes. Compile `blosc2-file.c` against the repository plugin header, its built static library and the installed c-Blosc2 library, then run the resulting executable with `write PATH` and `read PATH` in separate invocations. The logs group the command outputs by integration and build.
