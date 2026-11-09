#!/usr/bin/env python3
"""Run the earlier paired gate, redirecting its outputs to preserve prior evidence."""
import hashlib
import json
import pathlib

root = pathlib.Path(__file__).resolve().parents[2]
script = root/'benchmark/next/protect_gains.py'
manifest = json.loads((root/'benchmark/checkpoints/independent-gpu-v1/manifest.json').read_text())
expected = next(f['sha256'] for f in manifest['files'] if f['path']=='benchmark/next/protect_gains.py')
assert hashlib.sha256(script.read_bytes()).hexdigest()==expected
source = script.read_text()
for name in ('protected-performance.csv','protected-gate.json'):
    old = 'benchmark/next/'+name
    assert source.count(old)==1
    source = source.replace(old,'benchmark/ratio/'+name)
print('Running the pinned paired gate with evidence destinations redirected only',flush=True)
exec(compile(source,str(script),'exec'),{'__file__':str(script),'__name__':'__main__'})
