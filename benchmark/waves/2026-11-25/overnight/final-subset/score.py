"""Score matched trials of the complete integrated binary, print a human-readable handoff."""
import csv,json,statistics
from collections import defaultdict
from pathlib import Path
root=Path(__file__).resolve().parent
meta=json.loads((root/'manifest.json').read_text())
reps=meta['rounds']; rows=list(csv.DictReader((root/'raw.csv').open())); groups=defaultdict(dict)
for r in rows:
 k=(r['source'],r['level'],int(r['threads']))
 sub=(r['arm'],int(r['repeat']))
 assert sub not in groups[k]
 assert r['sha256']==meta['sources'][k[0]]['input_sha256']
 groups[k][sub]=r
assert len(groups)==len(meta['settings'])
arms=['main','main-AA','combined']
records=[];paired=[]
def phase(r,p):
 return int(r['encode_ns'])+int(r['decode_ns']) if p=='combined' else int(r[p+'_ns'])
for (s,l,t),g in sorted(groups.items()):
 assert len(g)==(reps+1)*3
 for a in arms:
  cells=[g[a,i] for i in range(1,reps+1)]
  h={r['frame_sha256'] for r in cells};assert len(h)==1
  assert g['main',0]['frame_sha256']==g['main-AA',0]['frame_sha256']
  if s!='owid-co2.csv':assert list(h)[0]==g['main',0]['frame_sha256']
  n=int(cells[0]['input_bytes']);size=int(cells[0]['stored_bytes'])
  result={'source':s,'level':l,'threads':t,'arm':a,'input_bytes':n,'stored_bytes':size,'ratio':n/size,'frame_sha256':next(iter(h)),'frame_codec_counts':cells[0]['codecs']}
  for p in ['encode','decode','combined']:
   result[p+'_MBps']=n*1000/statistics.median(phase(r,p) for r in cells)
   if p!='combined':result[p+'_peak_rss_median_B']=statistics.median(int(r[p+'_rss_B']) for r in cells)
  records.append(result)
 for a,b in [('main','combined'),('main-AA','combined'),('main','main-AA')]:
  r={'source':s,'level':l,'threads':t,'baseline':a,'candidate':b,'paired_reps':reps,'size_delta_bytes':int(g[b,0]['stored_bytes'])-int(g[a,0]['stored_bytes'])}
  for p in ['encode','decode','combined']:
   gain=[phase(g[a,i],p)/phase(g[b,i],p)-1 for i in range(1,reps+1)]
   r[p+'_pct']=100*statistics.median(gain)
   r[p+'_wins']=sum(x>0 for x in gain)
  paired.append(r)
for name,table in [('score.csv',records),('paired.csv',paired)]:
 with (root/name).open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=table[0]);w.writeheader();w.writerows(table)

lines=['# Final candidate subset', '',
       'Fresh comparison of qualified main a1d39559 with the opt-in combined build from 5af8c465. Each of the three requested settings had a warmup and eleven scored main/main-AA/candidate triplets. Every timed arm restores the original bytes. Each warmup additionally checks the frame or archive with the stock main decoder. Absolute sizes, phase throughput, RSS and complete frame hashes are in score.csv; within-pair changes and win counts are in paired.csv. This is a separate run from the four-way 38d512f5 matrix.','',
       '| Workload and options | Bytes main → final | Encode MB/s main → final | Decode MB/s main → final | Combined MB/s main → final | Paired Δ vs main / main-AA | Same-binary Δ |',
       '|---|---:|---:|---:|---:|---:|---:|']
old=list(csv.DictReader((root.parent/'combined'/'score.csv').open()))
for k in sorted(groups):
 s,l,t=k
 arms={r['arm']:r for r in records if (r['source'],r['level'],r['threads'])==k}
 prior=next(r for r in old if (r['source'],r['level'],int(r['threads']),r['arm'])==(s,l,t,'combined'))
 assert prior['frame_sha256']==arms['combined']['frame_sha256']
 pairs={(r['baseline'],r['candidate']):r for r in paired if (r['source'],r['level'],r['threads'])==k}
 fmt=lambda key,spec:' → '.join(format(arms[a][key],spec) for a in ['main','combined'])
 delta=lambda a,b:format(pairs[a,b]['combined_pct'],'+.2f')+'%'
 lines.append(f"| {s} L{l} T{t} | {fmt('stored_bytes',',')} | {fmt('encode_MBps','.2f')} | {fmt('decode_MBps','.2f')} | {fmt('combined_MBps','.2f')} | {delta('main','combined')} / {delta('main-AA','combined')} | {delta('main','main-AA')} |")
lines+=['','The complete compressed SHA-256 for the final candidate matched the earlier 38d512f5 candidate at all three settings. The archive and ordinary prose frames also matched qualified main. For CSV the opt-in produces a smaller v12-compatible frame. The program warms shared-host file cache; each operation includes launching the CLI and reading/writing files. Times and ratios are descriptive and localized to the settings and CPU recorded here.']
(root/'README.md').write_text('\n'.join(lines)+'\n')
