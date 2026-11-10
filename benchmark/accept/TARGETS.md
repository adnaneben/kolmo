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

Rivals measured today: lz4, zstd (-1, -3, -9, -19), brotli, gzip, xz, bzip3. To be added as they are installed:
OpenZL, zxc, JPEG XL (images), Pcodec (numeric), CLP (logs).

## Status

| Build | Verdict | Open |
|---|---|---|
| main (rename commit 87dfc86) | not accepted | lz4 -1, zstd -1, zstd -3 on encode speed; 19 unchanged-output slowdowns; `fast` encode 0.963x and `max` encode 0.865x of 1.0.1 |
| claude/next 75aec70 | not accepted | lz4 -1 (encode 0.78x), zstd -1 and -3 (no `-q` rows in that run); 16 unchanged-output slowdowns of 5 to 13%; `hc -l 9` decode 0.957x; `max` encode 0.899x |

What moved between the two on the M5: `rt` 716 -> 744 MB/s encode and 1537 -> 1662 decode; `fast` back to 1.0.1's encode
speed (351 vs 349) while keeping the 9.5% ratio gain; `max` decode 158 -> 181. The cloud gate passes for the branch
(132 ladder outputs identical to the record build, 90 floors, 51 references, all test suites).

Known reasons for what is still open:
- lz4: with one thread lz4 reaches 949 MB/s; kolmo `rt` wraps LZ4 and adds planning, a checksum and its container, so
  it cannot be faster than lz4 without a codec of its own. On files where a transform fires it is 2 to 4x slower to
  encode for a 3.5x better ratio. Beating lz4 at speed needs a native fast LZ (see zxc) and a transform-free path.
- zstd -1 / -3: `fast -q` and `fast -l 1 -q` are the contenders; they are in the Mac script from the next run on.
- Small files (1 to 4 MB) carry a fixed start-up and buffer set-up cost that shows as 5 to 13% at these speeds.
