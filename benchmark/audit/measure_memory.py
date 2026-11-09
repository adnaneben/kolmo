#!/usr/bin/env python3
"""Measure child CLI peak RSS on macOS; run from the Kolmo repository."""
import json
import pathlib
import resource
import subprocess

root = pathlib.Path(__file__).resolve().parents[2]
rows = []
for mode in ['fast', 'ultra']:
    subprocess.run([str(root / 'build/kolmo'), '-m', mode, '-T', '1',
                    str(root / 'benchmark/audit/data/silesia/dickens'),
                    f'/tmp/kolmo-memory-{mode}.czn'], check=True)
    rows.append({'mode': mode,
                 'cumulative_child_maxrss': resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss,
                 'input_bytes': 10192446, 'threads': 1})
print(json.dumps({'platform': 'macOS', 'runs': rows}, indent=2))
