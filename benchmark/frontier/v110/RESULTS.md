# Kolmo v1.1.0 validation, November 10, 2026

The Apple M5 used here has 10 physical CPU cores, 10 logical threads, 10 GPU cores and 24 GiB of unified memory. The validation below uses one CPU worker. Each comparison uses the same input and protocol for all entries within its group. Our runners took a shared exclusive timing lock. Another local compression job was observed after the board run, including in 74 of 112 four-second snapshots between 19:48 and 19:56 UTC. Small later timing margins remain provisional. The [interactive chart](index.html) also covers the project’s pinned 35-file Mac panel, cloud lzbench runs and prior specialist comparisons. The [replay instructions](REPRODUCE.md) include the exact inputs and graphs.

## Whole Silesia tar in lzbench

The common input is the 211,947,520-byte upstream board tar. These results use the GitHub v1.1.0 **default** modes. Throughput is in decimal MB/s. The harness verified every recovery. Ultra used one encode/decode pass. The other rows used three encode and five decode iterations.

| Setting | Frame bytes | Ratio | Encode MB/s | Decode MB/s |
|---|---:|---:|---:|---:|
| Kolmo rt | 98,721,068 | 2.147× | 906.82 | 4,654.33 |
| Kolmo fast | 58,260,313 | 3.638× | 336.16 | 1,706.74 |
| Kolmo eco | 50,606,599 | 4.188× | 27.36 | 705.36 |
| Kolmo hc | 47,624,712 | 4.450× | 6.03 | 1,538.55 |
| Kolmo max | 40,996,303 | 5.170× | 7.20 | 115.50 |
| Kolmo ultra | 36,061,471 | 5.877× | 0.85 | 0.81 |
| LZ4 | 100,880,800 | 2.101× | 988.93 | 5,769.52 |
| Zstd −3 | 66,133,500 | 3.205× | 517.68 | 2,051.45 |
| OpenZL generic | 61,230,519 | 3.461× | 176.87 | 1,804.56 |

Ultra produced a 36,061,471-byte frame, 10.9% smaller than the 40,470,202-byte ZPAQ −5 result at the top of the [published lzbench 2.4 ratio table](https://github.com/inikep/lzbench/blob/2706ba16721e947a9240c70d080b8efa3f752da0/doc/lzbench24_sorted.md). The November 7 local harness reproduced that ZPAQ byte count. This measures one corpus. Ultra also selects among context mixing, LZMA2 and BWT by block, while default fast uses Zstd after Kolmo’s transform selection. These are results for the complete multi-codec product. The upstream board lists its own submissions and uses an AMD EPYC host. Our Mac timings are comparable with the local replay. [Local lzbench receipts](../v110-lz/replay.json.gz)

In the project’s separate 35-file Mac CLI panel, default fast beat Zstd −3 on frame size for 24 inputs, encode speed for 4, and decode speed for 22. It led on all three for 0 of 35. Quick fast led on all three for 4 of 35. The file selector exposes the results individually. [Pinned 35-file measurements](../../lzbench/results/mac-m5-2026-11-10-v1.0.0-v1.0.1-v1.1.0.json)

## Electricity decision

Passing requires at least 20% fewer bytes and equal or lower encode time, decode time, encode RSS and decode RSS for each rival. A dataset passes when the same opt-in fast setting passes both rivals. Negative savings mean a larger Kolmo file. The European grid is a development control. The three other sources are transfer checks at this stage.

| Dataset | Saving vs Zstd −3 | All five | Saving vs trained OpenZL | All five |
|---|---:|:---:|---:|:---:|
| European grid · development | 41.5% | Pass | 24.1% | Pass |
| Portugal · customer meters | 26.2% | Miss | -45.2% | Miss |
| Germany · CoSSMic household meters | 76.9% | Miss | 67.7% | Pass |
| United States · EIA balancing authorities | 10.1% | Miss | -72.4% | Miss |

**0/3 transfer datasets passed both rivals.** The biggest size win was the German household file, where Zstd used less time and memory. The US trained OpenZL graph was 10,338 bytes and took 22.86 seconds to train on 4,194,249 earlier bytes. Its compression and decompression of the independent test file are measured separately. [US electricity source](https://www.eia.gov/electricity/gridmonitor/sixMonthFiles/EIA930_BALANCE_2020_Jan_Jun.csv)

## Release progress against 1.0.0

Same file-to-file runner, shipped default fast on both builds. Each cell shows 1.0.0 → 1.1.0. RSS is the maximum measured across seven timed trials.

| Input | Frame MB | Encode MB/s | Decode MB/s | Encode MiB RSS | Decode MiB RSS |
|---|---:|---:|---:|---:|---:|
| Silesia tar | 58.3 → 58.3 | 280.5 → 331.2 | 1201.8 → 1458.8 | 335.4 → 130.9 | 273.5 → 23.1 |
| European grid | 29.0 → 23.4 | 380.5 → 483.8 | 1149.3 → 1341.7 | 128.3 → 7.8 | 122.9 → 6.6 |
| US grid | 5.7 → 5.7 | 261.5 → 288.7 | 1014.6 → 1276.2 | 108.0 → 44.1 | 42.4 → 11.1 |
| Portuguese meters | 154.6 → 110.0 | 398.8 → 216.8 | 1,426.9 → 651.9 | 732.4 → 23.8 | 725.5 → 18.0 |

The opt-in 1.1.0 build reproduced M1 frame hashes on 10 of 11 release-replay inputs. On the Portuguese meters it reduced M1’s 130,249,618 bytes to 110,032,177, a further 15.5% saving. The 1.0.0 release was faster on the Portuguese file.

## Opt-in fast on genomes

Both FASTA selector candidates were frozen after synthetic development. The first used a fixed complexity threshold and discarded the earlier zebrafish and yeast size gains. The second checks up to four production-encoded slices where routing is ambiguous and keeps a recognized structured table plan. It uses the existing opt-in wire format. DNA frames require a frontier-capable decoder. Stock frames remain readable by the opt-in build. The three fresh inputs are PAO1, zebrafish chromosome 25 and worm chromosome I. Human chromosome 22, budding yeast and B. subtilis are preservation controls.

| Complete input | Stock bytes | Earlier opt-in bytes | New selector bytes | Encode / decode MB/s | Three wins vs Zstd −3 | NAF −19 bytes |
|---|---:|---:|---:|---:|:---:|---:|
| P. aeruginosa PAO1 | 1,932,748 | 1,492,955 | 1,492,955 | 1,005 / 1,321 | Pass | 1,497,412 |
| Zebrafish chr25 | 12,646,689 | 9,950,649 | 9,950,649 | 903 / 2,089 | Pass | 7,967,925 |
| C. elegans chrI | 4,966,585 | 3,878,458 | 3,878,458 | 866 / 1,773 | Pass | 3,492,015 |
| Human chr22 | 12,724,397 | 13,264,957 | 12,724,397 | 342 / 1,546 | Miss | 8,390,844 |
| Budding yeast | 4,114,465 | 3,552,511 | 3,552,511 | 791 / 1,471 | Pass | 2,988,337 |
| B. subtilis | 1,346,907 | 1,033,584 | 1,033,584 | 768 / 982 | Pass | 1,040,316 |

Human chr22 recovered the release-sized frame at the cost of switching away from the faster DNA path. On PAO1 the gain over Zstd was decisive. The size advantage over NAF −19 was below 1%. Sampling may miss an unusual region outside the inspected windows. Every tested output was restored byte for byte. [NAF](https://github.com/KirillKryukov/naf) · [Trial receipts](file-trials.json.gz)

## Ultra memory experiment

The optional compact build directly indexes the reachable states in two one-byte contexts. It keeps the original hash tags, bucket neighbors and collision decisions. A stock decoder reads its outputs. For each row below, all four stock runs and all four compact runs produced the same complete compressed frame. RSS is the highest of three timed runs after a warm-up. Time change is the percentage change in the median phase wall time. Positive time change means slower.

| Input | Encode peak MiB, stock → compact | Decode peak MiB, stock → compact | Encode time change | Decode time change |
|---|---:|---:|---:|---:|
| Dickens, 256 KiB prefix | 387.1 → 355.6 | 337.0 → 326.6 | +4.5% | +0.7% |
| Database, 1 MiB prefix | 1,139.2 → 1,071.6 | 1,131.7 → 1,064.5 | +0.6% | +0.6% |
| Dictionary, 1 MiB prefix | 1,084.0 → 1,069.0 | 1,066.2 → 1,051.3 | +5.7% | +0.8% |
| X-ray, 1 MiB prefix | 2,075.3 → 1,988.4 | 2,061.8 → 1,974.6 | -3.5% | +2.4% |
| MR image, complete corpus member | 4,263.4 → 4,157.1 | 4,231.5 → 4,126.2 | -2.1% | -1.5% |

Build with `make build/kolmo-cm-compact`, then use `build/kolmo-cm-compact -T 1 -m ultra input output`. The timed compact binary was built at a0d59c6. A subsequent change at f5146d8 affects fast tar planning. The combined compact binary reproduced both ultra model frames and passed cross-decoding. [Combined-build parity](experiments/compact-combined-parity.json). The earlier strategy of remapping every context lost substantial throughput in the November 7 experiments. This variant restricts remapping to two contexts with provably bounded input.

## Fast file memory on archives

The release kept a whole-file planning map alive while visiting members. The proposal gives each large member its own temporary read-only mapping. The measured size and hash of every complete stock frame were preserved. One warm-up and seven randomized timed pairs ran per codec and per input.

| Input | Encode MiB RSS, v1.1.0 → proposed | Median encode ms, v1.1.0 → proposed | Median decode ms, v1.1.0 → proposed |
|---|---:|---:|---:|
| Complete Silesia | 130.8 → 76.5 | 654.3 → 656.1 | 148.9 → 145.8 |
| Synthetic tar of structured files | 46.6 → 46.6 | 148.3 → 146.1 | 39.6 → 39.7 |
| Synthetic tar of binary files | 31.2 → 25.8 | 48.7 → 48.6 | 15.3 → 15.4 |

The November M1 encoder used 82.5 MiB and took 707.5 ms on the same Silesia trials. On the synthetic binary tar, M1 still encoded faster and peaked at 17.9 MiB versus 25.8 MiB for the proposal. Archive RSS measures process residency. The OS may keep file data in its shared filesystem cache.

## New US temporal holdout

On the already-scored 2020 EIA file, setting a 256 KiB block in the existing release reduced the output to 2,275,330 bytes. This was exploratory. The same preset, comparators, chronological split and training procedure were then frozen before downloading January–June 2022. The 2022 figures below are the confirmation. The two time periods use separate trained graphs. Training the 10,222-byte 2022 graph took 22.37 seconds on 4,194,304 early bytes. [Pre-test freeze](experiments/eia2022-freeze.json).

| Setting | Bytes | Encode MB/s | Decode MB/s | Peak encode / decode MiB |
|---|---:|---:|---:|---:|
| v1.1.0 default fast | 5,179,218 | 292.8 | 1,150.1 | 52.1 / 11.2 |
| v1.1.0 fast, 256 KiB | 2,291,098 | 426.5 | 701.6 | 13.5 / 3.1 |
| Zstd −3 | 6,329,468 | 503.1 | 1,423.2 | 7.5 / 7.4 |
| Trained OpenZL | 3,446,709 | 113.6 | 253.0 | 482.3 / 232.6 |

The preset saved 63.8% relative to Zstd and 33.5% relative to trained OpenZL. The five thresholds against Zstd were missed. The five against OpenZL were met. [2022 source](https://www.eia.gov/electricity/gridmonitor/sixMonthFiles/EIA930_BALANCE_2022_Jan_Jun.csv)

## Investment plan

1. **Advance ultra for archival storage.** Reproduce the Silesia ratio on an independent Linux host, then test full mixed corpora with compact tables at the same peak-memory budget. Track archive size, both phase speeds and peak RSS.
2. **Keep fast selective.** Protect the successful grid and genome frames in regression tests. Use EIA as development data for lowering working memory and improving categorical-column compression. Freeze the next change before scoring a new geographic or time-period holdout against both Zstd and trained OpenZL.
3. **Keep the GPU research measurable.** The [preserved November experiments](https://github.com/adnaneben/kolmo/tree/0c6f9eed39e70ca9f09204d5be5ddbb98d1b35f5) include Apple Metal acceleration and CPU-only fallback prototypes. Time transfers and verified file output in the next end-to-end replay. No GPU speed is counted in the CPU comparisons above.
