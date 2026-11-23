# Format compatibility

This is Kolmo's promise about the data it writes: what you compress today, every later release decompresses. The format
it covers is specified in [FORMAT.md](FORMAT.md).

## The promise

1. **Every later release reads version 12.** Every Kolmo release from 0.0.2 on decodes every frame and stream
   written by Kolmo 0.0.1 or any later release whose header says container version 12, to the same bytes. This
   holds:
   - for every level, every documented option, every platform and every thread count;
   - for files written by the CLI, the C library, the Python package (`kolmoc`) and the Rust crate (`kolmo`).
2. **Identifiers keep their meaning.** No identifier of version 12 is ever given another meaning. That covers:
   - codec ids, plan types and tflags;
   - graph node ids, CM model nibbles, extension tags and checksum seeds.

   A retired identifier stays retired.
3. **New features cannot be misread by older releases.** A later release may add coders, transforms or fields. When
   it writes something a 0.0.2 decoder cannot read, the frame names it with an identifier that 0.0.2 refuses, or
   with a new container version. An older release then stops with an error. It never returns wrong bytes, and it
   never silently skips data. Frames that use no new feature stay readable by older releases.
4. **Reading version 12 is never removed.** Version 12 support is not dropped in any release, 1.0 and later included.
   If a future format ever replaces it, releases keep decoding version 12.
5. **Damaged data is reported, not returned.** Every block carries a checksum and the header and index carry a CRC.
   A decoder returns an error rather than bytes that fail them. This is true of every release that keeps this promise.

## What is not promised

- **Identical compressed bytes across releases.** A later release may compress the same input differently, usually
  smaller. Within one release the output is deterministic:
  - the same on every platform and thread count, and with any memory limit;
  - checked by `benchmark/determinism/check.py` in CI.

  Do not use a compressed file's hash to identify its content across releases. Hash the original.
- **Older releases reading newer files.** They refuse what they do not know (promise 3). They are not guaranteed to
  read it.
- **Frames of Kolmo 1.x and the 1.2.0 release candidates** (container versions 1 to 11, library codecs). Releases
  from 0.0.1 on refuse them with `KOLMO_ERR_LEGACY`. Decompress them with the release that wrote them, then compress
  again.
- **Experimental builds.** Builds made with `-DKOLMO_FRONTIER` or the `experimental-*` targets write codec ids 8-11 and
  transform flags that are not part of version 12.
- **The command-line options and the library API.** These follow the version number (0.0.x may change them; see
  CHANGELOG.md). This document is about the bytes of compressed data only.

## How the promise is checked

- **`tests/v12/` is the frozen corpus.** It holds 347 frames with their originals, and frames are added to it, never
  rewritten:
  - the top level: 84 frames in the six modes of 0.0.1;
  - `r0.0.2/`: 263 frames written by the 0.0.2 release binary. These cover every level 1 to 9 and ultra, the options
    that change the format (`--native-leaf`, `--typed-huf`, `--codec-level 1`, `--stream`), and inputs that reach
    every block codec 0.0.2 writes (0, 7, 12, 16-20, 24-26, 28, 31 and 32) and every tflag it writes by default
    (1, 2, 4-10, 12 and 14). The last 17 frames were added after the release, from the 0.0.2 tag binary. Eleven are for
    codecs 25, 26 and 31 and tflags 2, 6 and 8. Six cover the table transform's numeric modes, formulas, group column
    and short rows.
  - **Not in the corpus:** the column-split table (tflag 13), which is written only on request (FORMAT.md 8.3), and
    the read-only codecs 5 and 29. The promise covers them all the same.
- **Four tests run in CI on Linux x86-64, Linux ARM64 and macOS, on every push and pull request** (all part of `make check`):
  - `test-v12-corpus` decodes each frame through the library: whole buffer, 1 and 4 threads, and a range read;
  - `tests/check_v12_frames.py` decodes each frame and stream through the CLI, and fails if any frozen frame file has
    changed;
  - `tests/format_reference.py` re-checks every frame against FORMAT.md with an independent Python implementation of
    the container layer;
  - `tests/coder_reference.py` decodes every block and graph leaf that uses Kolmo's own LZ and Huffman coders (KLZ,
    KLZH2, KLZMA, khuf, with krans), and the text transforms over them, with an independent Python decoder written from
    FORMAT.md sections 14 to 21.
- **Fuzzing and hardening tests** (`test-decoder-hardening`, `test-harden`) check that damaged input is refused.

## Rules for changing the format

1. **A new identifier is claimed in FORMAT.md first** (codec id, plan type, tflag, node id, model nibble or extension
   tag), in the same change that writes it.
2. **It must be one that every earlier version-12 decoder refuses.** Unknown codec ids, plan types, tflags and node
   ids are refused today. Reserved header and index bytes are ignored, so they must never carry something a decoder
   needs.
3. **A change that would make version-12 frames decode differently is not allowed.** This includes a change to a
   coder's decoder, even one that leaves the frozen corpus passing. If a decoder has a bug that returns wrong bytes,
   the fix restores the bytes the original was compressed from, and the change log says so.
4. **Each release adds its frames to the corpus.** Releasing (RELEASING.md) runs `tests/make_v12_frames.py` with the
   release binary, which adds `tests/v12/r<version>/`. Existing frames are never regenerated.
5. **A new container version is the last resort.** It is used only when a change cannot be expressed as a refused
   identifier. Releases keep reading version 12.
