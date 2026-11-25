# Whole-file competitor results

The experimental typed fast presets reduced both stored bytes and combined wall time versus OpenZL's built-in CSV profile at settings 1, 6 and 12 on the 6,922,645-byte energy CSV and 14,377,942-byte CO₂ CSV. Stock zstd levels 1 through 22 all produced larger frames than either typed preset on these files. Fast zstd levels can still finish earlier. Four independent GHCN station files gave the opposite size result against OpenZL, especially with an OpenZL graph trained on different stations.

## Fast presets and the built-in OpenZL CSV profile

Whole files, seven scored repetitions. Encode, decode and combined rates are medians in decimal MB/s. Each combined rate uses the input bytes divided by that repetition's summed encode and decode wall times. Every decompression matched the original SHA-256.

| Input | Encoder | Bytes | Encode | Decode | Combined |
|---|---|---:|---:|---:|---:|
| Energy | Kolmo typed L1 | 1,205,594 | 235.5 | 370.3 | 140.1 |
| Energy | Kolmo typed L2 | 1,052,607 | 213.0 | 361.3 | 130.6 |
| Energy | OpenZL CSV 1 | 1,561,138 | 88.6 | 182.7 | 57.4 |
| Energy | OpenZL CSV 6 | 1,660,002 | 55.7 | 176.2 | 42.7 |
| Energy | OpenZL CSV 12 | 1,625,668 | 10.5 | 158.8 | 9.8 |
| CO₂ | Kolmo typed L1 | 2,210,685 | 298.0 | 639.8 | 203.3 |
| CO₂ | Kolmo typed L2 | 1,927,296 | 301.8 | 569.1 | 187.8 |
| CO₂ | OpenZL CSV 1 | 3,073,012 | 108.5 | 215.5 | 70.1 |
| CO₂ | OpenZL CSV 6 | 3,113,453 | 51.2 | 215.8 | 41.4 |
| CO₂ | OpenZL CSV 12 | 3,053,400 | 8.9 | 210.6 | 8.6 |

## zstd size and time comparisons

All 22 stock compression levels were encoded on each input. The closest sizes were the smallest zstd outputs. No zstd result in this sweep matches either typed preset's compressed size. The table includes the smallest zstd frame and nearby-time settings chosen by the initial screen. Full phase medians for all retimed levels are in competitor-results.csv. Original A/A2/B fast-route measurements are in results.csv.

| Input | Encoder | Bytes | Encode | Decode | Combined |
|---|---|---:|---:|---:|---:|
| Energy | Kolmo typed L1 | 1,205,594 | 242.1 | 413.4 | 155.9 |
| Energy | Kolmo typed L2 | 1,052,607 | 231.6 | 418.0 | 147.8 |
| Energy | zstd 1 | 1,892,422 | 316.4 | 511.6 | 198.7 |
| Energy | zstd 3 | 2,078,047 | 213.4 | 386.2 | 139.9 |
| Energy | zstd 22, smallest | 1,662,959 | 1.6 | 407.5 | 1.6 |
| CO₂ | Kolmo typed L1 | 2,210,685 | 270.3 | 660.5 | 191.4 |
| CO₂ | Kolmo typed L2 | 1,927,296 | 221.3 | 561.2 | 164.2 |
| CO₂ | zstd 2 | 4,312,346 | 292.8 | 633.7 | 204.5 |
| CO₂ | zstd 4 | 4,565,663 | 234.3 | 595.8 | 167.2 |
| CO₂ | zstd 18, smallest | 3,442,435 | 2.0 | 570.5 | 2.0 |

Kolmo typed L1 used 27.5% fewer bytes on energy and 35.8% fewer on CO₂ than the smallest zstd frames. The CO₂ L2 and zstd 4 recheck was within 2% on combined speed with 57.8% fewer bytes from Kolmo. The time-selection screen has a single timing per level and ran on a shared host. Levels 1 through 22 cover stock positive levels, with fast negative levels, advanced parameter combinations and external dictionaries outside this sweep.

## Transfer to four held-out GHCN stations

Each typed L1 and L2 output on these stations matched the corresponding pinned-main frame byte for byte. The pre-existing OpenZL CSV graph was trained on four separate station IDs. Its training receipt records 941.57 seconds. The 36,560-byte graph was supplied to the encoder. The decoder used only the built-in CSV profile. Bytes in this table are directly produced archive bytes; neither the graph's encoder deployment cost nor historical one-time training time enters the measured rates.

| Station | Kolmo L2 bytes | Kolmo L2 combined | OpenZL CSV 1 bytes | OpenZL CSV 1 combined | Trained OpenZL 6 bytes | Trained OpenZL 6 combined |
|---|---:|---:|---:|---:|---:|---:|
| USW00012839 | 950,300 | 292.2 | 489,422 | 76.4 | 240,234 | 53.2 |
| USW00013874 | 1,076,860 | 375.2 | 537,490 | 73.0 | 266,885 | 48.8 |
| USW00023183 | 879,336 | 321.4 | 473,690 | 58.0 | 235,215 | 36.2 |
| USW00024233 | 962,922 | 305.6 | 500,955 | 71.8 | 232,694 | 50.2 |

The four station files share a schema. This measures generalization across stations within that schema. The whole-file energy and CO₂ measurements cover the built-in OpenZL CSV profile. The [trained-model follow-up](trained-owid/README.md) used a fresh fixed country-label split within each schema to compare against two separate learned OpenZL models. Those results use different input hashes.

## Reproduction

All 400 arms in these whole-file comparisons completed. The country-split follow-up includes a further 128 successful arms and separately records the CO₂ strict-graph failures. The JSONL files preserve the original, frame and readback hashes, wall and child CPU seconds per phase, and peak RSS bytes per child. competitor-results.csv summarizes these raw records. ghcn-inputs.json includes separate SHA-256 digests of the training files, tests and model, plus pinned-main frame hashes. Energy SHA-256 is 3f89e200ea9e91a6a1491784d3fd6afe1643e01f3eb0ddf31a1c2214afbba0cb. CO₂ SHA-256 is 7f78e2b218ce4bb8c538bbec04fdc9a7982e8d40bff972e650df603899edd5f6.

Use the baseline and guarded candidate binaries in README.md. OpenZL CLI v0.3.0 was built from 50f8789a0c6da9273079045310149db396bb5bba. Its binary SHA-256 is 0f4bd75ed39d7c3a3cc583fd302631dddf502df4859ebcd69b75f9bd354153f9. Stock zstd v1.5.7 binary SHA-256 is f59db1267be3158941b6a70902ddcea423a294271ded955f0afb8e889b587426. Kolmo and zstd were explicitly single-threaded; OpenZL used its CLI defaults. All operations use normal file-to-file commands and include process startup, warm-cache I/O and publication. CPU/RSS and elapsed times on this shared Apple M5 remain exploratory. Energy was development data and the CO₂ transfer input was previously public.

Run from a clean output directory, where CORPUS is the November review corpus and the remaining variables identify the respective executables and scripts.

```sh
python3 "$SCRIPTS/matched_zstd.py" "$CORPUS" "$MAIN" "$CANDIDATE" zstd-regular.jsonl
python3 "$SCRIPTS/matched_zstd.py" "$CORPUS" "$MAIN" "$CANDIDATE" zstd-ultra-and-time.jsonl --extend zstd-regular.jsonl
python3 "$SCRIPTS/csv_competitor.py" "$CORPUS" "$MAIN" "$CANDIDATE" "$ZLI" openzl-csv.jsonl
python3 "$SCRIPTS/csv_competitor.py" "$CORPUS" "$MAIN" "$CANDIDATE" "$ZLI" openzl-trained-holdout.jsonl --ghcn
```

Run summarize-competitors.py alongside the resulting files to regenerate the CSV. Exact frame bytes depend on the specified versions. Rank comparisons use timings from the same suite and input.
