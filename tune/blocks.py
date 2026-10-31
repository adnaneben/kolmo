#!/usr/bin/env python3
"""List the blocks of a .czn file: raw size, stored size, codec and transform."""
import struct, sys
CODEC = ["store", "lz4", "lz4hc", "zstd", "brotli", "bwt", "lzma", "cm"]
d = open(sys.argv[1], "rb").read()
assert d[:4] == b"CZN1"
nb = None
for off in (8, 12, 16, 20, 24):            # block count: the u32 whose index fits exactly
    k = struct.unpack_from("<I", d, off)[0]
    if 0 < k < 1 << 20 and 32 + k * 16 <= len(d):
        tot = sum(struct.unpack_from("<I", d, 32 + i * 16 + 4)[0] for i in range(k))
        if 32 + k * 16 + tot <= len(d) <= 32 + k * 16 + tot + 64: nb = k; break
for i in range(nb):
    raw, st = struct.unpack_from("<II", d, 32 + i * 16)
    print(f"{i}\t{raw}\t{st}\t{CODEC[d[32 + i * 16 + 12]]}\tt{d[32 + i * 16 + 13]}")
