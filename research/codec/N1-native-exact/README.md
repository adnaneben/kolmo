# Native, exact-header OpenZL comparator

This research-only adapter closes the Python-wrapper/header-as-numeric comparison gap recorded by N1-exact and the P2-E17 audit. It is a custom typed OpenZL encoder front end, NOT a new Kolmo codec or proof of leadership. It uses the unmodified pinned OpenZL library and only standard wire-format nodes, including a serial split into original prefix / typed numeric payload / original tail. No application parser or custom node is required for decoding.

Pin OpenZL commit `32246b48faee46807f84183dac4db479089f5445` and its exact dependencies (the original N1 artifact contains the source plus tree hashes). The encoder supports bounded, explicit-endian integer NPY and PCM16 RIFF/WAV. Unsupported inputs explicitly fail; never relabel them as typed successes. Payload bytes are never converted through floating point or text. Shape metadata, quote spelling, padding, original ordering and tail bytes are all preserved. Frames carry normal OpenZL checksums; output files must not exist.

## Build and test

```
cmake -S research/codec/N1-native-exact -B /tmp/n1native \
  -DOPENZL_SOURCE=/absolute/path/to/verified/openzl -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/n1native --target native_exact native_exact_san stock_reader parse_cli -j2
PARSER=/tmp/n1native/parse_cli python -m unittest discover \
  -s research/codec/N1-native-exact -p test_parser.py -v
ADAPTER=/tmp/n1native/native_exact STOCK_READER=/tmp/n1native/stock_reader \
  INPUTS=/absolute/path/to/R3/prepared/inputs python -m unittest discover \
  -s research/codec/N1-native-exact -p test_roundtrip.py -v
```

Repeat the second suite with `ADAPTER=/tmp/n1native/native_exact_san`, `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1`. The library itself is not sanitizer-instrumented in this build; the new parser/wrapper is. Parser-only Clang instrumentation can be built directly from `parse_cli.cpp`.

```
/tmp/n1native/native_exact c 9 exact input.npy output.zl
/tmp/n1native/stock_reader output.zl restored.npy
cmp input.npy restored.npy
```

Modes `whole` and `serial` are diagnostic rival controls under the same library/build, not new Kolmo candidates. `whole` treats all file bytes as fixed-width values, including header; this is byte-lossless but is not payload-specific typing. `exact` parses first and splits the original bytes. Neither is guaranteed to be smaller than the other. Use both appropriate rival configurations rather than picking whichever is easiest to beat.

## Reproduce evidence

`bench.py` requires exactly the eight hash-recorded prepared R3 NPY inputs. It writes complete frames, raw subprocess wall/CPU samples, receipt and medians to a NEW directory. Encoding includes native typing, graph construction, compression, process startup and I/O. Decoding uses a separately compiled stock public API reader, independent from the new parser. No Python program runs inside timed native codec processes.

```
python research/codec/N1-native-exact/bench.py --adapter /tmp/n1native/native_exact \
  --reader /tmp/n1native/stock_reader --inputs /path/to/eight/R3/inputs --output /tmp/new-native-run
python research/codec/N1-native-exact/audit.py
```

The second command audits the compact committed original measurements, not a new run. See PROTOCOL.md, RESULTS.md and EVIDENCE.json. Source archives, binaries, 56 frames and detailed logs are separate artifacts, not all Git-tracked. The compact measurement ledger and reproduction code are Git-tracked. No runner job, main-branch write, default change, merge or release is part of this continuation.
