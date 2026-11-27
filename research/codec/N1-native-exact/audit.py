#!/usr/bin/env python3
"""Recompute the compact native-comparator ledger without rerunning codecs."""
import csv, hashlib, io, json, lzma, pathlib, statistics

def audit(root):
    schema=json.loads((root/'schema.json').read_text());packed=(root/'timings.csv.xz').read_bytes();raw=lzma.decompress(packed)
    assert hashlib.sha256(raw).hexdigest()==schema['raw_sha256']
    assert hashlib.sha256(packed).hexdigest()==schema['compressed_sha256']
    samples=list(csv.DictReader(io.StringIO(raw.decode())))
    for r in samples:
        for k in ('pass','round','wall_ns','cpu_ns'):r[k]=int(r[k])
    assert len(samples)==672
    summary=json.loads(lzma.decompress((root/'summary.json.xz').read_bytes()));assert len(summary)==56
    for s in summary:
        for pid in (0,1):
            for op in ('encode','decode'):
                rows=[r for r in samples if r['input']==s['input'] and r['policy']==s['policy'] and r['pass']==pid and r['op']==op]
                assert len(rows)==3 and {r['round'] for r in rows}=={0,1,2}
                for metric in ('wall_ns','cpu_ns'):
                    assert statistics.median(r[metric] for r in rows)==s['pass'+str(pid)][op][metric]
    totals={p:sum(s['bytes'] for s in summary if s['policy']==p) for p in schema['policies']}
    for name in schema['inputs']:
        rows={s['policy']:s for s in summary if s['input']==name}
        assert rows['exact9']['sha256']==rows['exact9_duplicate']['sha256']
    return {'samples':len(samples),'configurations':len(summary),'medians_verified':56*2*2*2,'bytes':totals,'exact9_vs_whole9_pct':100*(totals['exact9']/totals['whole9']-1)}
if __name__=='__main__':print(json.dumps(audit(pathlib.Path(__file__).parent/'results'),indent=2))
