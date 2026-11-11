# Release targets

A version is released only when `accept.py` prints ACCEPTED on a results file from the Apple M5 benchmark
(`~/Downloads/kolmo-bench`, written by `night.sh` for every candidate branch).

1. **Every rival is beaten in its own game.** For each rival setting there is one kolmo setting that, across the
   whole file set, has at least the rival's ratio, at least 98% of its encode and decode speed, and is better on at
   least one of the three.
2. **Unchanged output never gets slower.** A file whose compressed size equals the baseline's may not encode or
   decode more than 5% slower.
3. **No setting moves backwards.** Against the baseline (1.0.1), per setting: ratio not lower, encode and decode not
   below 97%.
4. **No file gets larger.** At the same setting, no file may compress to more bytes than with the baseline.

Rivals measured today: lz4, zstd (-1, -3, -9, -19), brotli, gzip, xz, bzip3. To be added as they are installed:
OpenZL, zxc, JPEG XL (images), Pcodec (numeric), CLP (logs).

## Status

| Build | Verdict | Open |
|---|---|---|
| main (rename commit 87dfc86) | not accepted | lz4 -1, zstd -1, zstd -3 on encode speed; 19 unchanged-output slowdowns; `fast` encode 0.963x and `max` encode 0.865x of 1.0.1 |
| claude/next bb04836 (11 Nov) | not accepted: targets 3 and 4 pass; lz4 misses target 1 by 0.2 points; target 2 has 7 confirmed cases | see below |

claude/next bb04836 on the M5 (four runs of the branch agree within a point on every figure here):
- Target 1: nine of ten rivals beaten. zstd -1 (3.25x, 690, 1104 MB/s) by `fast -l 1 -Q` (3.34x, 723, 1254); zstd -3 by
  `fast -l 1 -q` (4.12x, 510, 1206); zstd -9 by `fast` (4.64x, 351, 1105); zstd -19, brotli and gzip by `hc -l 9` (5.09x, 122,
  984); xz -6 by `max` (5.99x, 16, 173); bzip3 by `max -l 1` (5.85x, 53, 121).
  Open: lz4 -1 (2.28x, 952, 1498) against `speed` (2.32x, 931, 1942): encode 97.8% of lz4, target 98%.
- Target 3: every setting at or above 1.0.1: rt x1.000 / x1.034 / x1.095 (ratio / encode / decode); fast x1.095 / x1.009 /
  x1.064; hc -l 9 x1.080 / x1.041 / x1.021; hc x1.075 / x1.191 / x1.032; max x1.085 / x1.029 / x1.088.
- Target 4: no file larger than with 1.0.1 (Mac set, and 264 file/setting pairs on the cloud).
- Target 2, confirmed in two runs (5 to 9% slower with unchanged output): api.json at `fast`, `hc -l 9` and `max` (the
  minified-JSON trial runs and is rejected; shorter trials were tried and miss the gain on record-style JSON);
  ld30.txt at `hc -l 9` (wide-table trial rejected by the strong-codec check); loghub_all.log at `max` (block sorting
  tried on the log streams and not taken) and at `hc` decode; owid-co2-data.csv at `max`.
- The cloud gate passes: 132 ladder outputs identical to the record build, 90 floors, 51 references, all test suites.
