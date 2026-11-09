#!/usr/bin/env python3
"""Run portable library phase timings without including file I/O."""
import csv
import pathlib
import random
import re
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[2]
files = ['silesia/mozilla', 'silesia/nci', 'owid-co2-data.csv']
jobs = [(f, t, i) for f in files for t in [1, 4, 10] for i in range(3)]
random.Random(20261107).shuffle(jobs)
outdir = root / 'benchmark/audit/milestone1'
outdir.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='kolmo-profile-') as tmp, (outdir/'planning.csv').open('w') as output, (outdir/'planning.stderr').open('w') as log:
    writer = csv.DictWriter(output, fieldnames=['file', 'threads', 'repeat', 'plan_ms', 'blocks_ms', 'image_ms', 'record_ms'])
    writer.writeheader()
    for file, threads, repeat in jobs:
        target = pathlib.Path(tmp) / 'frame.czn'
        cmd = [str(root/'build/kolmo-profile'), '-m', 'fast', '-T', str(threads), str(root/'benchmark/audit/data'/file), str(target)]
        result = subprocess.run(cmd, text=True, capture_output=True, check=True)
        log.write(f'{file} T={threads} repeat={repeat}\n{result.stderr}\n'); log.flush()
        pairs = re.findall(r'(plan_ms|blocks_ms|image_ms|record_ms)=([0-9.]+)', result.stderr)
        values = {k: sum(float(v) for name, v in pairs if name == k) for k in ['plan_ms', 'blocks_ms', 'image_ms', 'record_ms']}
        writer.writerow({'file':file, 'threads':threads, 'repeat':repeat, **values}); output.flush()
        back = pathlib.Path(tmp) / 'back'
        subprocess.run([str(root/'build/kolmo'), '-d', '-T', str(threads), str(target), str(back)], check=True)
        assert back.read_bytes() == (root/'benchmark/audit/data'/file).read_bytes()
print('PROFILE PASSED (27 verified runs; randomized order, 1/4/10 threads)')
