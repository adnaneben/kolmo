#!/usr/bin/env python3
"""DFloat11-equivalent size of a bf16 safetensors file, computed on the CPU from DFloat11's published method.

DFloat11 (Zhang et al., 2025, https://github.com/LeanModels/DFloat11) keeps each bf16 weight's sign and 7-bit
mantissa as is (8 bits) and Huffman-codes the 8-bit exponent; its decoder runs only on NVIDIA GPUs (CUDA), so the
real tool cannot run on the Apple M5 used here. This script computes the size that method gives:
    8 bits x weights  +  Huffman-coded exponents (one code per tensor, code lengths from the tensor's histogram)
    + the other (non-bf16) tensors and the safetensors header, stored as is.
It is a close estimate, not DFloat11's own output: DFloat11 shares one code per transformer block and adds small
lookup tables, so its real files differ by well under 1%. Label it "DFloat11 (method, computed)" wherever shown.

usage: dfloat11_tool.py MODEL.safetensors [MODEL ...] OUT.json     (needs numpy)"""
import heapq, json, os, struct, sys

import numpy as np


def huffman_bits(counts):
    c = [int(x) for x in counts if x]
    if len(c) <= 1:
        return sum(c)  # one symbol: 1 bit each at most; DFloat11 would need ~0, count 1 bit to stay conservative
    h = [(x, i, 0) for i, x in enumerate(c)]; heapq.heapify(h); total = 0; k = len(c)
    while len(h) > 1:
        a, _, _ = heapq.heappop(h); b, _, _ = heapq.heappop(h)
        total += a + b; heapq.heappush(h, (a + b, k, 0)); k += 1
    return total  # sum of count x code length


def one(path):
    with open(path, "rb") as f:
        n = struct.unpack("<Q", f.read(8))[0]
        hdr = json.loads(f.read(n)); base = 8 + n
        out_bits = 8 * base; bf16 = other = 0
        for name, t in hdr.items():
            if name == "__metadata__":
                continue
            s, e = t["data_offsets"]; f.seek(base + s); buf = f.read(e - s)
            if t["dtype"] != "BF16":
                out_bits += 8 * len(buf); other += len(buf); continue
            w = np.frombuffer(buf, dtype="<u2")
            exp = ((w >> 7) & 0xFF).astype(np.uint8)
            out_bits += 8 * w.size + huffman_bits(np.bincount(exp, minlength=256))
            bf16 += len(buf)
    size = os.path.getsize(path); comp = (out_bits + 7) // 8
    return dict(file=os.path.basename(path), bytes=size, bf16_bytes=bf16, other_bytes=other,
                dfloat11_method_bytes=comp, ratio=size / comp)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    res = [one(p) for p in sys.argv[1:-1]]
    json.dump(res, open(sys.argv[-1], "w"), indent=1)
    for r in res:
        print(f"{r['file']:40s} bf16 {r['bf16_bytes']/2**20:8.1f} MiB  DFloat11 method {r['ratio']:.3f}x")
