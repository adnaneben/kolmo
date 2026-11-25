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
arms=['pre','main','main-AA','combined']
records=[];paired=[]
def phase(r,p):
 return int(r['encode_ns'])+int(r['decode_ns']) if p=='combined' else int(r[p+'_ns'])
for (s,l,t),g in sorted(groups.items()):
 assert len(g)==(reps+1)*4
 for a in arms:
  cells=[g[a,i] for i in range(1,reps+1)]
  h={r['frame_sha256'] for r in cells};assert len(h)==1
  assert g['pre',0]['frame_sha256']==g['main',0]['frame_sha256']==g['main-AA',0]['frame_sha256']
  if s!='owid-co2.csv':assert list(h)[0]==g['pre',0]['frame_sha256']
  n=int(cells[0]['input_bytes']);size=int(cells[0]['stored_bytes'])
  result={'source':s,'level':l,'threads':t,'arm':a,'input_bytes':n,'stored_bytes':size,'ratio':n/size,'frame_sha256':next(iter(h)),'frame_codec_counts':cells[0]['codecs']}
  for p in ['encode','decode','combined']:
   result[p+'_MBps']=n*1000/statistics.median(phase(r,p) for r in cells)
   if p!='combined':result[p+'_peak_rss_median_B']=statistics.median(int(r[p+'_rss_B']) for r in cells)
  records.append(result)
 for a,b in [('pre','main'),('pre','combined'),('main','combined'),('main-AA','combined'),('main','main-AA')]:
  r={'source':s,'level':l,'threads':t,'baseline':a,'candidate':b,'paired_reps':reps,'size_delta_bytes':int(g[b,0]['stored_bytes'])-int(g[a,0]['stored_bytes'])}
  for p in ['encode','decode','combined']:
   gain=[phase(g[a,i],p)/phase(g[b,i],p)-1 for i in range(1,reps+1)]
   r[p+'_pct']=100*statistics.median(gain)
   r[p+'_wins']=sum(x>0 for x in gain)
  paired.append(r)
for name,table in [('score.csv',records),('paired.csv',paired)]:
 with (root/name).open('w',newline='') as f:
  w=csv.DictWriter(f,fieldnames=table[0]);w.writeheader();w.writerows(table)
lines=['# Combined experimental build against main','',
       'The build at 38d512f5 combines the native Gear archive scanner, decoder/writer fixes, optional typed-table routing, KLZ capacity changes and the offline learning tooling. This is a research build. Dynamic policy training does not run on every input. The experiment compares a historical main, qualified main, a second copy of the same main executable, and the combined build. Each setting includes a warmup and eleven shuffled scored trials of every arm. Timed runs require byte-exact recovery, and the warmup cross-checks each emitted frame/archive with the stock main reader. The enwik8 archive also checks stored mode and mtime.','',
       '| Input | Level | T | Stored B pre / main / combined | Encode MB/s pre / main / combined | Decode MB/s pre / main / combined | Combined MB/s pre / main / combined | Paired combined Δ vs main / AA | A/A spread |',
       '|---|---|---:|---:|---:|---:|---:|---:|---:|']
for k in sorted(groups):
 s,l,t=k
 by={r['arm']:r for r in records if (r['source'],r['level'],r['threads'])==k}
 pairs={(r['baseline'],r['candidate']):r for r in paired if (r['source'],r['level'],r['threads'])==k}
 vals=lambda fld,fmt:' / '.join(format(by[a][fld],fmt) for a in ['pre','main','combined'])
 pct=lambda a,b:f"{pairs[a,b]['combined_pct']:+.1f}%"
 lines.append(f"| {s} | {l} | {t} | {vals('stored_bytes',',')} | {vals('encode_MBps','.2f')} | {vals('decode_MBps','.2f')} | {vals('combined_MBps','.2f')} | {pct('main','combined')} / {pct('main-AA','combined')} | {pct('main','main-AA')} |")
lines+=['','Original input hashes, executable digests and settings are in manifest.json. raw.csv preserves all phase times, RSS, CPU and order. score.csv reports independent medians per phase. paired.csv reports each repeated within-round comparison, including combined vs the historical baseline, raw byte changes and win counts. The CLI was built on an Apple M5; these warm-cache, shared-host timings cannot establish native x86 throughput or generalize to unrelated producers and sizes. Requested thread count is recorded; actual active worker counts vary.']
(root/'README.md').write_text('\n'.join(lines)+'\n')
