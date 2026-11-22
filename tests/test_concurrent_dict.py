"""Concurrent callers must not share active codec state; dictionaries are not available in the Kolmo-native build
(0.0.1: they needed library codecs) and must fail cleanly with the 'not available' error, also under concurrency."""
import concurrent.futures
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'bindings/python'))
import kolmo

samples = [[f'{prefix} event={i} host=node-{i%11} state=ready metric={i%97}\n'.encode()
            for i in range(600)] for prefix in ['alpha', 'beta']]
inputs = [b''.join(s * 8) for s in samples]

def unsupported(f):
    try:
        f()
    except ValueError as e:
        assert 'not available in this build' in str(e), e
        return
    raise AssertionError('dictionary call did not fail')

def run(which):
    data = inputs[which % 2]
    for _ in range(20):
        blob = kolmo.compress(data, threads=2, block=16384, transforms=False)
        assert kolmo.decompress(blob, threads=2) == data
        if which < 2:
            unsupported(lambda: kolmo.train_dict(samples[which], dict_cap=4096))
            unsupported(lambda: kolmo.compress(data, threads=2, block=16384, transforms=False, dict=b'x' * 4096))

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    list(pool.map(run, range(4)))
print('CONCURRENCY PASSED (80 concurrent round trips; dictionary calls refused cleanly)')
