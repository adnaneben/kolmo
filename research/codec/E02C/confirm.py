#!/usr/bin/env python3
"""Compare stored bytes, frame hashes, corpus and capture identities across runs."""
import argparse,csv,json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('first',type=Path);p.add_argument('second',type=Path);a=p.parse_args()
def rows(path):
    with (path/'measurements.csv').open() as f:
        values=list(csv.DictReader(f))
    result={(r['case'],r['method'],r['backend']):(r['stored_bytes'],r['frame_sha256']) for r in values}
    if len(result)!=len(values):raise RuntimeError('Duplicate configuration')
    return result
r1,r2=rows(a.first),rows(a.second)
if r1!=r2:raise RuntimeError('Stored-size or frame-identity mismatch between runs')
for name in ('corpus_manifest.json','capture_hashes.json'):
    if json.loads((a.first/name).read_text())!=json.loads((a.second/name).read_text()):raise RuntimeError('Corpus/capture changed')
print(json.dumps({'status':'passed_repeat_size_and_frame_identity','configurations':len(r1),'scope':'repeat on same development data/host, not independent validation'}))
