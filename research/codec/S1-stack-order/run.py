#!/usr/bin/env python3
"""Frozen S1 matrix runner: all samples, outputs, failures and identities retained."""
from __future__ import annotations
import argparse,hashlib,json,os,pathlib,subprocess,sys

def sha(x):return hashlib.sha256(x).hexdigest()
def validate(rows,frames,arms,rounds,n):
 expected={(a,o,r) for a in arms for o in (0,1) for r in range(rounds)};got=set();batch=max(1,min(8,(2<<20)//max(1,n)))
 if set(frames)!=set(arms):raise ValueError('missing frame')
 for row in rows:
  k=(row['arm'],row['op'],row['round'])
  if k in got or k not in expected:raise ValueError('duplicate/extra sample')
  got.add(k)
  for x in ['op','round','batch','input','bytes','wall_ns','cpu_ns']:
   if type(row[x])!=int or row[x]<0:raise ValueError('noninteger/negative sample')
  if row['input']!=n or row['batch']!=batch or row['bytes']!=frames[row['arm']]['bytes']:raise ValueError('shape mismatch')
  if not row['wall_ns'] or not row['cpu_ns']:raise ValueError('nonpositive time')
 if got!=expected:raise ValueError('missing samples')
 return True

def main():
 p=argparse.ArgumentParser();p.add_argument('--binary',type=pathlib.Path,required=True);p.add_argument('--inputs',type=pathlib.Path,required=True);p.add_argument('--manifest',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--rounds',type=int,required=True);p.add_argument('--seed',type=int,required=True);p.add_argument('--reference',type=pathlib.Path);a=p.parse_args()
 out=a.output.resolve();out.mkdir(exist_ok=False);binary=a.binary.resolve();inputs=json.loads(a.manifest.read_text());arms=[x.split()[0] for x in subprocess.check_output([binary,'arms'],text=True).splitlines()]
 status={'status':'RUNNING','binary_sha256':sha(binary.read_bytes()),'rounds':a.rounds,'seed':a.seed,'arms':arms,'inputs':inputs,'completed':0,'reference':str(a.reference) if a.reference else None};allmeta={};hashes={}
 try:
  for index,inp in enumerate(inputs):
   path=(a.inputs/inp['name']).resolve();data=path.read_bytes()
   if sha(data)!=inp['sha256'] or len(data)!=inp['bytes']:raise ValueError('input mismatch')
   dest=out/inp['name'];dest.mkdir();cmd=[str(binary),str(path),str(dest),str(a.rounds),str(a.seed+index)]
   c=subprocess.run(cmd,capture_output=True,text=True,timeout=240,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1','OMP_NUM_THREADS':'1'})
   (dest/'stdout.jsonl').write_text(c.stdout);(dest/'stderr.log').write_text(c.stderr)
   if c.returncode:raise RuntimeError('process failure '+inp['name']+' '+str(c.returncode))
   frames={};rows=[]
   for line in c.stdout.splitlines():
    row=json.loads(line)
    if row['kind']=='frame':
     if row['arm'] in frames:raise ValueError('duplicate frame record')
     frames[row['arm']]=row
    elif row['kind']=='sample':rows.append(row)
    else:raise ValueError('unrecognized record')
   validate(rows,frames,arms,a.rounds,len(data));hashes[inp['name']]={}
   for arm in arms:
    frame=(dest/(arm+'.frame')).read_bytes()
    if len(frame)!=frames[arm]['bytes']:raise ValueError('frame length mismatch')
    hashes[inp['name']][arm]=sha(frame)
    if a.reference and frame!=(a.reference/inp['name']/(arm+'.frame')).read_bytes():raise ValueError('repeat frame changed')
   for x,y in [('zstd3','zstd3_duplicate'),('split_H','split_H_duplicate')]:
    if hashes[inp['name']][x]!=hashes[inp['name']][y]:raise ValueError('duplicate frame differs')
   allmeta[inp['name']]=frames;status['completed']+=1;print('verified',inp['name'],len(frames),flush=True)
  status['status']='PASS_DEVELOPMENT_EXECUTION_NOT_GENERAL_ACCEPTANCE'
 except BaseException as e:status['status']='FAIL';status['error']=repr(e);raise
 finally:
  (out/'STATUS.json').write_text(json.dumps(status,indent=2)+'\n');(out/'FRAMES.json').write_text(json.dumps(allmeta,indent=2)+'\n');(out/'HASHES.json').write_text(json.dumps(hashes,indent=2)+'\n')
if __name__=='__main__':main()
