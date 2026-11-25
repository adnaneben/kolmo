# Security policy

Kolmo decodes compressed data that may come from anyone. A crash, hang, out-of-bounds read or write, unbounded
allocation, or a frame that decodes to the wrong bytes without an error is a security bug.

## Reporting a vulnerability

Report it privately through GitHub: **[Security → Report a vulnerability](https://github.com/adnaneben/kolmo/security/advisories/new)**
on this repository. Please do not open a public issue, pull request or discussion for it.

Include what you can of:
- the Kolmo version (`kolmo -V`) or commit, the platform, and whether you used the CLI, the C library, the Python
  package `kolmoc` or the Rust crate `kolmo`;
- the input that triggers it (attach the file to the advisory), and the command or call;
- what happens (sanitizer report, stack trace, wrong output) and what you expected.

What to expect:
- an acknowledgement within 7 days;
- an assessment, and a fix or a plan with a date, within 30 days;
- a fixed release, then a published GitHub security advisory (with a CVE where it applies) crediting you unless you
  ask not to be named. We aim to publish within 90 days of the report, sooner once a fix is out.

## Supported versions

Kolmo is pre-1.0 and fixes go into the latest release only.

| Version | Supported |
|---|---|
| 0.0.2 (latest) | yes |
| 0.0.1 and earlier, 1.0.0 and 1.0.1 (yanked) | no: upgrade to the latest |

The frame format (container version 12) is stable (COMPATIBILITY.md), so upgrading never loses the ability to read
existing frames.

## Decode work and memory

What a decoder bounds for untrusted frames, and what the caller must bound itself:

- **Output size**: `kolmo_decode_limits.max_output_bytes` (and `max_block_bytes`, `max_blocks`), checked from the
  header before anything is allocated. A frame's size says nothing about its decoded size (a 29-byte block can code
  16 MiB), so set it.
- **Memory**: `max_memory` (CLI `-M`). Every frame, video frames included, is admitted from the figure its header
  declares before any allocation, and each block (and each extra FASTQ model job) is reserved with the memory
  governor. The library never calls `abort()` or `exit()`; an allocation that fails returns `KOLMO_ERR_ALLOC`.
- **Work (CPU)**: not bounded inside the decoder. Decoding time follows the decoded bytes of each coder family:
  hundreds of MB/s and up for Kolmo's LZ and Huffman/rANS coders, tens to hundreds of MB/s for BWT and KLZMA, and
  about 0.5 to 10 MB/s per core for context mixing and the other model coders (ultra, and some blocks at levels 8 and
  9). A valid 5,978-byte context-mixing frame decodes 16 MiB of zeros in about 21 s. To bound it:
  - call `kolmo_frame_cost` before decoding and refuse a frame whose `bytes[KOLMO_COST_MODEL]` (or
    `bytes[KOLMO_COST_SORT]`) exceeds your budget; nothing is decoded or allocated beyond the index;
  - the header's mode is now a cost signal: a frame of modes rt to max (levels 1 to 9) cannot hold a context-mixing
    graph leaf (FORMAT.md 7.4), so its model bytes come only from block codecs the index names;
  - run decoding of untrusted input under a time limit of your own (a worker process or thread you can stop) if a
    budget by bytes is not enough.

  Not done, and why: a work counter inside the decoders (stopping a context-mixing stream part way) would have to be
  threaded through every coder's inner loop and checked per bit, for a bound the byte count already gives before
  decoding starts, since each class decodes at a steady rate per byte. It may come later as a decode option; the
  frame format does not need to change for it.

## Fuzzing

The decoders are fuzzed continuously, with the targets in `fuzz/` built by `fuzz/build.sh`:

- **Every pull request and push to `main`**: libFuzzer under AddressSanitizer and UndefinedBehaviorSanitizer, 2
  minutes per target (`.github/workflows/fuzz.yml`), and a bounded ASan/UBSan smoke run of every harness in
  `ci.yml`.
- **Nightly**: 30 minutes per target, continuing from the corpus the earlier runs grew.
- **Targets**: the frame decoder, the limited and range decoders, the streaming decoder (with and without limits),
  round trips at every level, Kolmo's own coders on raw streams, the block and leaf decoders called directly, the
  container extensions (KEXT, IDX1, DDP1) and the `--target` parser.
- **Seeds**: frames of every mode (`fuzz/corpus`) and the frozen frames of every release (`tests/v12`).
- **OSS-Fuzz**: an application is prepared in `packaging/oss-fuzz/` but not yet submitted.

Findings from fuzzing are fixed before a release, and each fix adds the input to the regression tests.

## Verifying release files

Release files are signed with GitHub build provenance. See [RELEASING.md](RELEASING.md#verifying-release-files):

```sh
gh attestation verify kolmo-X.Y.Z-linux-x86_64.tar.gz --repo adnaneben/kolmo
```
