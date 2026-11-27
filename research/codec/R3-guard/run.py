#!/usr/bin/env python3
"""R3 immutable measurements, independent reconstruction, complete-frame checks."""
from __future__ import annotations
import argparse,hashlib,importlib.util,json,os,platform,random,statistics,subprocess,struct
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('r2reference',HERE.parent/'R2-stack/reference.py');reference=importlib.util.module_from_spec(spec);spec.loader.exec_module(reference)
POLICIES=(0,7,8,9,10)
REGIMES=('encode','decode_verified_warm','decode_verified_displaced32MiB','decode_kernel_warm','decode_kernel_displaced32MiB')
def sha(b):return hashlib.sha256(b).hexdigest()
def save(p,x):p.write_text(json.dumps(x,indent=2)+'\n')
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--manifest',type=Path,required=True);p.add_argument('--probe',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--rounds',type=int,default=7);p.add_argument('--seed',type=int,default=2026111203);p.add_argument('--correctness-only',action='store_true');a=p.parse_args()
 if not 1<=a.rounds<=31:p.error('rounds')
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);manifest=json.loads(a.manifest.read_text());cases=list(manifest['inputs']);random.Random(a.seed).shuffle(cases)
 rows=[];timings=[];commands=[];status={'status':'running','partition':manifest['partition'],'manifest_sha256':sha(a.manifest.read_bytes()),'rounds':a.rounds,'seed':a.seed,'correctness_only':a.correctness_only};save(out/'status.json',status)
 try:
  for i,c in enumerate(cases):
   directory=out/c['id'];directory.mkdir();source=Path(c['frame']).read_bytes()
   if sha(source)!=c['source_frame_sha256']:raise ValueError('source frame hash')
   cmd=[str(a.probe.resolve()),'bench',c['frame'],str(directory),str(a.rounds),str(a.seed+i+1),'0' if a.correctness_only else '1']
   cp=subprocess.run(cmd,capture_output=True,text=True,timeout=180,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
   data=(cp.stdout+cp.stderr).encode();(directory/'command.log').write_bytes(data);commands.append({'argv':cmd,'returncode':cp.returncode,'log_sha256':sha(data)});save(out/'commands.json',commands)
   if cp.returncode:raise RuntimeError('case failed '+c['id']+' '+cp.stderr[-1000:])
   obs=[json.loads(s) for s in cp.stdout.splitlines() if s.startswith('{')];ss=[r for r in obs if r['type']=='size'];tt=[r for r in obs if r['type']=='timing'];vv=[r for r in obs if r['type']=='verification']
   if {r['policy'] for r in ss}!=set(POLICIES) or len(ss)!=5 or len(vv)!=1 or len(tt)!=(0 if a.correctness_only else 25*a.rounds):raise ValueError('incomplete measurements')
   raw=(directory/'original.npy').read_bytes();res=(directory/'residual.bin').read_bytes()
   if sha(raw)!=c['input_sha256'] or reference.restore(source,res)!=raw:raise ValueError('independent numeric inverse')
   base=(directory/'policy-0.frame').read_bytes()
   if 'baseline_frame_sha256' in c and sha(base)!=c['baseline_frame_sha256']:raise ValueError('development baseline changed')
   for s in ss:
    f=(directory/f"policy-{s['policy']}.frame").read_bytes();pn,rn=struct.unpack_from('<2Q',f,16)
    if len(f)!=s['bytes'] or f[:6]!=source[:6] or f[8:40]!=source[8:40] or f[48:60]!=source[48:60] or f[64:64+pn+rn]!=source[64:64+pn+rn] or 64+pn+rn+int.from_bytes(f[40:48],'little')!=len(f) or reference.crc(f[64:],reference.crc(f[:60]))!=int.from_bytes(f[60:64],'little'):raise ValueError('complete framing mismatch')
    row={k:c[k] for k in ('id','family','group','partition','input_bytes')};row.update(s);row.pop('type');row.update(frame_sha256=sha(f),input_sha256=sha(raw),residual_sha256=sha(res),verification=vv[0],baseline_bytes=len(base))
    if not a.correctness_only:
     for regime in REGIMES:
      rr=[r for r in tt if r['policy']==s['policy'] and r['regime']==regime]
      if len(rr)!=a.rounds:raise ValueError('timing regime')
      for field in ('wall_ns','cpu_ns'):row[regime+'_'+field]=statistics.median(r[field]/r['batch'] for r in rr)
    rows.append(row)
   for r in tt:timings.append({'id':c['id'],**r})
   print(json.dumps({'case':c['id'],'status':'passed'}),flush=True)
  (out/'measurements.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in rows));(out/'timings.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in timings))
  save(out/'environment.json',{'platform':platform.platform(),'python':platform.python_version(),'cpu':Path('/proc/cpuinfo').read_text().split('\n\n')[0],'probe_sha256':sha(a.probe.read_bytes()),'scope':'residual-to-frame encode; separate entropy kernel/verified KG_INT decode; component-only tracked allocations'})
  status.update(status='passed_execution_NOT_RELEASE',cases=len(cases),configurations=len(rows),timing_batches=len(timings));save(out/'status.json',status)
 except BaseException as e:status.update(status='failed_or_interrupted',error=str(e));save(out/'status.json',status);raise
if __name__=='__main__':main()
