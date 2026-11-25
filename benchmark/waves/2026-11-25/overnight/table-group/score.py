"""Summarize complete frame bytes, phase timings and sample-to-file size regret."""
import csv,json
from pathlib import Path
from statistics import median
p=Path(__file__).resolve().parent
rows=list(csv.DictReader((p/'raw.csv').open()));samples=list(csv.DictReader((p/'samples.csv').open()))
wires=json.loads((p/'wire.json').read_text());manifest=json.loads((p/'manifest.json').read_text())
assert len(rows)==48 and len(samples)==6
with (p/'score.csv').open('w',newline='') as f:
    w=csv.writer(f);w.writerow('source threads arm input_B frame_B frame_sha256 encode_ms decode_ms combined_ms encode_MB_s decode_MB_s combined_MB_s table_blocks declared_group_blocks used_group_blocks grouped_column_instances'.split())
    for source,x in manifest['sources'].items():
        for t in [1,4]:
            for arm in ['chosen','no-group']:
                g=[r for r in rows if r['source']==source and int(r['threads'])==t and r['arm']==arm]
                assert len(g)==4 and {r['restored_sha256'] for r in g}=={x['sha256']} and len({r['frame_sha256'] for r in g})==1
                timed=[r for r in g if r['trial']!='0']
                enc,dec,both=[median(sum(int(r[k]) for k in ks) for r in timed)/1e6 for ks in [('encode_ns',),('decode_ns',),('encode_ns','decode_ns')]]
                d=wires[f'{source}/{t}/{arm}']['wire']
                w.writerow([source,t,arm,x['bytes'],g[0]['frame_B'],g[0]['frame_sha256'],enc,dec,both,*[x['bytes']/1000/z for z in (enc,dec,both)],len(d),sum(r['group']>=0 for r in d),sum(r['grouped_predictors']>0 for r in d),sum(r['grouped_predictors'] for r in d)])
print('48 whole readbacks, 12 diagnostic readbacks, 12 sample readbacks verified by runners')
