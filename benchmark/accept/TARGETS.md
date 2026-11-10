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
| claude/next 05aa2dc (11 Nov) | not accepted: targets 1, 3 and 4 pass, target 2 has 6 confirmed cases | see below |

claude/next 05aa2dc on the M5:
- Target 1: all ten rivals beaten. lz4 -1 (2.28x, 891, 1409 MB/s) by `speed` (2.32x, 878, 1820); zstd -1 (3.25x, 667, 1070)
  by `fast -l 1 -Q` (3.34x, 696, 1188); zstd -3 by `fast -l 1 -q` (4.12x, 492, 1140); zstd -9 and -19 by `fast` (4.64x, 338,
  1041); brotli and gzip by `hc -l 9` (5.09x, 117, 939); xz -6 by `max` (5.99x, 14, 167); bzip3 by `max -l 1`.
- Target 3: every setting at or above 1.0.1: rt x1.000 / x1.039 / x1.078 (ratio / encode / decode); fast x1.095 / x1.007 /
  x1.046; hc -l 9 x1.080 / x1.045 / x1.021; hc x1.075 / x1.206 / x1.035; max x1.085 / x0.976 / x1.093.
- Target 4: no file larger than with 1.0.1 (Mac set, and 264 file/setting pairs on the cloud after the small-block fix).
- Target 2, confirmed in two runs (5 to 12% slower with unchanged output): `fast` and `hc -l 9` on api.json (the minified-JSON
  trial runs and is rejected); `hc -l 9` on ld30.txt (wide-table trial rejected by the strong-codec check); `max` on
  loghub_all.log (block sorting tried on the log streams and not taken; the same attempt makes JSON 10% smaller);
  `hc` decode on sao and lfw_subset.npy.
- The cloud gate passes: 132 ladder outputs identical to the record build, 90 floors, 51 references, all test suites.
