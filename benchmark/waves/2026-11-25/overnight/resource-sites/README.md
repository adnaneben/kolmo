# Valid-input allocation failure qualification

The isolated tests exercised **174 named allocation failures at 47 distinct source locations across 18 API and CLI implementation paths** on Apple arm64, at candidate `ac10ce80048ab0664074f7ba031634055f4d2225`. Of those injections, 173 produced `KOLMO_ERR_ALLOC`. Failure to allocate the optional two-worker decode order returned the original 8,192 bytes successfully. Every injection left zero tracked container allocations outstanding and every clean retry reproduced the successful baseline. The ordinary and AddressSanitizer plus UndefinedBehaviorSanitizer runs produced identical per-trial records.

## Original 13 compression failures

The unmodified project counter was replayed on the original valid 8,192-byte input, two 4,096-byte blocks and one thread. At each budget the original counter keeps failing subsequent checked allocations. The result below identifies its first failure. The extra one-shot tests independently failed exactly one named allocation and measured output and retry.

| Budget | First allocation that failed | File and line at the pinned candidate |
| --- | --- | --- |
| 0, 1, 2 | Segment starts, ends, and kinds | src/core/compress.c line 96, calls 1 through 3 |
| 3, 4, 5, 6 | Plans, block starts, block plan IDs, raw starts | src/core/compress.c lines 115 through 118 |
| 7 | Per-segment block starts | src/core/blocks.c line 14 |
| 8, 9, 10 | Grow starts, block plan IDs, raw starts | src/core/compress.c lines 173, 175 and 177 |
| 11, 12 | Per-block output buffers and lengths arrays | src/core/compress.c line 227, calls 1 and 2 |

All 13 named failures left the destination sentinel bytes intact. The two existing decoder budgets mapped to the packed and raw offset arrays in src/core/decode.c, lines 59 and 60. Both returned the allocation error before changing destination bytes.

## Additional behavior

| Entry points | Observations from the valid fixtures |
| --- | --- |
| One- and two-thread compression and decode, FD/memory combinations, both FD-to-FD directions and range reads | The worker-start bitmap, file index, FD input block and packed-read buffer have direct receipts. The optional order array at src/core/decode.c line 407 fell back to ordinary decode, with identical output. |
| Archive creation, extraction of one member, extraction of all members and the CLI's selected-member walk | Covered the dedup table and its growth, metadata and extension storage, and extraction with consecutive and repeated references. Allocation failures during late archive creation changed 879 and 1,731 sentinel bytes on two fixtures. Two bulk-extract faults occurred after one validated callback. |
| Streaming encode, decode of a CZNS stream, a raw CZN1 member and an indexed CZN1 member | The 15 tested streaming encode failures left the 12-byte stream header on the destination FD. The exercised streaming decode faults emitted zero bytes. The indexed member exercised frame growth while reading its genuine extension. |

The 640-member archive used many distinct small files to reach both table-growth paths. Normal archive creation checked each member against its original bytes, and stream and frame controls checked decoded bytes. Retries used the same process with the allocation restored and the caller's input/output positions and capacity reset. The output columns separately count changed bytes in a prefilled memory destination, the length written to an initially empty FD, and successfully delivered archive callbacks. None of those columns is a transactional rollback guarantee.

The test-only source-copy instrumentation localizes the existing `KOLMO_TESTING` failure hook at checked calls and at ordinary allocation calls in six container source files plus the public API source. It records the lifetime of those successful allocations. Worker-private scratch, every platform allocator, and every other codec are outside that accounting. Of the 58 instrumented call locations, 11 had no matching input path in this bounded run. They include special video frame encode/decode allocations, specialized block scratch, empty projected-column allocation and secondary block-array growth. The zero-trial rows in the site receipts identify each one. The production source is unchanged.

## Reproduction and records

Run `./replay.sh /path/to/kolmo` in a writable copy of this directory with the pinned revision available locally. It builds the unchanged original-counter test, instruments a temporary copy and compares the ordinary and sanitized test records. `rows.csv` contains every fault. `site-receipts.csv` maps each instrumented call to source text and observed routes. `instrumented-sites.csv` gives the complete instrumentation inventory. `manifest.json` records input and source hashes, limits and compiler. `original.log`, `terminal.log` and `sanitized.log` record terminal results.
