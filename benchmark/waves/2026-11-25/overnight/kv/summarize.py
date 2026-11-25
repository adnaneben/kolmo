import csv
import json
from pathlib import Path
from statistics import median

d=Path(__file__).resolve().parent
allrows=list(map(json.loads,(d/'observations.jsonl').read_text().splitlines()))
assert len(allrows)==288 and all(x['page_sha256']==x['readback_sha256'] for x in allrows)
with (d/'results.csv').open('w') as f:
    w=csv.writer(f)
    w.writerow(['tokens','codec','raw_bytes_six_pages','frame_bytes','indexed_bytes','indexed_raw_pct','enc_MBps','dec_MBps','combined_MBps','bytes_per_8token_requested_byte'])
    for n in (8,16,30):
        for arm in ('kolmo-L1','kolmo-L3','zstd-1','zstd-3'):
            rows=[r for r in allrows if r['tokens']==n and r['codec']==arm]
            bypage={(r['kind'],r['layer']) for r in rows}
            assert len(bypage)==6
            raw=size=0
            for k,l in bypage:
                samples=[r for r in rows if (r['kind'],r['layer'])==(k,l)]
                assert sorted(r['rep'] for r in samples)==[0,1,2,3]
                assert len({r['frame_sha256'] for r in samples})==1
                raw+=samples[0]['raw_bytes'];size+=samples[0]['frame_bytes']
            def phase(keys):
                return median(raw/1e6/sum(r[k] for r in rows if r['rep']==rep for k in keys) for rep in (1,2,3))
            indexed=size+6*16
            w.writerow([n,arm,raw,size,indexed,round(100*indexed/raw,3),
                       round(phase(['encode_s']),3),round(phase(['decode_s']),3),round(phase(['encode_s','decode_s']),3),
                       round(indexed/(6*8*1024*2),4)])
