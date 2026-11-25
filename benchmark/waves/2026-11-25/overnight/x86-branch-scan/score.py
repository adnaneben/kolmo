"""Score the native kernel and complete CLI with paired identical-binary controls."""
import argparse,csv,json
from pathlib import Path
from statistics import median
def score(p):
 for kind in ['cli','micro']:
  with (p/(kind+'.csv')).open() as f:raw=list(csv.DictReader(f))
  cells={}
  for r in raw:
   if r.get('round')=='0':continue
   key=(r['source'],r.get('level',''),r.get('threads',''))
   arm={'0':'A','1':'AA','2':'B'}.get(r['arm'],r['arm'])
   cells.setdefault(key,{}).setdefault(arm,{})[int(r['round'])]=r
  out=[]
  for (source,l,t),g in sorted(cells.items()):
   row={'source':source,'level':l,'threads':t,'scored_triplets':9}
   for arm in ['A','AA','B']:assert set(g[arm])==set(range(1,10))
   if kind=='cli':assert len({(r['frame_sha256'],r['restored_sha256'],r['frame_B']) for arm in g.values() for r in arm.values()})==1
   for phase,cols in [('encode',['encode_ns']),('decode',['decode_ns']),('combined',['encode_ns','decode_ns'])]:
    totals={a:[sum(int(g[a][i][k]) for k in cols) for i in range(1,10)] for a in g}
    for left,right,name in [('B','A','B_vs_A'),('B','AA','B_vs_AA'),('AA','A','AA_vs_A')]:
     ratios=[before/after for before,after in zip(totals[right],totals[left])]
     row[name+'_'+phase+'_gain_pct']=(median(ratios)-1)*100
     row[name+'_'+phase+'_wins']=sum(x>1 for x in ratios)
    for arm in ['A','AA','B']:
     one=g[arm][1];n=int(one['input_B'])*int(one.get('iterations',1))
     row[arm+'_'+phase+'_MB_s']=n*1000/median(totals[arm])
   out.append(row)
  with (p/(kind+'-scores.csv')).open('w',newline='') as f:
   w=csv.DictWriter(f,fieldnames=out[0]);w.writeheader();w.writerows(out)
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('path',type=Path);score(a.parse_args().path)
