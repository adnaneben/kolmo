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

Status on 2026-11-10 (main at the rename commit): not accepted. Open: lz4 -1 and zstd -1 / -3 on encode speed;
19 unchanged-output slowdowns; `fast` encode at 0.963x and `max` encode at 0.865x of 1.0.1.
