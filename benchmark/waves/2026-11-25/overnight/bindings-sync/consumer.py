"""Use the installed wheel and validate the safe Python interface with the same settings as Rust."""
from pathlib import Path
import sys
import kolmo

src, dest = map(Path, sys.argv[1:])
data = src.read_bytes()
frame = kolmo.compress(data, level="ultra", threads=1, memlimit=256<<20)
assert kolmo.decompress(frame, threads=1) == data
dest.write_bytes(frame)
print(kolmo.__file__, kolmo._lib._name)
