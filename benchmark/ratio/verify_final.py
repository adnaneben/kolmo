#!/usr/bin/env python3
"""Check corpus coverage, preservation, reproducibility and experiment integrity."""
import csv
import hashlib
import json
import pathlib
import subprocess
import sys
import tarfile

root = pathlib.Path(__file__).resolve().parents[2]
base = root/'benchmark/ratio'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
for name in ['cpu-planning-v1','independent-gpu-v1','byte-context-v1']:
    folder = root/'benchmark/checkpoints'/name
    m = json.loads((folder/'manifest.json').read_text())
    assert sha(folder/'source.tar.gz')==m['source_archive_sha256']
    with tarfile.open(folder/'source.tar.gz') as t:
        for item in m['files']:
            saved = t.extractfile(item['path']).read()
            assert hashlib.sha256(saved).hexdigest()==item['sha256']
            if name=='byte-context-v1':
                current = (root/item['path']).read_bytes()
                if item['path'] in ['Makefile','README.md','.github/workflows/ci.yml','src/experimental/KBC.md']:
                    assert current.startswith(saved), item['path']
                else:
                    assert current==saved, item['path']
subprocess.run([sys.executable,str(root/'benchmark/next/protect_gains.py'),'--sources-only'],check=True)
subprocess.run([sys.executable,str(base/'protect_experiments.py')],check=True)

expected = {'baseline-silesia':108,'development':250,'validation':126,'board':9,
            'scaling-1':4,'scaling-10':4,'ratio-leaders':1,'kolmo-ultra':1}
tables = {}
for stem,count in expected.items():
    rows = list(csv.DictReader((base/(stem+'.csv')).open()))
    assert len(rows)==count,(stem,len(rows))
    keys = set()
    for row in rows:
        key = row['Filename'],row['Compressor name']
        assert key not in keys;keys.add(key)
        assert int(row['Original size'])==(root/row['Filename']).stat().st_size
        assert int(row['Compressed size'])>0
        assert float(row['Compression speed'])>0 and float(row['Decompression speed'])>0
    tables[stem]=rows
    metadata = base/(stem+'.json')
    if metadata.exists() and stem not in ['kolmo-ultra']:
        m = json.loads(metadata.read_text())
        assert len(m['commands'])==count
        assert all(c['exit_code']==0 for c in m['commands'])
        assert m['lzbench_sha256']==sha(pathlib.Path('/tmp/kolmo-lzbench-audit/lzbench'))
        for item in m['inputs']:
            assert sha(root/item['path'])==item['sha256']
validation = json.loads((base/'validation-inputs.json').read_text())
assert len(validation['standard_calgary_files'])==14
assert {r['Filename'] for r in tables['validation']}==set(validation['standard_calgary_files'])
for item in validation['files']:
    assert sha(root/item['path'])==item['sha256']
one = {r['Compressor name']:r['Compressed size'] for r in tables['scaling-1']}
ten = {r['Compressor name']:r['Compressed size'] for r in tables['scaling-10']}
assert one==ten
assert int(tables['ratio-leaders'][0]['Compressed size'])==40470202
for name in ['krc.c','krc.h','kbc.c','kbc.h']:
    assert sha(root/'src/experimental'/name)==sha(pathlib.Path('/tmp/kolmo-lzbench-audit/bench/independent')/name)
gate = json.loads((base/'protected-gate.json').read_text())
assert gate['passed'] and gate['default_frames_identical'] and gate['repeats']>=5
assert len(gate['measurements'])==9 and max(r['elapsed_ratio'] for r in gate['measurements'])<=1.05
assert (base/'byte-context-tests.txt').read_text().count('KBC TESTS PASSED')==2
assert 'KBC CLI TESTS PASSED' in (base/'cli-tests.txt').read_text()
fresh = json.loads((base/'ultra-fresh-process.json').read_text())
assert fresh['fresh_process_verification'] and fresh['matches_lzbench_one_thread_byte_count']
assert fresh['frame_bytes']==int(tables['kolmo-ultra'][0]['Compressed size'])
assert sha(root/fresh['frame_path'])==fresh['frame_sha256']
assert sha(root/'benchmark/ratio/data/silesia.tar')==fresh['input_and_restored_sha256']
print('FINAL GATES PASSED: 503 successful lzbench configurations; frozen inputs; three immutable source archives; unchanged prior evidence; matching equal-block sizes; CPU/default regression gates')
