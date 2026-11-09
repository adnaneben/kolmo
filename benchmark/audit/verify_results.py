#!/usr/bin/env python3
"""Validate primary benchmark coverage, sizes, and scaling invariants."""
import csv
import pathlib

root = pathlib.Path(__file__).resolve().parent
rows = list(csv.DictReader((root / 'lzbench-single.csv').open()))
assert len(rows) == 130, len(rows)
assert len({r['Filename'] for r in rows}) == 13
assert len({r['Compressor name'] for r in rows}) == 10
for r in rows:
    assert int(r['Original size']) > 0 and int(r['Compressed size']) > 0
    assert float(r['Compression speed']) > 0 and float(r['Decompression speed']) > 0
    assert pathlib.Path(r['Filename']).stat().st_size == int(r['Original size'])
assert not (root / 'lzbench-single.stderr').read_text().strip()
for kind, expected in [('kolmo', 6), ('baseline', 9)]:
    sizes = {}
    for t in [1, 4, 10]:
        r = list(csv.DictReader((root / f'scaling-{kind}-{t}.csv').open()))
        assert len(r) == expected, (kind, t, len(r))
        sizes[t] = {(x['Filename'], x['Compressor name']): x['Compressed size'] for x in r}
        assert not (root / f'scaling-{kind}-{t}.stderr').read_text().strip()
    assert sizes[1] == sizes[4] == sizes[10], (kind, 'output sizes changed with worker count')
print('Validated all 130 primary measurements and 45 scaling measurements; sizes stable across thread counts.')

archive = list(csv.DictReader((root / 'lzbench-archive.csv').open()))
assert len(archive) == 91
assert len({r['Filename'] for r in archive}) == 13
assert len({r['Compressor name'] for r in archive}) == 7
assert not (root / 'lzbench-archive.stderr').read_text().strip()
print('Validated all 91 archive measurements and complete coverage of 13 inputs.')
