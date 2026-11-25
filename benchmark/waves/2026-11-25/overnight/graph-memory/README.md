# B33 · Peak live graph-decoder memory

The highest observed graph-owned allocation total was **67,317,232 bytes** on the 23,938,176-byte OWID energy array at level 9. The same data at level 3 used eight graph blocks and peaked at **8,431,568 bytes** in one block. The levels produced different stored sizes. On the level 9 file, the unmodified CLI reached **83.36 MiB** of process RSS. All 438 full-frame restores matched their original bytes by SHA-256, and every traced graph finished with zero owned allocations.

| Pre-existing complete frame | Original / frame bytes | Graph blocks | Peak graph-owned bytes | Stock / instrumented peak RSS, MiB |
|---|---:|---:|---:|---:|
| OWID energy binary64, L3 | 23,938,176 / 1,538,698 | 8 | 8,431,568 | 15.09 / 15.14 |
| OWID energy binary64, L9 | 23,938,176 / 1,367,054 | 1 | 67,317,232 | 83.36 / 83.44 |
| Wide CSV, L3 | 8,227,871 / 1,163,263 | 8 | 2,141,430 | 13.97 / 14.05 |
| Wide CSV, L9 | 8,227,871 / 943,475 | 1 | 16,368,886 | 33.67 / 33.25 |
| Population integer series, L3 | 151,152 / 39,460 | 1 | 524,782 | 2.66 / 2.72 |
| BF16 tensor, L3 | 10,485,880 / 6,992,158 | 8 | 4,194,836 | 9.64 / 9.70 |
| Retinal image, L3 | 5,972,780 / 1,414,933 | 6 | 2,097,646 | 13.66 / 13.73 |

## What was live at the peak

On the larger OWID level 9 graph, 33,558,528 B belonged to decoded leaf buffers, 33,685,504 B to inverse-node buffers, 72,192 B to DEC64 column descriptors, and 1,008 B to graph metadata. All four amounts coexisted at the peak. Per-category maxima can occur at different times. For example, leaf ownership alone peaked at 33,689,856 B earlier.

The wide level 9 CSV contained 315 fused DECIMAL nodes, five CONCAT nodes and one FIELDS node. At the graph-owned peak, leaf buffers accounted for 7,872,768 B, inverse buffers 8,388,608 B, FIELDS cursor-position scratch 2,528 B, and metadata 104,982 B. Its CONCAT slots simultaneously viewed 6,140,236 unique logical bytes within 7,872,512 B of owned backing allocations. The decimal cursors retained 5,868,717 B of logical spans for FIELDS. These observations describe references to the already counted storage. The graph-owned peak counts each allocation once.

All seven graph node types occurred across the corpus. Every observed DECIMAL node fused into FIELDS. The independent DECIMAL string reconstruction path and DEC64 dictionary-value scratch had no qualifying example in this set. DEC64 column scratch did occur. The experiment cannot establish their additional demand on other legitimate inputs.

## Accounting and verification

The measuring copy came from pinned main a1d39559686a9ec17c8614b9154a64d7b5b98672. The script checks the original graph.c SHA-256 before inserting test counters only in a separate build. The full stock CLI and probe CLI both used the ordinary file decoder, one thread and --memlimit 512M. A separate 128 MiB test ceiling applied to live requested bytes owned by one graph. Every observed graph remained within it. This is an observation of the corpus. The format's existing limit of four times the block size plus 1 MiB applies to logical stream and leaf sizes. It is a different measure from the combined owned capacities reported here.

The counters follow graph.c's actual malloc, calloc, realloc and free while kg_decode executes. A buffer contributes its allocated capacity, including the kbuf growth rounding, during ownership. A successful realloc replaces the old allocation in this accounting. The caller's compressed frame and final output are excluded. The graph's private reconstruction buffer is included while it coexists with its input leaves, even when its data are about to be copied to the caller. The graph metadata arrays and direct temporary allocations made by inverse nodes are also included. The instrumentation verifies free and resize identities and requires zero remaining bytes and allocation count at graph exit.

The CONCAT counter unions live non-owning byte ranges after each creation and finds their separately allocated owners, deduplicating the capacity of each backing allocation. The FIELDS/DECIMAL counter measures the byte lengths referenced by fused cursors, releasing that counter when FIELDS consumes them. Those lengths can overlap the CONCAT observations. Decoder workspaces private to the individual leaf codecs, outer staging buffers, allocator-internal realloc copies, stack, allocator headers and instrument overhead fall outside the graph-owned byte count. Process peak RSS includes all resident sources together. The test makes no claim that RSS can be partitioned by subtracting the graph count.

The 66 frozen cases are all directly indexed graph-containing frames in the pinned tests/v12/MANIFEST. Seven existing R30 review artifacts add real corpus workloads, multiple graph blocks and fused cursor and alias coverage. No inputs or frames were generated for this measurement. An independent read of the frame index and graph program checked the counts of all seven operations on every traced block. Three fresh stock processes and three fresh measuring processes per file ran in seeded interleaved order on an Apple M5/macOS 27.2. Darwin wait4 on each posix_spawn child supplied peak RSS in bytes. Each instrumented repetition had identical ownership figures, and both executables restored every source hash. The complete matrix covers 73 frames and 99 graph blocks per instrumented pass.

## Replay

Use a Kolmo checkout containing the pinned commit and its matching stock CLI. REVIEW is the historical R30 review campaign root with corpus/ and review40b/work/R30/enc/. fixture-hashes.json covers all inputs and archived frames. The historical paths can be reconstructed from equivalent copies with matching digests.

```sh
sh build.sh "$REPO" "$EMPTY_BUILD_DIR"
python3 measure.py --corpus "$REPO/tests/v12" --review "$REVIEW" --stock "$PINNED_CLI" --probe "$EMPTY_BUILD_DIR/build/kolmo" --out "$OUTPUT_DIR"
```

block-allocations.csv gives the observed peaks, the simultaneous category amounts at each total peak, retained view and cursor sizes, node counts and zero end balances. process-rss.csv has each restore's measured RSS and actual SHA-256. summary.csv contains all 73 files, including peak and median RSS for both executables. verified.json identifies the two measured binaries, limits, and tested counts. The data establish memory behavior for these valid frames. Production code and defaults were unchanged.
