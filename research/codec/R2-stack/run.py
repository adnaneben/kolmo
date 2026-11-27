#!/usr/bin/env python3
"""Run frozen R2 factorial on observed archived E03 frames. No fresh holdout."""
from __future__ import annotations
import argparse,hashlib,json,os,platform,random,statistics,subprocess,time,zipfile
from pathlib import Path
from reference import restore
EXPECTED='5d02cee72dbbbc8a396e85e29c2cb3f5bf91f26abd2e808995b9a54205660e82'
def sha(b):return hashlib.sha256(b).hexdigest()
def save(p,x):p.write_text(json.dumps(x,indent=2)+'\n')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--archive',type=Path,required=True);p.add_argument('--probe',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--rounds',type=int,default=7);p.add_argument('--seed',type=int,default=20261112);p.add_argument('--correctness-only',action='store_true');a=p.parse_args()
 if not 1<=a.rounds<=31:p.error('round count')
 if sha(a.archive.read_bytes())!=EXPECTED:raise ValueError('archive mismatch')
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);probe=a.probe.resolve();rows=[];timings=[];commands=[]
 status=dict(status='running',scope='observed development, residual-to-frame encoding; full-input verified decoding',archive_sha256=EXPECTED,rounds=a.rounds,seed=a.seed,correctness_only=a.correctness_only)
 save(out/'status.json',status)
 try:
  with zipfile.ZipFile(a.archive) as z:
   if len(z.namelist())!=len(set(z.namelist())):raise ValueError('duplicate ZIP entries')
   manifest=json.loads(z.read('MANIFEST.json'))
   for name,h in manifest.items():
    if '..' in Path(name).parts or name.startswith('/') or sha(z.read(name))!=h:raise ValueError('manifest '+name)
   status['archive_manifest_verified']=len(manifest)
   records=[json.loads(l) for l in z.read('first/measurements.jsonl').decode().splitlines()];records=[r for r in records if r['policy']=='native_base']
   if len(records)!=58 or len(set(r['id'] for r in records))!=58:raise ValueError('incomplete source matrix')
   random.Random(a.seed).shuffle(records)
   for idx,r in enumerate(records):
    directory=out/r['id'];directory.mkdir();original=z.read('first/'+r['frame_file'])
    if sha(original)!=r['frame_sha256']:raise ValueError('baseline frame hash')
    source=directory/'baseline.frame';source.write_bytes(original)
    cmd=[str(probe),'bench',str(source),str(directory),str(a.rounds),str(a.seed+idx+1),'0' if a.correctness_only else '1']
    t=time.monotonic();cp=subprocess.run(cmd,capture_output=True,text=True,timeout=120,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
    (directory/'command.log').write_text(cp.stdout+cp.stderr);commands.append(dict(argv=cmd,returncode=cp.returncode,seconds=time.monotonic()-t,log_sha256=sha((cp.stdout+cp.stderr).encode())))
    save(out/'commands.json',commands)
    if cp.returncode:raise RuntimeError('case failed '+r['id']+' '+cp.stderr[-1000:])
    obs=[json.loads(l) for l in cp.stdout.splitlines() if l.startswith('{')];sizes=[o for o in obs if o['type']=='size'];ts=[o for o in obs if o['type']=='timing']
    if len(sizes)!=9 or len(ts)!=(0 if a.correctness_only else 27*a.rounds):raise ValueError('incomplete records')
    residual=(directory/'residual.bin').read_bytes();raw=(directory/'original.npy').read_bytes()
    if sha(residual)!=r['capture']['residual'] or sha(raw)!=r['input_sha256']:raise ValueError('capture/input hash mismatch')
    # Independent inverse once per identical (params,rest,residual). Every frame CRC/header
    # is checked separately; the old C inverse ran on every encoded candidate.
    independent=restore(original,residual)
    if independent!=raw:raise ValueError('independent input mismatch')
    for s in sizes:
     policy=s['policy'];f=(directory/f'policy-{policy}.frame').read_bytes()
     if len(f)!=s['bytes'] or f[:6]!=original[:6] or f[8:40]!=original[8:40] or f[48:60]!=original[48:60]:raise ValueError('candidate metadata')
     import struct
     from reference import crc
     pn,rn=struct.unpack_from('<2Q',f,16)
     if f[64:64+pn+rn]!=original[64:64+pn+rn] or len(f)!=64+pn+rn+struct.unpack_from('<Q',f,40)[0] or crc(f[64:],crc(f[:60]))!=int.from_bytes(f[60:64],'little'):raise ValueError('candidate framing')
     if policy==0 and f!=original:raise ValueError('baseline not byte identical')
     row={k:r[k] for k in ('id','source','family','kind','size_class','input_bytes')};row.update(s);row.pop('type');row.update(frame_sha256=sha(f),residual_sha256=sha(residual),input_sha256=sha(raw),baseline_bytes=r['stored_bytes'])
     if not a.correctness_only:
      for regime in ('encode_residual_to_frame','decode_warm_verified_full_input','decode_displaced32MiB_verified_full_input'):
       rr=[t for t in ts if t['policy']==policy and t['regime']==regime]
       if len(rr)!=a.rounds:raise ValueError('timing count')
       row[regime+'_wall_ns']=statistics.median(t['wall_ns']/t['batch'] for t in rr)
       row[regime+'_cpu_ns']=statistics.median(t['cpu_ns']/t['batch'] for t in rr)
     rows.append(row)
    for t in ts:timings.append(dict(id=r['id'],**t))
    print(json.dumps(dict(case=r['id'],rounds=a.rounds,status='passed')),flush=True)
  (out/'measurements.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in rows));(out/'timings.jsonl').write_text(''.join(json.dumps(t)+'\n' for t in timings))
  environment=dict(platform=platform.platform(),python=platform.python_version(),cpu=Path('/proc/cpuinfo').read_text().split('\n\n')[0],probe_sha256=sha(probe.read_bytes()),timing_boundary='residual-to-frame encode; separate CRC-verified full decode; 32MiB displacement NOT guaranteed cold cache')
  save(out/'environment.json',environment)
  status.update(status='passed_development_NOT_RELEASE',cases=len(records),configurations=len(rows),timing_batches=len(timings));save(out/'status.json',status)
 except BaseException as exc:status.update(status='failed_or_interrupted',error=str(exc));save(out/'status.json',status);raise
if __name__=='__main__':main()
