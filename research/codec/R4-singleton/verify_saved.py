#!/usr/bin/env python3
"""Independently verify the committed raw timing ledger and all saved medians."""
from __future__ import annotations
import csv
import hashlib
import io
import json
import lzma
import statistics
from pathlib import Path


def verify(root: Path) -> dict:
    schema = json.loads((root / 'results/timings_schema.json').read_text())
    compressed = (root / 'results/timings.csv.xz').read_bytes()
    assert hashlib.sha256(compressed).hexdigest() == schema['xz_sha256']
    raw = lzma.decompress(compressed)
    assert hashlib.sha256(raw).hexdigest() == schema['raw_sha256']
    rows = [{k: int(v) for k, v in r.items()} for r in csv.DictReader(io.StringIO(raw.decode()))]
    assert len(rows) == schema['rows'] == 504
    assert len({(r['phase'], r['round'], r['case'], r['arm'], r['displaced']) for r in rows}) == 504
    saved = json.loads((root / 'results/summary.json').read_text())
    values_checked = 0
    for phase, phase_name in enumerate(schema['phases']):
        expected_rounds = 7 if phase == 0 else 5
        for entry in saved[phase_name]:
            case = schema['cases'].index(entry['case'])
            for displaced, regime in enumerate(('warm', 'displaced_batch')):
                for metric in ('kernel_ns', 'full_ns'):
                    medians = []
                    for arm in range(3):
                        matching = [r for r in rows if r['phase'] == phase and r['case'] == case
                                    and r['arm'] == arm and r['displaced'] == displaced]
                        assert len(matching) == expected_rounds
                        assert {r['round'] for r in matching} == set(range(expected_rounds))
                        assert all(r['batch'] > 0 and r[metric] > 0 for r in matching)
                        medians.append(statistics.median(r[metric] / r['batch'] for r in matching))
                    expected = dict(baseline=medians[0], candidate=medians[1],
                                    ratio=medians[1] / medians[0], identical_baseline_ratio=medians[2] / medians[0])
                    assert entry[regime + '_' + metric] == expected
                    values_checked += len(expected)
    manifest = json.loads((root / 'SOURCE_MANIFEST.json').read_text())
    for name, expected in manifest.items():
        assert hashlib.sha256((root / name).read_bytes()).hexdigest() == expected, name
    return dict(status='passed', timing_rows=len(rows), summary_values=values_checked,
                source_manifest_files=len(manifest))


if __name__ == '__main__':
    print(json.dumps(verify(Path(__file__).resolve().parent), indent=2))
