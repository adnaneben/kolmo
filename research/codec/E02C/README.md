# E02C: fixed-entropy numeric residual experiment

Research only. No production code/default/API/format changes. Read `BASELINE.md` and root `AGENTS.md` first. Publish only to the authorized isolated branch; do not merge or release.

## Precisely what is executed

`build.py` verifies the full repaired `src/kolmo.c` and original `src/kgraph.inc` blobs, plus unchanged production/dependency trees relative to the validated revision. It generates a complete-source translation unit outside `src/`. A single final-serialization call in `kg_encode_num` is intercepted to copy the already-produced parameters, exact residual stream and head/tail stream. Prediction, GCD normalization, width choice and existing fixed leaf-coder bodies are not replaced. The generated diff is retained.

This is forced, explicitly typed numeric processing. It is not an automatically selected new production transform. Native full-pipeline timers include capture/build/copy overhead but NOT automatic file detection. Normal, uninstrumented Kolmo is built separately and its CLI outputs/plan prefixes are measured as product controls. CLI times include process startup and file I/O; do not equate them with in-process timings.

## Frozen matrix

Representations: exact upstream layout; global bitpacking; local bitpacking; patched local bitpacking; local byte-plane control. Local sub-blocks are 128 values. Global-versus-local is a separate segmentation change. Local plane widths use the existing 1/2/4/8-byte rounding; bit widths range from 0 to 64. Patched base width minimizes its native stored representation, NOT the downstream entropy-coded size; its full search cost is charged.

Backends: stored; existing Huffman-only (ID 13); existing rANS (ID 14); fixed zstd level 1 (ID 3). Each backend is attempted independently, with stored fallback when the coding attempt fails or does not shrink the represented bytes. Requested and actual outer codec are recorded, along with Huffman's internal stored/repeated/coded chunk counts. The Huffman implementation uses vendored zstd HUF; entropy-only is not dependency-free.

All metadata and head/tail bytes are stored and counted. There is no representation selector and no free oracle. No claim of whole-frame non-expansion or production integration is made.

## Research envelope E2C1

All multibyte integers are little-endian. Header is 64 bytes:

| Offset | Field |
|---|---|
| 0 | `E2C1` |
| 4..7 | version=1, representation ID, requested codec ID, actual codec ID |
| 8 | original byte length u64 |
| 16 | parameter length u64 |
| 24 | rest/head-tail length u64 |
| 32 | represented-stream decoded length u64 |
| 40 | coded payload length u64 |
| 48 | original residual-layout length u64 |
| 56 | original-input CRC32C u32 |
| 60 | CRC32C of header bytes 0..59 followed by body u32 |

Body: exact INT parameters, rest bytes, coded representation. Parameters retain input/output widths, flags, head, count, GCD, bias and seeds. Decoding verifies bounds/checksums, entropy-decodes, restores the exact residual layout, invokes the actual INT inverse, verifies the original checksum, then releases output. CRCs are not authentication.

Packing is LSB-first. Global/local bitpacking stores one width byte then ceil(count*width/8) bytes, with zero high padding bits. Patched blocks store width u8, exception count u16, low-bit packed values, then strictly ascending exception positions u8 plus positive canonical unsigned LEB128 high parts. Local planes store width u8 then the low-byte plane through highest plane. All IDs belong only to this research envelope, not CZN1/CZNS.

The adapter accepts 2/4/8-byte explicit numeric widths, at least 16 values, at most 1,048,576 values and 4 MiB complete inputs. Other lengths/identifiers/expansions outside the documented C bounds fail. The format is experimental, not a stable public API.

## Tests and metrics

`test_packing.c`: finite C round trips and truncated/mutated parser probes.
`test_reference.py`: independently written Python packing inverse against C outputs.
`probe test`: full-source representation/backend round trips plus truncation and checksum-repaired mutation probes under ASan/UBSan/LSan. Vendored archive is built as in existing repository sanitizer jobs; it is not fully instrumented.
`reference.py`: independent packing and numeric inverse, plus frame/input CRC verification for every measured configuration. Entropy decoding is reused from the C implementation; this is NOT an independent Huffman/rANS decoder.

Native C timers report full forced-numeric encode/decode wall and CPU duration, plus narrower representation+entropy intervals (these narrower intervals exclude framing/inverse and cleanup). Allocation/copy/CRC overhead is included in full pipeline measurements. Every configuration runs in a separate native process; peak RSS is that process's high-water mark including inputs, capture buffers, retained reference frame, exports, library/runtime overhead, not a pure coder scratch estimate. Warmup precedes recorded repeats. Configuration order is shuffled; repeats within a configuration are consecutive. Shared-runner timings are descriptive, not target-machine acceptance.

Two runs must reproduce input/capture hashes, all stored sizes and complete frame hashes. Confirmation on the same development data is not independent validation. Keep raw repetitions and all failures.

## Data and controls

Pinned Python dependencies provide bundled public datasets: sklearn digits; Plotly Gapminder; statsmodels Nile and sunspots. Numeric casts are checked exact; sunspot activity uses checked decimal-string scaling by ten. These derived NPY arrays reconstruct exactly, not the original source CSVs. Related columns are not independent datasets and the digits pixels can dominate aggregates. Synthetic cases use explicit PCG64 seeds. All inputs are DEVELOPMENT; no final holdout or deployment workload is claimed.

Report native stored/packed, fixed Huffman/rANS and zstd results separately. Compare a representation against the original layout under the SAME backend before attributing a gain. Ordinary Kolmo balanced/hc and system-zstd CLI results are separate complete-product size controls with their own timing regime. Preserve negative outcomes, including the preceding packing-before-zstd result.

## Run on Linux

```sh
python -m pip install -r research/codec/E02C/requirements.txt
python research/codec/E02C/test_reference.py
python research/codec/E02C/build.py --output /absolute/new/asan --cc clang --sanitize
python research/codec/E02C/build.py --output /absolute/new/release --cc gcc
python research/codec/E02C/run.py --probe /absolute/new/release/probe --output /absolute/new/results --repeats 5
```

Install liblzma development files, zstd CLI and supported C/C++ compilers first. Result directories must be new. `baseline_evidence.py` retrieves the previously completed validation artifacts using read-only `gh` access; artifacts have finite retention, so preserve receipts/logs independently. The workflow runs this check before measurements.

## Status and remaining gates

Local standalone packing and Python-reference checks are evidence only for those components. At initial publication, the full-source build, research sanitizer, corpus matrix and measured results are NOT yet established; check the E02C Actions run and artifacts rather than treating this README as a pass.

Independent validation/holdout, representative deployment workloads, cross-architecture/new-code checks, optimized native performance, bounded representation selection, production integration and M5 acceptance remain ahead. No default promotion follows automatically from a development size gain.
