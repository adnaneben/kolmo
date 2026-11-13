"""Independently derive ratios, throughput, resident memory and matched gaps."""
from __future__ import annotations
import argparse,collections,hashlib,json,math,pathlib,statistics
MiB=1048576

def finite(x):
 if isinstance(x,bool) or not isinstance(x,(int,float)) or not math.isfinite(x) or x<0:raise ValueError('invalid measurement')
 return float(x)
def charged(r):
 w=finite(r['wall_ns'])/r['loops']/1e9;c=finite(r['cpu_ns'])/r['loops']/1e9
 return c if c>1.03*w else w
def costs(e,d,size):
 return dict(speed=charged(e)+charged(d)+size/125_000_000,
             archive=(charged(e)+3.3*charged(d))*(3.89/72)/3600+size/1e9*0.023)
def metrics(rows):
 if not rows:raise ValueError('empty cohort')
 U=sum(x['input_bytes'] for x in rows);S=sum(x['stored_bytes'] for x in rows)
 E=sum(x['encode_s'] for x in rows);D=sum(x['decode_s'] for x in rows)
 if S<=0 or E<=0 or D<=0:raise ValueError('nonpositive aggregate')
 return dict(inputs=len(rows),original_bytes=U,stored_bytes=S,ratio=U/S,encode_mib_s=U/MiB/E,decode_mib_s=U/MiB/D,
             encode_s=E,decode_s=D,enc_avg_mib=statistics.mean(x['enc_avg_mib'] for x in rows),enc_peak_mib=max(x['enc_peak_mib'] for x in rows),
             dec_avg_mib=statistics.mean(x['dec_avg_mib'] for x in rows),dec_peak_mib=max(x['dec_peak_mib'] for x in rows),
             speed_cost=sum(x['speed_cost'] for x in rows),archive_cost=sum(x['archive_cost'] for x in rows))
def delta(a,b):
 return dict(size_saving=1-a['stored_bytes']/b['stored_bytes'],encode_gain=a['encode_mib_s']/b['encode_mib_s']-1,
             decode_gain=a['decode_mib_s']/b['decode_mib_s']-1,enc_avg_saving=1-a['enc_avg_mib']/b['enc_avg_mib'],
             enc_peak_saving=1-a['enc_peak_mib']/b['enc_peak_mib'],dec_avg_saving=1-a['dec_avg_mib']/b['dec_avg_mib'],dec_peak_saving=1-a['dec_peak_mib']/b['dec_peak_mib'],
             speed_saving=1-a['speed_cost']/b['speed_cost'],archive_saving=1-a['archive_cost']/b['archive_cost'])
def rival(group):
 return 'ozwhole_9' if group=='Numeric arrays' else 'ozcsv_9' if 'CSV' in group else 'ozexact_9' if group=='Audio samples' else 'ozserial_9'
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--results',type=pathlib.Path,required=True);ap.add_argument('--inputs',type=pathlib.Path,required=True);a=ap.parse_args();root=a.results
 done=json.loads((root/'COMPLETE.json').read_text());assert done['status']=='executed'
 inputs={x['id']:x for x in json.loads((root/'INPUTS.json').read_text())};arms={x['id']:x for x in json.loads((root/'REGISTRY.json').read_text())}
 events=[json.loads(l) for l in (root/'EVENTS.jsonl').read_text().splitlines()];seen=set();gen={};times=collections.defaultdict(dict);mem=collections.defaultdict(dict);bad=[]
 for x in inputs.values():
  b=(a.inputs/x['file']).read_bytes();assert len(b)==x['input_bytes'] and hashlib.sha256(b).hexdigest()==x['input_sha256']
 for e in events:
  k=(e['input_id'],e['arm']);assert k[0] in inputs and k[1] in arms
  ident=(e['phase'],*k,e.get('op'),e.get('round'));assert ident not in seen,ident;seen.add(ident)
  if e['status']!='OK':bad.append(e)
  if e['phase']=='generation':gen[k]=e;continue
  if e['status']!='OK':continue
  assert e['input']==inputs[k[0]]['input_bytes'] and e['stored']==gen[k]['stored']
  assert e['loops']>0 and finite(e['wall_ns'])>0 and finite(e['cpu_ns'])>0
  assert e['out']==(e['stored'] if e['op']=='encode' else e['input'])
  if e['phase']=='timing':times[k][e['op'],e['round']]=e
  elif e['phase']=='memory':
   tr=root/f"rss-{k[0]}--{k[1]}--{e['op']}.json";b=tr.read_bytes();assert hashlib.sha256(b).hexdigest()==e['trace_sha256'];ss=json.loads(b)
   assert ss and e['rss_samples']==len(ss)
   integral=0
   for i,(t,r) in enumerate(ss):
    assert e['begin_ns']<=t<=e['end_ns'];finite(r)
    tt=ss[i+1][0] if i+1<len(ss) else e['end_ns'];assert tt>=t;integral+=(tt-t)*r
   avg=integral/(e['end_ns']-e['begin_ns']);assert math.isclose(avg,e['avg_rss_kib'],rel_tol=1e-10)
   assert e['peak_rss_kib']>=max(r for t,r in ss) and e['peak_rss_kib']>=e['phase_hwm_kib']
   mem[k][e['op']]=e
 per=[];hashes={};complete=[]
 expected=set()
 for iid,inp in inputs.items():
  suffix=pathlib.Path(inp['file']).suffix
  for arm,definition in arms.items():
   scope=definition['scope']
   applicable=(scope=='all' or scope=='csv' and suffix=='.csv' or scope=='numeric' and suffix=='.npy' or scope=='typed' and suffix in ('.npy','.wav'))
   if applicable:expected.add((iid,arm))
 assert set(gen)==expected,('missing/extra generation cases',sorted(expected-set(gen)),sorted(set(gen)-expected))
 for k,g in gen.items():
  if g['status']!='OK':continue
  b=(root/'frames'/f'{k[0]}--{k[1]}.bin').read_bytes();assert len(b)==g['stored'];h=hashlib.sha256(b).hexdigest();hashes['--'.join(k)]=h
  ts=times[k];ms=mem[k]
  if len(ts)!=2*done['rounds'] or set(ms)!={'encode','decode'}:continue
  for op in ('encode','decode'):
   assert {rnd for oo,rnd in ts if oo==op}==set(range(done['rounds']))
  es=[ts['encode',i] for i in range(done['rounds'])];ds=[ts['decode',i] for i in range(done['rounds'])]
  cs=[costs(e,d,len(b)) for e,d in zip(es,ds)]
  x=dict(input_id=k[0],arm=k[1],group=inputs[k[0]]['group'],kind=arms[k[1]]['kind'],input_bytes=g['input'],stored_bytes=len(b),frame_sha256=h,
         frame_version=b[4] if b[:4]==b'CZN1' else None,ratio=g['input']/len(b),
         encode_s=statistics.median(e['wall_ns']/e['loops']/1e9 for e in es),decode_s=statistics.median(d['wall_ns']/d['loops']/1e9 for d in ds),
         enc_avg_mib=ms['encode']['avg_rss_kib']/1024,enc_peak_mib=ms['encode']['peak_rss_kib']/1024,
         dec_avg_mib=ms['decode']['avg_rss_kib']/1024,dec_peak_mib=ms['decode']['peak_rss_kib']/1024,
         speed_cost=statistics.median(c['speed'] for c in cs),archive_cost=statistics.median(c['archive'] for c in cs),pair_costs=cs)
  x['encode_mib_s']=x['input_bytes']/MiB/x['encode_s'];x['decode_mib_s']=x['input_bytes']/MiB/x['decode_s'];per.append(x);complete.append(k)
 by={(x['input_id'],x['arm']):x for x in per};dups=[]
 for iid in inputs:
  if (iid,'zstd_3') in by and (iid,'zstd_3_duplicate') in by:
   x,y=by[iid,'zstd_3'],by[iid,'zstd_3_duplicate'];assert x['frame_sha256']==y['frame_sha256'];dups.append(dict(input_id=iid,encode_ratio=y['encode_s']/x['encode_s'],decode_ratio=y['decode_s']/x['decode_s'],cost_ratio=y['speed_cost']/x['speed_cost']))
 groups=sorted(set(x['group'] for x in inputs.values()));summary=[];comparisons=[];release_comparison=[]
 for group in groups:
  scope=[x['id'] for x in inputs.values() if x['group']==group]
  for arm in arms:
   rows=[by[i,arm] for i in scope if (i,arm) in by]
   if not rows:continue
   summary.append(dict(group=group,arm=arm,kind=arms[arm]['kind'],coverage=len(rows),registered_inputs=len(scope),**metrics(rows)))
   if not arm.startswith(('rc_','release_')):continue
   archive=any(t in arm for t in ['eco','hc','max','ultra','csv_archive']);ref=rival(group) if archive else 'zstd_3';cost='archive' if archive else 'speed'
   both=[i for i in scope if (i,arm) in by and (i,ref) in by]
   if both:
    x=metrics([by[i,arm] for i in both]);y=metrics([by[i,ref] for i in both]);wins=[]
    for i in both:
     c,r=by[i,arm],by[i,ref];wins.append(c['stored_bytes']<r['stored_bytes'] and all(cc[cost]<=rr[cost] for cc,rr in zip(c['pair_costs'],r['pair_costs'])))
    comparisons.append(dict(group=group,arm=arm,rival=ref,mode_cost=cost,comparable=len(both),expected=len(scope),wins=sum(wins),candidate=x,competitor=y,gaps=delta(x,y)))
   if arm.startswith('rc_'):
    base='release_'+arm[3:];both=[i for i in scope if (i,arm) in by and (i,base) in by]
    if both:
     x=metrics([by[i,arm] for i in both]);y=metrics([by[i,base] for i in both]);release_comparison.append(dict(group=group,arm=arm,baseline=base,comparable=len(both),candidate=x,competitor=y,gaps=delta(x,y)))
 result=dict(complete=done,case_counts=dict(generation=len(gen),fully_measured=len(complete),events=len(events),timed=sum(e['phase']=='timing' for e in events),memory=sum(e['phase']=='memory' for e in events)),failures=bad,duplicates=dups,summary=summary,comparisons=comparisons,release_comparisons=release_comparison,per_file=per,frames=hashes)
 (root/'AUDIT.json').write_text(json.dumps(result,indent=2)+'\n')
 print(json.dumps(result['case_counts']));print('statuses',collections.Counter(x['status'] for x in bad))
 assert sum(x['status']=='OK' for x in gen.values())==done['successful_generation']
 if len(complete)!=done['successful_generation']:print('INCOMPLETE CASES: inspect failures; excluded from comparisons but retained in ledger')
if __name__=='__main__':main()
