#!/usr/bin/env python3
"""Probe whether independent compression calls change format version metadata."""
import concurrent.futures
import pathlib
import sys
import threading

root = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / 'bindings/python'))
import kolmo

data = b'entity,index,signal\n' + b''.join(f'A{i//100},{i%100},{(i%100)**3}\n'.encode() for i in range(20000))
baseline = kolmo.compress(data, mode='fast', threads=1)
assert kolmo.decompress(baseline, threads=1) == data
print('Serial version:', baseline[4], 'bytes:', len(baseline), flush=True)
stop = threading.Event()

def noise():
    n = 0
    while not stop.is_set():
        kolmo.compress(b'x' * 4096, mode='fast', threads=1, transforms=False)
        n += 1
    return n

versions = []
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
    f = pool.submit(noise)
    try:
        for _ in range(12):
            blob = kolmo.compress(data, mode='fast', threads=1)
            assert kolmo.decompress(blob, threads=1) == data
            versions.append(blob[4])
    finally:
        stop.set()
    print('Concurrent background calls:', f.result(), flush=True)
print('Concurrent versions:', versions, flush=True)
print('Metadata mismatches:', sum(v != baseline[4] for v in versions), flush=True)
print('All current-decoder round trips passed.', flush=True)
