#!/usr/bin/env python3
"""png_idat.py in.png out.raw : concatenated IDAT payload, zlib-inflated (filter byte + filtered row per scanline).
Prints: png bytes, idat (deflate) bytes, inflated bytes, IHDR."""
import sys, zlib, struct
d = open(sys.argv[1], 'rb').read(); p = 8; idat = b''; ihdr = None
while p < len(d):
    l, t = struct.unpack('>I4s', d[p:p+8]); body = d[p+8:p+8+l]
    if t == b'IDAT': idat += body
    if t == b'IHDR': ihdr = struct.unpack('>IIBBBBB', body)
    p += 12 + l
raw = zlib.decompress(idat); open(sys.argv[2], 'wb').write(raw)
print(sys.argv[1], len(d), len(idat), len(raw), ihdr, 'zlib hdr %02x%02x' % (idat[0], idat[1]))
