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
| claude/next 6aeebcb (11 Nov) | not accepted: targets 2, 3 and 4 pass; lz4 misses target 1 | see below |

claude/next 6aeebcb on the M5 (six runs of the branch agree within a point on the lz4 figure):
- Target 1: nine of ten rivals beaten. zstd -1 (3.25x, 690, 1113 MB/s) by `fast -l 1 -Q` (3.34x, 722, 1247); zstd -3 by
  `fast -l 1 -q` (4.12x, 510, 1201); zstd -9 by `fast`; zstd -19, brotli and gzip by `hc -l 9`; xz -6 by `max`; bzip3 by
  `max -l 1`. Open: lz4 -1 (2.28x, 952, 1497) against `speed` (2.32x, 928, 1926): encode 97.5% of lz4, target 98%.
  kolmo is 11 to 29% faster than lz4 on incompressible input and 3 to 14% slower on compressible input; the per-file
  gaps fit a fixed cost of under a millisecond per run plus about 0.05 ms per MB.
- Target 2: no confirmed slowdown on unchanged output.
- Target 3: every setting at or above 1.0.1: rt x1.000 / x1.029 / x1.078 (ratio / encode / decode); fast x1.095 / x1.015 /
  x1.051; hc -l 9 x1.080 / x1.046 / x1.027; hc x1.075 / x1.192 / x1.040; max x1.085 / x1.037 / x1.091.
- Target 4: no file larger than with 1.0.1 (Mac set, and 264 file/setting pairs on the cloud).
- The cloud gate passes: 132 ladder outputs identical to the record build, 90 floors, 51 references, all test suites.
