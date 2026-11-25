import hashlib
import importlib.util
import json
import os
from pathlib import Path
import random

root = Path("/tmp/kolmo-audit-20261125.JEW7JB")
os.environ["KOLMO_LIB"] = str(root / "main/build/libkolmo.dylib")
def load_wrapper(variant):
    spec = importlib.util.spec_from_file_location(variant, root / variant / "bindings/python/kolmo.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

baseline, candidate = map(load_wrapper, ("main", "python"))
rows = []
for data in [b"", b"a", bytes(range(256)), b"hello world\0"*8533, random.Random(843).randbytes(2097152)]:
    for level in (1, 3, 5):
        for threads in (1, 4, 0):
            old = baseline.compress(data, level=level, threads=threads)
            new = candidate.compress(data, level=level, threads=threads)
            assert old == new
            assert baseline.decompress(new, threads=threads) == data
            rows.append([len(data), level, threads, len(new), hashlib.sha256(new).hexdigest()])
print(json.dumps(rows))
