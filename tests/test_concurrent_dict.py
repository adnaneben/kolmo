"""Different dictionaries and plain callers must not share active codec state."""
import concurrent.futures
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'bindings/python'))
import kolmo

samples = [[f'{prefix} event={i} host=node-{i%11} state=ready metric={i%97}\n'.encode()
            for i in range(600)] for prefix in ['alpha', 'beta']]
dicts = [kolmo.train_dict(s, dict_cap=4096) for s in samples]
inputs = [b''.join(s * 8) for s in samples]

def run(which):
    data = inputs[which % 2]
    d = dicts[which % 2] if which < 2 else None
    for _ in range(20):
        blob = kolmo.compress(data, threads=2, block=16384, transforms=False, dict=d)
        assert kolmo.decompress(blob, threads=2, dict=d) == data
        if d:
            try:
                kolmo.decompress(blob, dict=dicts[1 - which])
            except ValueError:
                pass
            else:
                raise AssertionError('Wrong dictionary accepted')

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    list(pool.map(run, range(4)))
print('DICTIONARY CONCURRENCY PASSED (80 mixed dictionary/plain round trips)')
