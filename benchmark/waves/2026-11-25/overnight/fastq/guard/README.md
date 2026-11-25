# Repetitive four-line records under optional Ultra block alignment

Source `516e1dca02ac8165ed91ef5068af84e44335a109` descends from [integration #109](https://github.com/adnaneben/kolmo/pull/109). The optional encoder consults the record planner only when built with `-DKOLMO_FASTQ_RECORD_BLOCKS` and run with `KOLMO_BENCH_READ_CUTS=1` at Ultra. Unset and explicit zero both use the ordinary splitter.

On the 8,437,837-byte repetitive binding fixture, the old experimental build produced 29,050 bytes with the setting unset and 1,851,143 bytes with either zero or one. In the guarded build all three settings emitted the same 29,050 bytes. Every frame decoded back to the same input with the independent #109 CLI. The unchanged SHA-256 identifies the complete stock frame. [Before](original-before.json) · [After](original-control.json) · [Original public binding reproduction](../../bindings-sync/README.md) · [Source and binary hashes](manifest.json)

| Additional opaque variation | #109 stock | #109 optional | Guarded optional |
|---|---:|---:|---:|
| Repeated payloads with distinct record names | 1,021 B | 6,561 B | 1,021 B |
| Near-duplicate payloads with distinct names and unique fields | 1,233 B | 14,997 B | 1,233 B |

These two small regressions and the positive varying-width control are in the [committed test](https://github.com/adnaneben/kolmo/blob/516e1dca02ac8165ed91ef5068af84e44335a109/tests/test_optional_fq_record_blocks.py). A separate before-pass measured them with the unguarded #109 optional encoder and a stock reader. [Earlier frames and restores](older-regression-rows.json) · [Guard test receipts](regression.jsonl)

The guard counts repeated field bytes using a bounded hash table independently of record identifiers. Above one quarter of segment bytes it retains stock cuts. It also checks up to three 256 KiB windows using the built-in fast LZ probe. A window compressing to at most one eighth of its original size, or an inconclusive probe, also retains stock cuts. The varying-width, punctuation-only activation control still used 650,645 B versus stock’s 634,456 B (+2.55%). On a two-pair recheck, median encode time fell from 1,955.39 to 501.66 ms and stock-reader decode from 2,119.94 to 572.48 ms. All four restores were exact. This control demonstrates a size-for-speed exchange on nonrepetitive varied-width records. [Matched phase measurements](variable-width-timing.json) The size and time outcomes cover the recorded cases. A hash collision, a distant repeat or an unsampled anomalous region limits the classifier. The guard does not guarantee the smallest encoding for arbitrary files. File preservation is independently checked by full restore.

Run `python3 tests/test_optional_fq_record_blocks.py EXPERIMENT_CLI STOCK_CLI` in the source checkout and use the [capped panel](capped/README.md) for the two previously retained genuine inputs. The [frozen reader suite](frozen-cli.log) checked 274 CLI fixtures including streams. The allocator-failure convention of the LZ probe yields the original sample size; the guard treats that result conservatively.

On the two retained 8.65 MB inputs (T1, Ultra, 256 MiB memory budget), all eight measured frames reproduced the previous stock or aligned digest. The final setting used 18.54% and 9.98% fewer bytes and delivered 12.68× and 10.78× combined throughput over two matched pairs on the shared ARM64 host. The [codec-family sanitizer suite](sanitizer.log) and all ten checked-in cases across four encoder arms passed.
