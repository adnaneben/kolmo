#!/usr/bin/env python3
"""Independent recomputation of S1 frames, sample completeness and paired costs."""
import argparse,hashlib,json,pathlib,statistics,sys
from run import validate
sha=lambda b:hashlib.sha256(b).hexdigest()
def charged(r):
 w=r['wall_ns']/r['batch']/1e9;c=r['cpu_ns']/r['batch']/1e9
 return c if c>w*1.03 else w

def main():
 p=argparse.ArgumentParser();p.add_argument('root',type=pathlib.Path);a=p.parse_args();root=a.root
 inputs=json.loads((root/'evidence/INPUTS.json').read_text());summary={};identities={};count=0
 for passname,nr in [('first',7),('confirmation',5)]:
  ss={};status=json.loads((root/passname/'STATUS.json').read_text());arms=status['arms']
  if status['status']!='PASS_DEVELOPMENT_EXECUTION_NOT_GENERAL_ACCEPTANCE':raise ValueError('incomplete run')
  for inp in inputs:
   name=inp['name'];data=(root/'inputs'/name).read_bytes()
   if sha(data)!=inp['sha256']:raise ValueError('input changed')
   dest=root/passname/name;items=[json.loads(x) for x in (dest/'stdout.jsonl').read_text().splitlines()];meta={r['arm']:r for r in items if r['kind']=='frame'};rows=[r for r in items if r['kind']=='sample'];validate(rows,meta,arms,nr,len(data));by={(r['arm'],r['op'],r['round']):r for r in rows};per={};count+=len(rows)
   for arm in arms:
    f=(dest/(arm+'.frame')).read_bytes();h=sha(f)
    if len(f)!=meta[arm]['bytes']:raise ValueError('frame size')
    key=(name,arm)
    if key in identities and identities[key]!=h:raise ValueError('repeat frame mismatch')
    identities[key]=h
    if sha((root/'sanitized'/name/(arm+'.frame')).read_bytes())!=h:raise ValueError('sanitizer bytes differ')
    e=[charged(by[arm,0,r]) for r in range(nr)];d=[charged(by[arm,1,r]) for r in range(nr)];cost=[e[r]+d[r]+len(f)/125000000 for r in range(nr)]
    per[arm]={'bytes':len(f),'sha256':h,'encode':e,'decode':d,'cost':cost,'medians':{'encode':statistics.median(e),'decode':statistics.median(d),'cost':statistics.median(cost)},'stages':meta[arm]['stages'],'groups':meta[arm]['groups'],'ow':meta[arm]['ow']}
   for v in per.values():v['vs_zstd3']={'smaller':v['bytes']<per['zstd3']['bytes'],'fits_all_pairs':all(x<=y for x,y in zip(v['cost'],per['zstd3']['cost']))}
   for arm,dup in [('zstd3','zstd3_duplicate'),('split_H','split_H_duplicate')]:
    if per[arm]['sha256']!=per[dup]['sha256']:raise ValueError('noise control mismatch')
   ss[name]=per
  summary[passname]=ss
 groupnames={'real':[x['name'] for x in inputs if x['kind']=='observed_real'],'synthetic':[x['name'] for x in inputs if x['kind']=='synthetic_mechanism']}
 ag={}
 for group,names in groupnames.items():
  ag[group]={}
  for passname,ss in summary.items():
   ag[group][passname]={arm:{'bytes':sum(ss[n][arm]['bytes'] for n in names),**{k:sum(ss[n][arm]['medians'][k] for n in names) for k in ['encode','decode','cost']}} for arm in arms}
 # Compare with the stronger standalone SIZE control at the SAME placement. Not an online selector.
 pairdefs={a:[a.rsplit('_',1)[0]+'_'+s for s in a.rsplit('_',1)[1]] for a in arms if len(a.rsplit('_',1)[-1])==2 and not a.startswith('outer')}
 interactions={}
 for arm,controls in pairdefs.items():
  if not all(c in arms for c in controls):continue
  details={}
  for name in groupnames['real']+groupnames['synthetic']:
   ref=min(controls,key=lambda c:summary['first'][name][c]['bytes']);cand=summary['first'][name][arm]
   details[name]={'standalone_size_reference':ref,'size_change_pct':100*(cand['bytes']/summary['first'][name][ref]['bytes']-1),'wins_both_standalone_size':all(cand['bytes']<summary['first'][name][c]['bytes'] for c in controls),'runs':{p:{k:summary[p][name][arm]['medians'][k]/summary[p][name][ref]['medians'][k] for k in ['encode','decode','cost']} for p in summary}}
  interactions[arm]={'controls':controls,'inputs':details}
 noise={}
 for arm,dup in [('zstd3','zstd3_duplicate'),('split_H','split_H_duplicate')]:
  noise[arm]={k:[min(summary[p][n][dup]['medians'][k]/summary[p][n][arm]['medians'][k] for p in summary for n in groupnames['real']),max(summary[p][n][dup]['medians'][k]/summary[p][n][arm]['medians'][k] for p in summary for n in groupnames['real'])] for k in ['encode','decode','cost']}
 audit={'status':'AUDITED_DEVELOPMENT_NOT_RELEASE','input_count':len(inputs),'arms':len(arms),'configurations_per_pass':len(inputs)*len(arms),'timed_batches':count,'distinct_frames':len(identities),'frame_comparisons_3_build_passes':len(identities)*3,'reported_medians':len(inputs)*len(arms)*2*3,'noise_real':noise,'aggregate':ag,'interactions':interactions,'observed_speed_wins':{arm:[n for n in groupnames['real'] if all(summary[p][n][arm]['vs_zstd3']['smaller'] and summary[p][n][arm]['vs_zstd3']['fits_all_pairs'] for p in summary)] for arm in arms}}
 out=root/'audited';out.mkdir(exist_ok=True);(out/'SUMMARY.json').write_text(json.dumps(summary,indent=2)+'\n');(out/'AUDIT.json').write_text(json.dumps(audit,indent=2)+'\n');print(json.dumps({k:v for k,v in audit.items() if k not in ('aggregate','interactions')},indent=2))
if __name__=='__main__':main()
