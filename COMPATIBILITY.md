# Format compatibility

This is Kolmo's promise about the data it writes: what you compress today, every later release decompresses. The format
it covers is specified in [FORMAT.md](FORMAT.md).

## The promise

1. **Every later release reads version 12.** Every Kolmo release from 0.0.2 on decodes every frame and stream
   written by Kolmo 0.0.1 or any later release whose header says container version 12, to the same bytes. This
   holds:
   - for every level, every documented option, every platform and every thread count;
   - for files written by the CLI, the C library, the Python package (`kolmoc`) and the Rust crate (`kolmo`).

   0.0.1 and 0.0.2 themselves fell short of this in one case: they wrote table blocks holding a field over 65,535
   bytes and could not read them back (FORMAT.md 20.5). The data in those files is intact, and 0.0.3 and every later
   release read them.
2. **Identifiers keep their meaning.** No identifier of version 12 is ever given another meaning. That covers:
   - codec ids, plan types and tflags;
   - graph node ids, CM model nibbles, extension tags and checksum seeds.

   A retired identifier stays retired.
3. **New features cannot be misread by older releases.** A later release may add coders, transforms or fields. When
   it writes something a 0.0.2 decoder cannot read, the frame names it with an identifier that 0.0.2 refuses, or
   with a new container version. (0.0.3 writes no such frame: see *0.0.3 and earlier releases* below.) An older release then stops with an error. It never returns wrong bytes, and it
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

- **`tests/v12/` is the frozen corpus.** It holds 632 frames with their originals, and frames are added to it, never
  rewritten:
  - the top level: 84 frames in the six modes of 0.0.1;
  - `r0.0.2/`: 274 frames written by the 0.0.2 release binary. These cover every level 1 to 9 and ultra, the options
    that change the format (`--native-leaf`, `--typed-huf`, `--codec-level 1`, `--stream`), and inputs that reach
    every block codec 0.0.2 writes (0, 7, 12, 16-20, 24-26, 28, 31 and 32) and every tflag it writes by default
    (1, 2, 4-10, 12 and 14). The last 28 frames were added after the release, from the 0.0.2 tag binary:
    - eleven for codecs 25, 26 and 31 and tflags 2, 6 and 8, one for tflag 1 under plan 1 (quote-aware transpose), and two
      three-channel kimg images;
    - six for the table transform's numeric modes, formulas, group column and short rows, and six queryable archives
      (`kolmo --index`) with column-split tables (tflag 13);
    - two (`longfield.csv.*`) that 0.0.2 writes but cannot read itself. 0.0.3 and later read them (CHANGELOG,
      0.0.3), and the corpus tests check that they do.
  - `r0.0.3/`: 274 frames written by the 0.0.3 release binary from the same 42 inputs with the same options, so the
    two directories pair frame for frame. 185 of them are byte-identical to 0.0.2's; the others come from the encoder
    changes of 0.0.3 (CHANGELOG). The two `longfield.csv.*` frames no longer hold a table block.
  - **Not in the corpus:** table v1 (tflag 3), which no release writes (FORMAT.md 20.9 has test vectors instead),
    and the read-only codecs 5 and 29. The promise covers them all the same.
- **Four tests run in CI on Linux x86-64, Linux ARM64 and macOS, on every push and pull request** (all part of `make check`):
  - `test-v12-corpus` decodes each frame through the library: whole buffer, 1 and 4 threads, and a range read;
  - `tests/check_v12_frames.py` decodes each frame and stream through the CLI, and fails if any frozen frame file has
    changed;
  - `tests/format_reference.py` re-checks every frame against FORMAT.md with an independent Python implementation of
    the container layer;
  - `tests/coder_reference.py` decodes every block and graph leaf that uses Kolmo's own LZ and Huffman coders (KLZ,
    KLZH2, KLZMA, khuf, with krans), and the transforms of FORMAT.md section 20 over them, with an independent Python decoder written from
    FORMAT.md sections 14 to 21, `tests/bwt_reference.py` the block-sorting coder of section 22, `tests/kimg_reference.py` kimg (section 23), and
    `tests/cm_reference.py` context mixing (section 24, the first 1024 bytes of each corpus stream; `make
    check-cm-reference` decodes them whole).
- **Fuzzing and hardening tests** (`test-decoder-hardening`, `test-harden`) check that damaged input is refused.

## 0.0.3 and earlier releases

Checked on 2026-11-25 with the binaries built from the tags `v0.0.1` and `v0.0.2` and the 0.0.3 release binary.

**What 0.0.3 writes that 0.0.2 refuses:**

- **Frames: nothing.** 0.0.3 uses no codec id, plan type, tflag, node id or extension tag that 0.0.2 does not read,
  and no new container version. The 0.0.2 and the 0.0.1 command lines decode all 274 frames of `tests/v12/r0.0.3/`
  to their originals, the queryable archives and the `--stream` file included.
- **One case in the bindings: a pipe over 256 MiB.** The 0.0.3 command line writes such a pipe as a `CZNS` stream of
  256 MiB segments (0.0.2 wrote one frame). The format is that of `--stream`, which 0.0.1 and 0.0.2 already wrote.
  - The 0.0.1 and 0.0.2 command lines and `kolmo_stream_decompress` read it (checked on a 300 MiB pipe).
  - The 0.0.1 and 0.0.2 **Python and Rust bindings refuse every `CZNS` stream** with "corrupt or unsupported
    input", as they refuse their own release's `--stream` output. They return an error, never wrong bytes. The
    0.0.3 bindings read streams.
  - To write one frame from a pipe, compress a file instead, or keep the pipe under 256 MiB.

**What 0.0.3 no longer writes:** table blocks with a field over 65,535 bytes. Such a block is coded without the table
transform, so 0.0.1 and 0.0.2 read every table 0.0.3 writes.

**What 0.0.2 accepted and 0.0.3 refuses.** No release ever wrote any of these, so no file written by Kolmo is
affected (CHANGELOG, 0.0.3):

- a context-mixing graph leaf in a frame whose header mode is not ultra (FORMAT.md 7.4);
- a stored block whose plan is not `0,0,0,0` or whose tflag is not 0 (FORMAT.md 7.1);
- codecs 14 and 27 as block codecs (FORMAT.md 8.1);
- an audio stream (codec 19) whose chunk is over 2^18 sample frames (SPEC.md, *Audio blocks*);
- bytes after a member that start no member (FORMAT.md 10.1); 0.0.2 ignored them after a stream;
- a frame over the memory limit because of a nested video frame, which 0.0.2 admitted;
- in `kolmo_query_run`, `kolmo_index_info` and the archive calls: a header the decoder itself refuses.

**What the 0.0.3 command line refuses on purpose:** `kolmo -d`, `-t`, `-l` and `--range` on a `kolmo archive` file
(use `kolmo extract`). The C library still decodes that frame, as every version-12 reader does.

**Files of several members** (FORMAT.md 10.1). 0.0.3 never writes one by itself: they come from appending outputs
(`cat a.kolmo b.kolmo`, `kolmo >> file`). 0.0.3 decodes every member. **0.0.2 and 0.0.1 stop after the first `CZNS`
stream and exit 0, without the rest of the data**, and refuse a frame followed by a stream. Read such files with
0.0.3.

## Rules for changing the format

1. **A new identifier is claimed in FORMAT.md first** (codec id, plan type, tflag, node id, model nibble or extension
   tag), in the same change that writes it.
2. **It must be one that every earlier version-12 decoder refuses.** Unknown codec ids, plan types, tflags and node
   ids are refused today. Reserved header and index bytes, and the reserved bytes and `segment_bytes` of the `CZNS`
   stream header, are ignored, so they must never carry something a decoder needs.
3. **A change that would make version-12 frames decode differently is not allowed.** This includes a change to a
   coder's decoder, even one that leaves the frozen corpus passing. If a decoder has a bug that returns wrong bytes,
   the fix restores the bytes the original was compressed from, and the change log says so.
4. **Each release adds its frames to the corpus.** Releasing (RELEASING.md) runs `tests/make_v12_frames.py` with the
   release binary, which adds `tests/v12/r<version>/` and its lines at the end of `tests/v12/MANIFEST`;
   `tests/check_v12_pinned.py --write` then pins the new files. Existing frames are never regenerated.
5. **A new container version is the last resort.** It is used only when a change cannot be expressed as a refused
   identifier. Releases keep reading version 12.
