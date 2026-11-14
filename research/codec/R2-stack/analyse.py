#!/usr/bin/env python3
"""Strict R2 factorial analysis; archived packing union is OFFLINE, not a joint encoder."""
from __future__ import annotations
import argparse,collections,hashlib,json,statistics,zipfile
from pathlib import Path
REGIMES=('encode_residual_to_frame','decode_warm_verified_full_input','decode_displaced32MiB_verified_full_input')
def sha(b):return hashlib.sha256(b).hexdigest()
def save(p,x):p.write_text(json.dumps(x,indent=2)+'\n')
def interaction(costs,a,b,other=0):
 c0,ca,cb,cab=(costs[k] for k in (other,other|a,other|b,other|a|b))
 if c0<=0:raise ValueError('positive baseline required')
 return dict(a=a,b=b,other=other,baseline=c0,a_bytes=ca,b_bytes=cb,ab_bytes=cab,
             additive_extra_saving_bytes=ca+cb-cab-c0,
             multiplicative_null_bytes=ca*cb/c0,
             extra_vs_multiplicative_bytes=ca*cb/c0-cab)
def load_pass(path):
 status=json.loads((path/'status.json').read_text())
 if status['status']!='passed_development_NOT_RELEASE' or status['cases']!=58 or status['configurations']!=522:raise ValueError('incomplete experiment')
 records=[json.loads(l) for l in (path/'measurements.jsonl').read_text().splitlines()]
 by={(r['id'],r['policy']):r for r in records}
 if len(by)!=522 or any(not all((i,p) in by for p in range(9)) for i,p in by):raise ValueError('incomplete factorial')
 ts=collections.defaultdict(list)
 for l in (path/'timings.jsonl').read_text().splitlines():
  t=json.loads(l);ts[t['id'],t['policy'],t['regime']].append(t)
 count=0
 for (i,p),r in by.items():
  f=(path/i/f'policy-{p}.frame').read_bytes()
  if len(f)!=r['bytes'] or sha(f)!=r['frame_sha256']:raise ValueError('frame mismatch')
  if not status['correctness_only']:
   for regime in REGIMES:
    x=ts[i,p,regime]
    if len(x)!=status['rounds']:raise ValueError('timing repetitions')
    for field in ('wall_ns','cpu_ns'):
     if statistics.median(t[field]/t['batch'] for t in x)!=r[regime+'_'+field]:raise ValueError('median mismatch')
     count+=1
 return by,status,count
GROUPS={'C_real':lambda x:x['source']=='C' and x['kind']=='real_source_derived',
        'D_full':lambda x:x['source']=='D' and x['size_class']=='full',
        'D_small128':lambda x:x['source']=='D' and x['size_class']=='small128',
        'D_small1024':lambda x:x['source']=='D' and x['size_class']=='small1024',
        'synthetic':lambda x:x['kind']=='synthetic'}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--first',type=Path,required=True);p.add_argument('--confirmation',type=Path,required=True);p.add_argument('--sanitized',type=Path,required=True);p.add_argument('--archive',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 first,fs,n1=load_pass(a.first);second,ss,n2=load_pass(a.confirmation);san,st,_=load_pass(a.sanitized)
 if first.keys()!=second.keys() or first.keys()!=san.keys():raise ValueError('case mismatch')
 fixed=('bytes','payload_bytes','frame_sha256','residual_sha256','input_sha256','actual_codec','entropy_calls','huf_failed','rans_failed','planes')
 for k,r in first.items():
  if any(r[f]!=second[k][f] or r[f]!=san[k][f] for f in fixed):raise ValueError('repeat/compiler mismatch')
 with zipfile.ZipFile(a.archive) as z:
  old=[json.loads(l) for l in z.read('first/measurements.jsonl').decode().splitlines()];packing={r['id']:r['stored_bytes'] for r in old if r['policy']=='sampled_optional'}
 result=dict(status='passed_development_interaction_audit_NOT_VALIDATION',frame_hashes_checked=3*len(first),medians_checked=n1+n2,configurations_per_pass=len(first),timing_batches=fs['timing_batches']+ss['timing_batches'],groups={},families={},noise={})
 def summarize(ids):
  totals={p:sum(first[i,p]['bytes'] for i in ids) for p in range(9)};eligible=[i for i in ids if first[i,0]['planes']>1]
  times={}
  for label,data in [('first',first),('confirmation',second)]:
   times[label]={regime:{p:sum(data[i,p][regime+'_wall_ns'] for i in ids) for p in range(9)} for regime in REGIMES}
  pairs=[interaction(totals,a,b,other) for a,b,other in ((1,2,0),(1,2,4),(1,4,0),(1,4,2),(2,4,0),(2,4,1))]
  return dict(cases=len(ids),input_bytes=sum(first[i,0]['input_bytes'] for i in ids),bytes=totals,
              relative_change_pct={p:100*(totals[p]/totals[0]-1) for p in totals},
              pair_interactions=pairs,conditional_saving_at_full_stack={name:totals[7^bit]-totals[7] for name,bit in [('H',1),('R',2),('S',4)]},
              eligible=dict(cases=len(eligible),baseline_bytes=sum(first[i,0]['bytes'] for i in eligible),joint_bytes=sum(first[i,7]['bytes'] for i in eligible)),
              timing_sum_input_medians_ns=times,
              expanding_cases={p:sum(first[i,p]['bytes']>first[i,0]['bytes'] for i in ids) for p in range(9)},
              offline_alternative_union_with_E03_packing_bytes=sum(min(first[i,8]['bytes'],packing[i]) for i in ids),
              offline_union_warning='post-hoc alternatives on same observed cases; no sequential composition, no selector timing')
 for name,pred in GROUPS.items():
  ids=[i for (i,p),r in first.items() if p==0 and pred(r)]
  if not ids:raise ValueError('empty group '+name)
  result['groups'][name]=summarize(ids)
 for family in sorted({r['family'] for (i,p),r in first.items() if p==0 and r['kind']=='real_source_derived'}):
  ids=[i for (i,p),r in first.items() if p==0 and r['family']==family and r['size_class']=='full']
  result['families'][family]=summarize(ids)
 for label,data in [('first',first),('confirmation',second)]:
  result['noise'][label]={}
  for regime in REGIMES[1:]:
   ratios=[r[regime+'_wall_ns']/data[i,0][regime+'_wall_ns'] for (i,p),r in data.items() if p and r['frame_sha256']==data[i,0]['frame_sha256']]
   result['noise'][label][regime]=dict(identical_frame_pairs=len(ratios),min_ratio=min(ratios),max_ratio=max(ratios),median_ratio=statistics.median(ratios))
 save(out/'RESULTS.json',result)
 print(json.dumps({k:{'bytes':v['bytes'],'conditional':v['conditional_saving_at_full_stack'],'union':v['offline_alternative_union_with_E03_packing_bytes']} for k,v in result['groups'].items()},indent=2))
if __name__=='__main__':main()
