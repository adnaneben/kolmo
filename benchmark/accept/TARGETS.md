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
5. **One thread means one thread.** Every tool is asked for one thread. A run whose CPU time exceeds its elapsed time
   by more than 3% used helper threads and is charged its CPU time. (lz4 1.10 with `-T1` does this: on a 20 MB file it
   takes 11.0 ms elapsed and 14.1 ms of CPU; kolmo takes 12.2 ms elapsed and 11.4 ms of CPU.)

Rivals measured today: lz4, zstd (-1, -3, -9, -19), brotli, gzip, xz, bzip3. To be added as they are installed:
OpenZL, zxc, JPEG XL (images), Pcodec (numeric), CLP (logs).

## Status

| Build | Verdict | Open |
|---|---|---|
| main (rename commit 87dfc86) | not accepted | lz4 -1, zstd -1, zstd -3 on encode speed; 19 unchanged-output slowdowns; `fast` encode 0.963x and `max` encode 0.865x of 1.0.1 |
| claude/next 6aeebcb (11 Nov) | not accepted: targets 2, 3 and 4 pass; lz4 misses target 1 | see below |
| claude/next 3f6c7d5 (11 Nov) | **ACCEPTED** (first time), with target 5 in force | none; needs a second accepted run before a merge to main |
| claude/next with line classes and Codex's three commits (11 Nov) | waiting for the M5 run | balanced changed, so it is measured again from scratch |

claude/next 6aeebcb on the M5 (six runs of the branch agree within a point on the lz4 figure):
- Target 1: nine of ten rivals beaten. zstd -1 (3.25x, 690, 1113 MB/s) by `fast -l 1 -Q` (3.34x, 722, 1247); zstd -3 by
  `fast -l 1 -q` (4.12x, 510, 1201); zstd -9 by `fast`; zstd -19, brotli and gzip by `hc -l 9`; xz -6 by `max`; bzip3 by
  `max -l 1`. Open: lz4 -1 (2.28x, 952, 1497) against `speed` (2.32x, 928, 1926): encode 97.5% of lz4, target 98%.
  kolmo is 11 to 29% faster than lz4 on incompressible input and 3 to 14% slower on compressible input; the per-file
  gaps fit a fixed cost of under a millisecond per run plus about 0.05 ms per MB.
- Target 2: no confirmed slowdown at 6aeebcb; the next run (e3e6b08, same code plus a static liblzma on macOS) confirms
  one, api.json at `max` (encode x0.92 to x0.94), and leaves lz4 at 97.7%. The Mac script now also records where a
  speed-mode run spends its time (start-up, planning, blocks) against lz4.
  A further run (339589d, same code) shows no confirmed slowdown and lz4 at 97.6%: lz4 is the only open item.
- Target 3: every setting at or above 1.0.1: rt x1.000 / x1.029 / x1.078 (ratio / encode / decode); fast x1.095 / x1.015 /
  x1.051; hc -l 9 x1.080 / x1.046 / x1.027; hc x1.075 / x1.192 / x1.040; max x1.085 / x1.037 / x1.091.
- Target 4: no file larger than with 1.0.1 (Mac set, and 264 file/setting pairs on the cloud).
- The cloud gate passes: 132 ladder outputs identical to the record build, 90 floors, 51 references, all test suites.

claude/next 3f6c7d5 on the M5, with runs that used helper threads charged their CPU time (target 5): all ten rivals
beaten. lz4 -1 (2.28x, 858, 1313 MB/s) by `speed` (2.32x, 907, 1844); zstd -1 (3.25x, 656, 1034) by `fast -l 1 -Q`
(3.34x, 712, 1212); zstd -3 (3.46x, 475, 964) by `fast -l 1 -q` (4.12x, 502, 1164); zstd -9 by `fast`; zstd -19 by `hc`;
brotli and gzip by `hc -l 9`; xz -6 by `max`; bzip3 by `max -l 1`. No confirmed slowdown on unchanged output, every
setting at or above 1.0.1, no file larger. lz4 on elapsed time alone is still 2 to 3% ahead on encode.

## A separate goal: balanced against zstd -3 on Silesia, round trip

Not part of the acceptance rule, tracked here. Silesia tar, in memory, one thread, same process, cloud VM, best of
seven: zstd -3 is 31.20% at 189 MB/s round trip (encode 230, decode 1063). `balanced` was 27.49% at 124 MB/s (66%) and
is 27.48% at 176 MB/s (93%; encode 221, decode 867). `fast -l 2` is 28.48% at 200 MB/s: 8.7% smaller than zstd -3 and
6% faster. Open for `balanced`: about 80 ms on 212 MB. Planning is 130 ms (record search on sao, 16-bit image
detection on mr and x-ray, the trials on mozilla, osdb and samba) and decode is 45 ms behind (image and line-class
inverses). The LZ pass is zstd level 3 in both, so passing zstd -3 at this ratio needs planning near zero or an LZ
stage cheaper than zstd's at the same ratio.
