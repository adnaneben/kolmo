# B26 · Whole-file regret for the shipped size selector

The pinned production CLI chose Ultra on each of three preselected files. Its ordinary v12 output matched the smallest complete frame produced by any of its nine eligible routes, byte for byte. Including startup, probing, encoding and stock decoding, the selector took 1.30×, 1.37× and 1.51× the time of forcing Ultra on the same input (median paired ratios).

| Complete input | Input bytes | Selector prediction | Actual and minimum bytes | Size regret | Selector round trip | Direct Ultra round trip | Median paired time ratio |
|---|---:|---:|---:|---:|---:|---:|---:|
| GNU GMP 6.3.0 manual, gmp.info-1 | 300,383 | 64,612 | 53,828 | 0 B | 2,133.6 ms | 1,637.5 ms | 1.30× |
| GnuPG 2.5.24_1, gpgv executable | 557,232 | 184,777 | 163,547 | 0 B | 2,552.4 ms | 1,764.3 ms | 1.37× |
| GnuTLS 3.8.14, handshake diagram PNG | 7,584 | 6,457 | 6,457 | 0 B | 90.7 ms | 52.9 ms | 1.51× |

## Where the samples misranked routes

The selector compressed three source spans on each large input. Replaying all nine settings on the exact spans recovered its reported winning route and integer size estimate on every file. GMP's three text spans covered 196,435 bytes after line alignment. GnuPG's three binary spans covered 196,608 bytes. All six spans individually preferred Ultra. The 7,584-byte PNG qualified for the whole-chunk path, which measured each complete frame and reused the winner. The scaled winning estimates overpredicted the final GMP and GnuPG sizes by 10,784 B and 21,230 B, respectively.

On GnuPG, the nine routes produce 36 unordered pairs. Thirty-three kept the same ordering, two reversed and one ceased to tie. At the sample offsets, the scaled estimates ranked ratio (203,698 B) and ratio-l1 (206,320 B) above x-l1 (210,311 B). On the complete file x-l1 stored 178,050 B, beating ratio by 7,412 B and ratio-l1 by 12,354 B. Balanced and balanced-n tied in the samples, then differed by 17,099 B on the whole file. GMP preserved all 36 comparisons including two ties. PNG preserved all 36 including one tie. The selection winner survived these ordering changes, but subsets of the eligible set could select a different winner.

## Size and speed alternatives

| Input | Fastest eligible direct route | Frame bytes | Direct round trip | Charged selector time / fastest | Selector encode / decode MB/s |
|---|---|---:|---:|---:|---:|
| GMP | speed | 121,012 | 8.11 ms | 262.99× | 0.21 / 0.43 |
| GnuPG | speed | 335,370 | 8.54 ms | 298.76× | 0.44 / 0.46 |
| GnuTLS PNG | balanced-n | 7,063 | 10.33 ms | 8.78× | 0.15 / 0.20 |

The fastest routes trade stored bytes for latency. Ultra was the only directly accessible contender at or below the chosen size on each file. Seven paired rounds favored direct Ultra in 6/7, 7/7 and 7/7 cases. On GMP, ratio-l1 reached 64,804 B in 34.2 ms, compared with 53,828 B in 1,637.5 ms for direct Ultra. On PNG, ratio reached 6,552 B in 13.2 ms, 95 B above direct Ultra. The observed size and median combined-latency Pareto frontiers across the nine settings are in regret.csv. Small timing differences can reorder that frontier. Wall-time values include normal operating-system activity on a shared local machine.

## Method and boundaries

freeze.json pins the untouched source SHA and byte count for each producer, the exact binary identities, the nine named settings, the three extra public controls, probe logic source digests and the randomization seed. These complete files came from three independent upstream packages that were unused in the earlier offline learning experiment. The selection and settings were frozen before this set was measured. Hindsight optima here serve only as comparisons. The three controls at levels 2, 4 and 6 complete coverage of the public levels 1–9 and Ultra and yielded no smaller frame. The native selector has two additional flag combinations, balanced-n and x-l1, within its nine eligible settings.

The encoder binary matches pinned main a1d39559686a9ec17c8614b9154a64d7b5b98672. The independent stock reader was the installed Homebrew release 0.0.2. The matching encoder build record supplies Apple clang 21, -O3, -mcpu=native and -ffp-contract=off on Apple M5/macOS 27.2 arm64. The replay verifies the executables' SHA-256 values at runtime. All inputs fit one selector chunk. Each arm ran as a fresh file-to-file CLI process with one thread, no custom calibration and a warm shared filesystem. One shuffled warmup and seven shuffled scored rounds per file used the same seed. The selector ran with -v to capture its actual route and prediction. The direct contenders ran with -q. Each encoder and independent decoder subprocess was timed from before launch until exit, including normal file I/O. Frame hashing and equality checks ran outside the timed processes. Combined latency uses the median of each round's encoder-plus-decoder wall time. Combined MB/s counts original bytes once per full round trip. “Median paired” divides the target and forced costs within each repeat before taking a median. It can differ from a ratio of the two displayed medians.

Every one of the 312 whole-file encode/decode pairs (273 scored) and 54 separate slice pairs produced a CZN1 version 12 frame and restored the original bytes with the stock reader. Scoring checks all 39 per-file/per-arm frame identities across the eight repeats, the reported selections, the source span digests, the sampled oracle, and the equality of the forced Ultra and chosen complete frames. The selector is a shipped per-input probing policy. This measurement establishes a cost for using it and a limit on its estimator; persisted feedback learning, multi-chunk boundaries, speed floors, cost expressions, thread scaling and the lower-level make_plan transform trials remain outside this experiment. No admission or inference defect was demonstrated in the separate learning deployment tooling. No training, routing or default change follows from these three files.

## Reproduction

Use the full files named in freeze.json or provide exact-digest copies named gmp-manual, gpgv and handshake-png in a directory passed with --input-dir to both commands. Choose an empty output directory.

```sh
python3 replay.py --encoder /path/to/pinned/kolmo --reader /path/to/stock/kolmo --out /tmp/regret-replay
python3 score.py --data /tmp/regret-replay
```

whole.csv and probes.csv hold original timings, maximum resident set sizes and frame hashes. spans.csv gives the verified source offsets, lengths and input hashes. settings.csv contains all whole-file size, encode, decode and combined speeds, and each route's projected size. rank-pairs.csv compares all sample and full-file pair orderings. regret.csv contains size regret, latency ratios, matched-size bounds and the directly measured frontiers.
