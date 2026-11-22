"""Concurrent callers of the Python binding must not share active codec state: four threads, each compressing and
decompressing at several levels with internal threads of its own, always get their own data back."""
import concurrent.futures
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'bindings/python'))
import kolmo

samples = [[f'{prefix} event={i} host=node-{i%11} state=ready metric={i%97}\n'.encode()
            for i in range(600)] for prefix in ['alpha', 'beta']]
inputs = [b''.join(s * 8) for s in samples]

def run(which):
    data = inputs[which % 2]
    for k in range(20):
        blob = kolmo.compress(data, level=1 + k % 9, threads=2, block=16384)
        assert kolmo.decompress(blob, threads=2) == data

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    list(pool.map(run, range(4)))
print('CONCURRENCY PASSED (80 concurrent round trips at levels 1-9)')
