#!/usr/bin/env python3
"""Apply frozen R3 decision; never tune or silently substitute missing cases."""
from __future__ import annotations
import argparse,json
from pathlib import Path
FIELDS=('bytes','frame_sha256','input_sha256','residual_sha256','aligned','sampled_bytes','actual_codec','huf_chunks','rans_chunks','singleton_chunks')
def read(root):
 status=json.loads((root/'status.json').read_text())
 if status['status']!='passed_execution_NOT_RELEASE':raise ValueError('incomplete execution')
 records=list(map(json.loads,(root/'measurements.jsonl').read_text().splitlines()))
 rows={(r['id'],r['policy']):r for r in records}
 if len(rows)!=len(records) or any({p for i,p in rows if i==name}!={0,7,8,9,10} for name,p in rows):raise ValueError('incomplete or duplicate records')
 return rows
def family_pass(rows,family):
 ids=sorted(i for i,p in rows if p==0 and rows[i,p]['family']==family);base=[rows[i,0] for i in ids];cand=[rows[i,9] for i in ids]
 sums=lambda pp,field:sum(rows[i,pp][field] for i in ids)
 base_bytes=sums(0,'bytes');new_bytes=sums(9,'bytes');saving=base_bytes-new_bytes
 exercised=sum(r['planes']>1 for r in base)>=2
 expansion_ok=all((c['bytes']<=1.01*b['bytes'] if b['input_bytes']>=65536 else c['bytes']-b['bytes']<=64) for b,c in zip(base,cand))
 noise={x:sums(10,x+'_wall_ns')/sums(0,x+'_wall_ns') for x in ('encode','decode_verified_warm')}
 ratios={x:sums(9,x+'_wall_ns')/sums(0,x+'_wall_ns') for x in ('encode','decode_verified_warm','decode_kernel_warm','decode_verified_displaced32MiB','decode_kernel_displaced32MiB')}
 noise_ok=all(.9<=x<=1.1 for x in noise.values())
 checks={'applicability':exercised,'size':saving>=128 and saving>=base_bytes*.01,'per_file_regression':expansion_ok,'encode':ratios['encode']<=1.15,'decode_verified':ratios['decode_verified_warm']<=1.10,'decode_kernel':ratios['decode_kernel_warm']<=1.10,'noise':noise_ok}
 return {'inputs':ids,'baseline_bytes':base_bytes,'candidate_bytes':new_bytes,'saving_pct':100*saving/base_bytes,'aligned_inputs':[i for i in ids if rows[i,9]['aligned']],'timing_ratios':ratios,'noise_ratios':noise,'checks':checks,'supports':all(checks.values())}
def main():
 p=argparse.ArgumentParser();p.add_argument('first',type=Path);p.add_argument('confirmation',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
 x,y=read(a.first),read(a.confirmation)
 if set(x)!=set(y):raise ValueError('configuration mismatch')
 for key in x:
  if any(x[key][f]!=y[key][f] for f in FIELDS):raise ValueError('repeat identity '+str(key))
 families=sorted({r['family'] for r in x.values()});result={'repeat_identity':'passed','configurations_per_pass':len(x),'families':{},'scope':'fresh component validation, not production or release'}
 for family in families:
  first,confirmation=family_pass(x,family),family_pass(y,family)
  result['families'][family]={'first':first,'confirmation':confirmation,'supported_both':first['supports'] and confirmation['supports']}
 exercised=[v for v in result['families'].values() if v['first']['checks']['applicability'] and v['confirmation']['checks']['applicability']]
 supported=sum(v['supported_both'] for v in exercised)
 timing_inconclusive=any(not v['first']['checks']['noise'] or not v['confirmation']['checks']['noise'] for v in exercised)
 result['decision']='inconclusive_insufficient_applicable_families' if len(exercised)<2 else 'timing_inconclusive' if timing_inconclusive else 'supported_for_further_research' if supported>=2 else 'provisional_one_family' if supported==1 else 'not_supported_for_registered_profile'
 result['supported_families']=supported
 a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
