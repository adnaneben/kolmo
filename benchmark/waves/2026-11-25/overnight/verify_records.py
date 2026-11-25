"""Verify row counts, digests and comparability in the distributed evidence."""
from collections import defaultdict
import csv
import hashlib
import io
import json
from pathlib import Path
import tarfile
import zipfile
here = Path(__file__).resolve().parent
def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def unique_rows(items, key, n):
    assert len(items) == n
    assert len({r[key] for r in items}) == n

def archive_csv(archive, name):
    return list(csv.DictReader(io.TextIOWrapper(archive.open(name), encoding='utf8')))

assert digest(here/'audit/source-and-receipts.tar.gz') == '5abae9f183c0586a1548417590c30183aec949e00e8347f10e0f1e2a821788f5'
with tarfile.open(here/'audit/source-and-receipts.tar.gz') as pack:
    files = {Path(el.name).name:el for el in pack if el.isfile()}
    for name in ('assignment-audit.csv', 'legacy-claim-audit.csv', 'receipt-index.csv', 'reopened-gates.csv'):
        assert pack.extractfile(files[name]).read() == (here/'audit'/name).read_bytes(), name
with (here/'audit'/'reopened-gates.csv').open() as f:
    reopened = {row['lane'] for row in csv.DictReader(f)}
with (here/'audit'/'current-gates.csv').open() as f:
    current = list(csv.DictReader(f))
unique_rows(current, 'assignment', len(reopened))
assert {row['assignment'] for row in current} == reopened
for row in current:
    target = row['evidence']
    assert row['latest_disposition'] and row['current_evidence_or_open_work'] and target
    if not target.startswith('https://'):
        assert (here/'audit'/target).is_file(), target
for n, key, count in [('assignment-audit','lane',123),('legacy-claim-audit','claim_id',178),('reopened-gates','lane',31)]:
    with (here/'audit'/(n+'.csv')).open() as f:
        unique_rows(list(csv.DictReader(f)),key,count)
assert digest(here/'performance/before-after-and-competition.zip') == 'd91d66d02022334801d8957193b0427f0855c8c0b4c41a84a38fb27584c0b48c'
with zipfile.ZipFile(here/'performance/before-after-and-competition.zip') as pack:
    assert pack.testzip() is None
    for name, count in [('evidence',1920),('klzma-evidence',576)]:
        rows = archive_csv(pack, name+'/rounds.csv')
        assert len(rows) == count
        seen = defaultdict(set)
        for row in rows:
            assert row['input_sha256'] == row['restored_sha256']
            assert int(row['encode_ns']) > 0 and int(row['decode_ns']) > 0
            seen[(row['input'],row['level'],row['threads'])].add(row['frame_sha256'])
        assert all(len(v)==1 for v in seen.values())
    rivals = archive_csv(pack, 'evidence/competitors-raw.csv')
    assert len(rivals) == 384
    by_source = defaultdict(set)
    for row in rivals:
        by_source[row['input']].add(row['restored_sha256'])
    assert len(by_source)==6 and all(len(v)==1 for v in by_source.values())
print('Validated 123 assignment IDs, 178 claim IDs, 31 reopened gates, 2,880 timed rows and both evidence packs.')
