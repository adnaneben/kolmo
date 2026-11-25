"""Score complete A/A/B file CLI runs, preserving independent and within-triplet rates."""
import csv
import json
from collections import defaultdict
from pathlib import Path
from statistics import median

def score(path, repeats, target):
    groups=defaultdict(dict)
    with path.open() as f:
        for r in csv.DictReader(f):
            assert r['restored_sha256']==r['input_sha256']
            k=(r['input'],r['level'],int(r['threads']))
            sk=(r['arm'],int(r['rep']))
            assert sk not in groups[k]
            groups[k][sk]=r
    records=[]
    for (s,level,threads), indexed in sorted(groups.items()):
        assert len(indexed)==3*(repeats+1)
        n=int(indexed['main',0]['bytes_in'])
        row={'input':s,'level':level,'threads':threads,'input_bytes':n,'input_sha256':indexed['main',0]['input_sha256'],'scored_triplets':repeats}
        totals={}
        cpus={}
        for arm in ['main','main-AA','candidate']:
            allrows=[indexed[arm,i] for i in range(repeats+1)]
            sig={r['frame_sha256'] for r in allrows};assert len(sig)==1
            if arm=='main-AA':
                assert next(iter(sig))==row['main_frame_sha256']
            row[arm+'_frame_sha256']=next(iter(sig));row[arm+'_frame_bytes']=int(allrows[0]['bytes_out'])
            row[arm+'_ratio']=n/int(allrows[0]['bytes_out'])
            trials=allrows[1:]
            enc=[int(r['encode_ns']) for r in trials];dec=[int(r['decode_ns']) for r in trials];totals[arm]=[e+d for e,d in zip(enc,dec)]
            cpus[arm]=[float(r['encode_cpu_s'])+float(r['decode_cpu_s']) for r in trials]
            for phase,ns in [('encode',enc),('decode',dec),('combined',totals[arm])]:
                row[f'{arm}_{phase}_MBps']=n*1000/median(ns)
            for phase in ['encode','decode']:
                row[f'{arm}_{phase}_rss_B']=median(int(r[phase+'_rss_B']) for r in trials)
        row['frame_identical']=row['main_frame_sha256']==row['candidate_frame_sha256']
        row['byte_delta']=row['candidate_frame_bytes']-row['main_frame_bytes']
        for before,after,name in [('main','candidate','gain'),('main-AA','candidate','gain_vs_AA'),('main','main-AA','AA')]:
            vals=[a/b for a,b in zip(totals[before],totals[after])]
            row[name+'_paired_pct']=(median(vals)-1)*100
            row[name+'_wins']=sum(v>1 for v in vals)
            row[name+'_paired_cpu_pct']=(median(a/b for a,b in zip(cpus[before],cpus[after]))-1)*100
        records.append(row)
    with target.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=records[0]);w.writeheader();w.writerows(records)
    return records

if __name__=='__main__':
    root=Path(__file__).resolve().parent
    meta=json.loads((root/'manifest.json').read_text())
    results=score(root/'rounds.csv',meta['rounds'],root/'score.csv')
    assert len(results)==80
    print('Exact cells',sum(r['frame_identical'] for r in results),'of',len(results))
    for r in results:
        if not r['frame_identical'] or min(abs(r['gain_paired_pct']),abs(r['gain_vs_AA_paired_pct']))>10:
            print(r['input'],r['level'],r['threads'],r['byte_delta'], 'main / AA / AA-control %',
                  round(r['gain_paired_pct'],2),round(r['gain_vs_AA_paired_pct'],2),round(r['AA_paired_pct'],2))
