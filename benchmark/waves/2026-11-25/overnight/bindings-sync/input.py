"""Create a deterministic nonbiological, punctuation-only four-line record corpus."""
import random
from pathlib import Path
import sys

rng = random.Random(825)
tile = []
for i in range(73):
    width = (83, 129, 247)[i % 3]
    header = ("@synthetic-%d" % i).encode()
    punctuation = bytes(rng.choices(b"?!~#%&*.)( ", k=width)).replace(b" ", b"!")
    opaque = bytes(rng.choices(range(33, 93), k=width))
    tile.append(b"\n".join([header, punctuation, (b"+" + header[1:]) if i % 2 else b"+", opaque]) + b"\n")
output = bytearray()
i = 0
while len(output) < (8 << 20) + (48 << 10):
    output.extend(tile[i % len(tile)])
    i += 1
Path(sys.argv[1]).write_bytes(output)
