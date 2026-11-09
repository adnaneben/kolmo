# Independent ratio and lzbench delivery

Existing Kolmo ultra achieved a local ratio win on the board-linked tar: 37,956,550 bytes versus ZPAQ5's 40,470,202, a 6.21% reduction. The new independently implemented byte-context candidate separately improved ratio over KRC1, but remains substantially slower and dominated by several established codecs. Retain the current hybrid/fast/GPU paths and keep the new independent model as a research reference. This is not an official public-board placement.

## Executive result

| Milestone | Evidence | Decision |
|---|---|---|
| Start with real lzbench | Standalone KRC1 and KBC1 callbacks installed in the pinned local build; original 108-row Silesia baseline retained | Use lzbench results for comparisons, with explicit input and resource budgets |
| Improve independent ratio | Silesia 1.107× → 2.733×; Canterbury 1.175× → 3.916×; frozen Calgary14 1.172× → 2.784× | Retain the independent byte-context model as a separate experimental format |
| Preserve previous improvements | Earlier source archives, KRC1/Metal implementations and raw evidence retained | Do not replace the faster existing paths with the slower model |
| Challenge leading codecs | Existing ultra beat ZPAQ5's verified board-tar size by 6.21%; KBC1 remains behind | Retain the local hybrid result with its memory tradeoff; no public-board rank follows from a Mac result |

Implementation is in `src/experimental/kbc.c`, documented in [KBC.md](../../src/experimental/KBC.md). It chooses the smallest complete payload from raw storage, the preserved KRC1 codec, or an independently written byte-context arithmetic stream. Context modeling and arithmetic coding are established techniques, not claimed inventions. There is no external compression backend, pretrained model or corpus-specific dictionary in KBC1. Mode selection, allocation, model initialization, metadata and checksums are all timed by the lzbench callback.

[PROVENANCE.md](../../src/experimental/PROVENANCE.md) credits the known `lowbias32` hash primitive used to index the context tables, alongside the established coding mathematics. Do not describe every line or primitive as a new invention.

## Complete corpus results

All rows below use one internal CPU thread and one lzbench worker. Compression ratio is original bytes / complete compressed bytes, higher is better. Speeds are MB/s of original data. Aggregate speeds are total bytes / sum of per-file median times; ratios are totals, not arithmetic means of individual ratios. Each row/file requests at least three compression and decompression iterations with median reporting.

| Complete Silesia, 12 files | Ratio | Encode MB/s | Decode MB/s |
|---|---:|---:|---:|
| Independent KRC1, preserved | 1.107 | 188.27 | 1856.49 |
| Independent KBC1, new | 2.733 | 10.53 | 6.66 |
| Normal Kolmo fast | 3.623 | 253.93 | 1751.84 |
| LZ4 1.10.0 | 2.101 | 1000.42 | 5898.66 |
| Zstandard 1.5.7 level 3 | 3.205 | 531.08 | 2141.17 |
| Zstandard level 9 | 3.587 | 122.06 | 2243.14 |
| Zstandard level 19 | 4.007 | 5.59 | 2164.21 |
| Brotli 1.2.0 level 8 | 3.711 | 36.28 | 777.13 |
| libbsc 3.3.12 m0/e1 | 4.501 | 50.69 | 50.77 |
| PPMd8 26.03 level 6 | 4.296 | 21.04 | 19.51 |

KBC1 reduced complete Silesia output from 191,481,384 to 77,560,246 bytes, a 59.49% saving over KRC1. Encoding became about 18 times slower and decoding about 279 times slower. Zstandard 3, Kolmo fast, Brotli 8, libbsc and PPMd8 all beat this candidate simultaneously in ratio, encoding and decoding speed. The ratio improvement is real; the candidate is not on this measured three-metric frontier.

| Corpus | KRC1 bytes | KBC1 bytes | Saving | KBC1 ratio | KBC1 encode/decode MB/s |
|---|---:|---:|---:|---:|---:|
| Canterbury, all 11 files | 2,392,201 | 717,723 | 70.00% | 3.916 | 12.23 / 7.45 |
| Calgary, standard 14 files | 2,680,190 | 1,128,321 | 57.90% | 2.784 | 10.27 / 6.78 |

The Calgary result is final validation for this cycle: its hashes and the standard fourteen-file subset were frozen before candidate performance evaluation, and no model change followed the first three-file development probe. The downloaded archive also contains paper3–paper6; these four files are excluded from the standard aggregate. Calgary is a public historical benchmark, not a claim of globally unseen data. See the [corpus descriptions](https://corpus.canterbury.ac.nz/descriptions/), `validation-inputs.json`, `validation.csv` and `summary.csv`.

On the previously used OWID table, KBC1 ratio is 3.512 versus Kolmo fast 10.693. On the packed signal file it is 1.975 versus Kolmo fast 2.827. Structure discovery remains a major gap. A generic context model alone does not reproduce Kolmo's existing structured-data gains.

## Board-input comparison

`board.csv` and [board-table.md](board-table.md) contain nine settings on the board-linked tar, each with at least three iterations and median reporting. `board.json` records a fresh-process peak RSS for every contender through macOS `time -l`; those numbers include lzbench input/output buffers and are not isolated codec workspace. For example, KBC1 measured 638.7 MiB process peak while the intrinsic count workspace is 4.46 MB per active worker. The two figures describe different scopes.

KBC1 measured 2.732× at 10.69 MB/s encoding and 6.86 MB/s decoding. Normal Kolmo fast measured 3.638× at 314.77/1760.54 MB/s; Zstandard 3 measured 3.205× at 526.04/2139.30 MB/s; libbsc m0/e1 measured 4.300× at 48.03/46.01 MB/s. All three beat KBC1 in every reported ratio/speed dimension. This is a local measured competitive failure, not a promotion candidate.

Brotli 8, LZ4 and libbsc compressed byte counts match their entries in the published board table. That is useful consistency evidence for the linked tar and settings; it does not make this Mac's timings comparable with the board's hardware. Exact tar framing and reset boundaries matter: libbsc ratio is 4.501× on the separately compressed twelve files and 4.300× on the tar. Report these as separate workloads instead of taking the strongest number from each.

The stronger ZPAQ 7.15 level 5 reference completed separately: **40,470,202 bytes (5.237×)**, exactly matching the published board entry, versus KBC1's **77,574,118 bytes (2.732×)**. KBC1 would need another 47.83% reduction in its own bytes to match this setting. ZPAQ's single observed encode/decode speeds were 0.73/0.73 MB/s, with 979.7 MiB whole-process peak RSS. This approximately ten-minute reference used the ARM portable interpreter (`NOJIT`); the pinned lzbench build enables its JIT only on x86. A one-second diagnostic sample during decoding confirmed progress and perturbed timing. Consequently use its verified size as strong evidence and its speed only as an exploratory local observation. It is not part of the three-repeat speed table. `ratio-leaders.csv`, `.json`, `.stderr` and `reference-diagnostic.json` preserve this distinction.

The existing highest-ratio **Kolmo ultra** mode then produced **37,956,550 bytes (5.584×)** with successful lzbench decompression verification. That is 2,513,652 bytes, or 6.21%, smaller than the ZPAQ5 result and the lowest byte count in the cited published board table. Its single observed encode/decode speeds were 1.23/1.56 MB/s on this Mac, with **5922.3 MiB whole-process peak RSS**, about six times ZPAQ's process peak. Those speeds are exploratory rather than a repeated performance win. This does not establish superiority under a matched memory budget, on AMD hardware, against every current contender/version, or on other data.

The ultra result belongs to **existing hybrid Kolmo**, with its existing backends and context-mixer provenance, not the new independent KBC1 model and not a newly invented algorithm. It is a newly measured local result, not a newly implemented compression improvement. Keep this distinction when discussing progress toward the public board. `kolmo-ultra.csv`, `.json`, `.stderr` and the separate archival section in `board-table.md` preserve it.

A separate CLI encode with two workers reproduced the exact 37,956,550-byte size. A newly launched CLI process decoded that saved frame and restored all 211,947,520 bytes with SHA-256 `a667bac80089b3d04d5496772149e632f0758974b0579d743cd8d6e5e0d63eb2`, matching the input. The saved frame is `data/kolmo-ultra.czn`; its hash and commands are in `ultra-fresh-process.json`. This was an additional correctness check, not another speed sample or a claim of matching the complete one-thread frame bit-for-bit.

## Next model decisions

The current fixed mixture sees only three preceding bytes and resets every 256 KiB. That is a design limitation, not yet a quantified attribution of each lost byte. Before changing it, profile prediction loss by block and mode, compare longer/adaptively weighted contexts against explicit run/record relationships, and charge every model/exception/restart byte. A useful theoretical reference is Willems, Shtarkov and Tjalkens, [The Context-Tree Weighting Method: Basic Properties](https://pure.tue.nl/ws/portalfiles/portal/1383848/Metis122608.pdf): it analyzes weighting bounded-memory tree models, not universal leaderboard dominance. An independent experiment can test that principle without copying a compressor implementation.

Retain the current candidate as the comparison point. A new model must improve a declared ratio/speed/memory tradeoff against both this reference and selected leaders, then survive a newly frozen validation family. Calgary has now been evaluated and must not be reused as an untouched holdout during future tuning. Optimize the probability/coding hot path only after prediction quality is useful. A GPU port must count model distribution, synchronization, transfers and decoding dependencies; the preserved KRC1 GPU result provides no automatic acceleration of this bit-dependent model.

## Equal-block CPU scaling

This separate Mozilla experiment gives every codec the same external 1 MiB blocks, one-thread callbacks, and either one or ten lzbench workers. The codec API payload sums include each KRC/KBC frame; lzbench does not add a shared archive index to the raw LZ4/Zstandard payloads. It is an equal-block scheduling comparison, not identical container formats.

| Codec | Ratio at both worker counts | Encode 1 / 10 workers MB/s | Decode 1 / 10 workers MB/s |
|---|---:|---:|---:|
| Independent KBC1 | 2.283 | 10.30 / 60.45 | 6.83 / 38.43 |
| Preserved KRC1 | 1.206 | 191.38 / 1270.67 | 2939.04 / 18363.26 |
| LZ4 | 1.936 | 1087.80 / 7497.05 | 5714.45 / 38846.33 |
| Zstandard 3 | 2.762 | 589.63 / 3480.42 | 1922.22 / 12060.98 |

Compressed byte counts are identical at one and ten workers for all four codecs, and every configuration passed lzbench verification. KBC1 uses more cores successfully, but Zstandard 3 still beats it in ratio and both speeds under the same worker/block conditions. These rows are not mixed into the single-thread corpus or whole-tar aggregates. Raw results and command manifests are `scaling-1.*` and `scaling-10.*`.

## Preservation, tests and resources

The pinned default gate passed on Mozilla, NCI and OWID at 1/4/10 threads with five randomized paired repeats. Frames remained identical; the largest median elapsed slowdown was 0.164%, below the declared 5% limit. Mozilla ten-thread current throughput measured 1001.6 MB/s against checkpoint 978.6 MB/s. That variation is retention evidence, not a new code optimization: normal Kolmo sources were unchanged. `check_default.py` verifies the earlier gate script's hash and redirects only its two output destinations into this directory. Previous measurement files were not overwritten. See `protected-tests.txt`, `protected-gate.json` and `protected-performance.csv`.

KBC1 passed 97 functional cases normally and under ASan/UBSan, including all storage modes, block edges, multiblock/thread agreement, output guards and repaired-checksum corruption. CLI tests additionally passed seven input boundaries, invalid arguments, decoded-size limits, and an 8 MiB numeric round trip with identical one/ten-thread frames. `byte-context-tests.txt` and `cli-tests.txt` preserve the logs. New CI jobs run CPU/sanitizer/source-preservation checks on Linux/macOS; no remote CI success is claimed here.

The numeric CLI example also exposes a loss: KBC1 stores 877,984 bytes versus bare KRC1's earlier 876,576 bytes because the extra outer/block framing costs space when KRC is selected. KBC1's single observed encode times were 279.0 ms at one thread and 60.7 ms at ten, with respective whole-process peaks about 31.6/72.7 MB. The old KRC1/GPU paths remain available for that reason. Do not describe KBC as a universally smaller or faster replacement.

`benchmark/checkpoints/byte-context-v1` saves the new model, CLI, tests and lzbench helpers as an immutable overlay after `independent-gpu-v1`. Later provenance notes and result documentation are recorded separately with final evidence. `protect_experiments.py` verified the earlier KRC1/GPU source and raw-evidence hashes after this cycle. The source archives preserve all earlier gains and rejected layouts; current executive documentation may evolve. Existing formats/default selections were not changed.

## Protocol and limitations

The pinned lzbench revision is `d68cdbc2309408e82da881c61d23cf70cca8db09`. The local build compiles KRC1/KBC1 sources directly with the same native optimization setting as its comparator implementations. Normal Kolmo links its separately built protected dylib to keep overlapping vendored codec symbols isolated through Mach-O two-level namespaces. Integer KBC1 uses no floating-point arithmetic. Existing Kolmo's floating-point contraction setting remains disabled for deterministic representation decisions.

`baseline-silesia.csv` is the first unchanged KRC1 comparison: nine settings, twelve files, fixed codec order. `development.csv` is the subsequent randomized codec/file matrix: ten settings across twelve Silesia, eleven Canterbury, OWID and one signal file, totaling 250 configurations. `validation.csv` has nine settings on standard Calgary14, totaling 126 configurations. The wrapper uses seed 20261108, records exact commands and executable hashes, rejects nonzero exits, malformed/missing rows, zero sizes and zero speeds. lzbench performs decompression and byte comparison; it was not replaced by custom timing. Its own adaptive inner timing loop may invoke a callback more than three times, so configuration counts and requested minimum iterations are the reproducible units.

No training weights, expected-output tables, filename detection, source-file references or omitted model bytes are used. Every standalone frame includes its format overhead and checksums. API workspace policies differ between codecs according to their normal lzbench adapters; KBC1 currently allocates its workspace inside each timed compression/decompression call. Neither the new codec nor the comparisons use the GPU. The earlier GPU gains remain evidence for the unchanged KRC1 planner only.

Local hardware is the previously inventoried Apple M5, 10 physical/logical CPU cores (4 performance + 6 efficiency), 10 GPU cores and 24 GiB unified memory. Heavy builds/tests were kept out of final timing runs. Other desktop activity, CPU frequency, thermals, affinity and energy were not controlled. Results are local medians without confidence intervals, not proof of a small statistically significant speed advantage. A successful test on this Mac does not establish performance on the public board's AMD machine or on other CPU architectures. Added CI coverage is configured for Linux/macOS but has not been run remotely in this session.

The public [lzbench board setup](https://github.com/inikep/lzbench/blob/master/doc/lzbench24_sorted.md) uses a tarred Silesia input on a single AMD EPYC 9555P thread with GCC 15.2, fixed performance governor and turbo disabled. The board-linked tar downloaded here is 211,947,520 bytes, SHA-256 `a667bac80089b3d04d5496772149e632f0758974b0579d743cd8d6e5e0d63eb2`. Its twelve member payloads hash-identically to our Silesia regression files. Tar headers are included in the local board-input measurement. The published board does not provide a verified artifact hash here, so matching URL/length is not a cryptographic equivalence claim. Local board-input speeds must not be ranked against speeds measured on that AMD system.

## Reproduce

Build the protected normal library first, then use the pinned lzbench checkout described in `benchmark/audit/COMMANDS.md`:

```sh
python3 benchmark/ratio/prepare_lzbench.py /tmp/kolmo-lzbench-audit
make -C /tmp/kolmo-lzbench-audit -j10 \
  BUILD_ONLY='KOLMO INDEPENDENT LZ4 ZSTD BROTLI LZMA BSC PPMD ZPAQ OPENZL' \
  MOREFLAGS='-mcpu=native'
make byte-context-check build/kbc
python3 benchmark/ratio/protect_experiments.py
python3 benchmark/next/protect_gains.py --sources-only
python3 benchmark/ratio/run_lzbench.py /tmp/kolmo-lzbench-audit/lzbench \
  /tmp/kolmo-development-repeat.csv --suite development
python3 benchmark/ratio/run_lzbench.py /tmp/kolmo-lzbench-audit/lzbench \
  /tmp/kolmo-validation-repeat.csv --suite validation
```

Run suites sequentially and use new output names. Frozen large inputs live in ignored `data` directories; check every hash in the manifests after downloading, because an upstream snapshot may change. The adapter is local only; no code or result has been submitted to upstream lzbench.

Additional suites use `--suite board --memory`, `--suite ratio-leaders --memory`, and `--suite scaling --workers 1` / `--workers 10` with separate output files. Memory collection currently requires macOS `time -l` and access to its read-only system counters. The existing-ultra command was `/usr/bin/time -l /tmp/kolmo-lzbench-audit/lzbench -ekolmo,5 -I1 -T1 -t0,0 -i1,1 -p3 -o4 -q benchmark/ratio/data/silesia.tar`. Run `python3 benchmark/ratio/check_default.py` to generate a new protected gate without replacing previous evidence, and `python3 benchmark/ratio/verify_final.py` to check the saved 503-configuration evidence bundle, source archives and frozen inputs. The latter also expects the local measured lzbench checkout/binary. `final-manifest.json` pins evidence, sources and executable hashes. The local adapter currently uses macOS dylib namespace isolation; target-platform ELF isolation and actual x86/Linux execution remain work before public-board qualification.
