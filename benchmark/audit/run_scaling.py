#!/usr/bin/env python3
"""Run internally parallel Kolmo and externally parallel baselines sequentially."""
import pathlib
import subprocess
import sys

bench = sys.argv[1]
root = pathlib.Path(__file__).resolve().parents[2]
files = ['benchmark/audit/data/silesia/mozilla', 'benchmark/audit/data/silesia/nci', 'benchmark/audit/data/owid-co2-data.csv']
for t in (1, 4, 10):
    for label, opts in (
        ('kolmo', ['-ekolmo1m,0,1', '-b0', f'-I{t}']),
        ('baseline', ['-elz4/zstd,1,3', '-b1024', f'-T{t}']),
    ):
        stem = root / f'benchmark/audit/scaling-{label}-{t}'
        with stem.with_suffix('.csv').open('w') as out, stem.with_suffix('.stderr').open('w') as err:
            subprocess.run([bench, *opts, '-t0,0', '-i3,3', '-p3', '-o4', '-q', *files], cwd=root, stdout=out, stderr=err, check=True)
        print(f'Completed {label} T={t}', flush=True)
