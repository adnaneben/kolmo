import csv
import json
import sys
from pathlib import Path
from statistics import median
d=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parent
scored = int(sys.argv[2]) if len(sys.argv)>2 else 5
r=list(map(json.loads,(d/'observations.jsonl').read_text().splitlines()))
counts=list(map(json.loads,(d/'counters.jsonl').read_text().splitlines()))
micro_path = d/'classifier.csv'
micro=list(csv.DictReader(micro_path.open())) if micro_path.exists() else []
assert len(r)==(scored+1)*3*len(counts)
with (d/'results.csv').open('w') as f:
 w=csv.writer(f);w.writerow(['case','level','threads','input','frame','worker_hashes','parse_hashes','reference_enc_MBps','trial_enc_MBps','reference_dec_MBps','trial_dec_MBps','reference_combined_MBps','trial_combined_MBps','paired_cpu_encode_time_pct','aa_cpu_time_pct','paired_combined_speed_pct','aa_speed_pct','micro_reference_Gclasses_s','micro_trial_Gclasses_s'])
 for c in counts:
  rows=[x for x in r if x['case']==c['case']]
  assert all(x['input_sha256']==x['readback_sha256'] and x['frame_sha256']==c['frame_sha256'] for x in rows)
  a,a2,b=([x for x in sorted(rows,key=lambda y:y['rep']) if x['rep'] and x['arm']==arm] for arm in ('A','A2','B'))
  assert list(map(len,(a,a2,b)))==[scored]*3
  rate=lambda arr,parts:round(median(x['n']/1e6/sum(x[y] for y in parts) for x in arr),3)
  total=lambda x:x['enc']+x['dec']
  paired=median(100*((total(x)+total(y))/(2*total(z))-1) for x,y,z in zip(a,a2,b))
  aa=median(100*(total(x)/total(y)-1) for x,y in zip(a,a2))
  cpu=median(100*(2*z['enc_cpu']/(x['enc_cpu']+y['enc_cpu'])-1) for x,y,z in zip(a,a2,b))
  aacpu=median(100*(y['enc_cpu']/x['enc_cpu']-1) for x,y in zip(a,a2))
  cells=[x for x in micro if x['case']==c['case'] and int(x['rep'])]
  micros=[round(median((int(x['bytes'])-7)/float(x['wall_s'])/1e9 for x in cells if x['arm']==arm),4) for arm in ('0','2')] if cells else ['','']
  w.writerow([c['case'],c['level'],c['threads'],a[0]['n'],a[0]['size'],c['worker_hashes'],c['parse_hashes'],
              *[rate(arr,ph) for ph in [('enc',),('dec',),('enc','dec')] for arr in [a,b]],round(cpu,3),round(aacpu,3),round(paired,3),round(aa,3),*micros])
