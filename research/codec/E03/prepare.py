#!/usr/bin/env python3
"""Reuse exact archived E02C/E02D inputs as OBSERVED DEVELOPMENT data."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
import zipfile

SOURCES = {
    'C': (13951053398, '83eee1ef7ba8dfd84bab172100dd538adcbdd095c75001705bd146854984d33b'),
    'D': (13955991234, '5b89de92cfab64d2baed3aa416affd83d329d7475bcb06132bd197f52df2a7a8'),
}
METHODS = {'upstream_layout': 0, 'global_bitpack': 1, 'local_bitpack': 2, 'patched': 3, 'local_byteplanes': 4}
NATIVE = {'stored', 'huffman', 'rans'}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n')


def collect(archive, label, out):
    artifact, digest = SOURCES[label]
    if sha(archive.read_bytes()) != digest:
        raise ValueError(f'{label}: archive digest mismatch')
    with zipfile.ZipFile(archive) as z:
        if len(z.namelist()) != len(set(z.namelist())):
            raise ValueError('Duplicate archive members')
        if label == 'D':
            for name, expected in json.loads(z.read('MANIFEST.json')).items():
                if sha(z.read(name)) != expected:
                    raise ValueError(f'E02D evidence member mismatch: {name}')
        prefix = 'first' if label == 'C' else 'prepared'
        manifest = json.loads(z.read(prefix + '/corpus_manifest.json'))
        captures = json.loads(z.read('first/capture_hashes.json'))
        rows = list(csv.DictReader(io.StringIO(z.read('first/measurements.csv').decode()))) if label == 'C' else [
            json.loads(x) for x in z.read('first/measurements.jsonl').decode().splitlines()]
        if len(manifest) != (25 if label == 'C' else 33) or len(rows) != len(manifest) * 20:
            raise ValueError('Incomplete previous matrix')
        cases = []
        for item in manifest:
            name = item['name']
            if not re.fullmatch('[a-zA-Z0-9_-]+', name):
                raise ValueError('Unsafe input name')
            data = z.read(prefix + '/corpus/' + name + '.npy')
            if sha(data) != item['sha256'] or len(data) != item['bytes'] or len(data) > 4 << 20:
                raise ValueError('Input identity/size mismatch')
            sizes = {}
            for row in rows:
                if row.get('case', row.get('name')) != name:
                    continue
                key = f"{METHODS[row['method']]}:{row['backend']}"
                if key in sizes:
                    raise ValueError('Duplicate previous matrix cell')
                sizes[key] = int(row['stored_bytes'])
            if len(sizes) != 20 or name not in captures:
                raise ValueError('Missing source matrix/capture')
            identifier = label.lower() + '__' + name
            (out / 'corpus' / (identifier + '.npy')).write_bytes(data)
            cases.append(dict(id=identifier, source=label, name=name, family=item['family'],
                kind=item.get('kind', 'real_source_derived'), size_class=item.get('size_class', 'full'),
                width=item['width'], head=item['head'], input_bytes=len(data), input_sha256=sha(data),
                capture=captures[name], previous_sizes=sizes, previous_metadata=item,
                partition='observed_development_NOT_fresh_validation'))
        return cases, dict(artifact_id=artifact, archive_sha256=digest, cases=len(cases),
                           previous_product_controls=json.loads(z.read('first/product_controls.json')))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--c-archive', type=Path)
    p.add_argument('--d-archive', type=Path)
    a = p.parse_args()
    out = a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    (out / 'corpus').mkdir()
    cases, receipts = [], {}
    for label, supplied in (('C', a.c_archive), ('D', a.d_archive)):
        archive = supplied
        if archive is None:
            archive = out / (label + '.zip')
            with archive.open('wb') as f:
                subprocess.run(['gh', 'api', f'repos/adnaneben/kolmo/actions/artifacts/{SOURCES[label][0]}/zip'],
                               stdout=f, check=True, timeout=180)
        items, receipt = collect(archive, label, out)
        cases.extend(items)
        receipts[label] = receipt
    if len(cases) != 58 or len({x['id'] for x in cases}) != 58:
        raise ValueError('Incomplete E03 corpus')
    save(out / 'inputs.json', cases)
    save(out / 'source_receipts.json', receipts)
    print('E03_PREPARED_58_OBSERVED_DEVELOPMENT_INPUTS')


if __name__ == '__main__':
    main()
