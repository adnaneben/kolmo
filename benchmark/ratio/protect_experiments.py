#!/usr/bin/env python3
"""Keep KRC1, the measured Metal implementation, and prior raw evidence intact."""
import hashlib
import json
import pathlib
import tarfile

root = pathlib.Path(__file__).resolve().parents[2]
checkpoint = root/'benchmark/checkpoints/independent-gpu-v1'
manifest = json.loads((checkpoint/'manifest.json').read_text())
sha = lambda data: hashlib.sha256(data).hexdigest()
assert sha((checkpoint/'source.tar.gz').read_bytes()) == manifest['source_archive_sha256']
with tarfile.open(checkpoint/'source.tar.gz') as archive:
    for entry in manifest['files']:
        saved = archive.extractfile(entry['path']).read()
        assert sha(saved) == entry['sha256']
        path = root/entry['path']
        if entry['path'] in ['Makefile', 'README.md', '.github/workflows/ci.yml']:
            assert path.read_bytes().startswith(saved), path
        elif entry['path'].startswith('src/experimental/') or entry['path'].startswith(('tests/', 'bench/')):
            assert sha(path.read_bytes()) == entry['sha256'], path
# Reports and plans may evolve; the immutable archive retains their old versions.
previous = json.loads((root/'benchmark/next/final-manifest.json').read_text())
for entry in previous['files_sha256']:
    if entry['path'].endswith(('.csv', '.stderr', '.txt', '.json', '.metal')):
        assert sha((root/entry['path']).read_bytes()) == entry['sha256'], entry['path']
print('KRC1/GPU SOURCE AND PREVIOUS RAW EVIDENCE GATES PASSED')
