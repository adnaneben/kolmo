#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, math, os, pathlib, random, resource, statistics, subprocess, threading, time
def registry():
 out=[]
 def add(n,f,p,kind,scope='all'):out.append(dict(id=n,family=f,param=p,kind=kind,scope=scope))
 for i,m in enumerate(['rt','fast','eco','hc','max','ultra']):
  add('release_'+m,'release',i,'Released v1.0.1');add('rc_'+m,'rc',i,'Integrated RC1')
 for i,n in enumerate(['native_leaf','typed_huf','csv_archive','ultra_l2','fast_quick','max_l1','ultra_l1'],6):
  scope='csv' if i==8 else 'typed' if i in (6,7) else 'all'
  add('rc_'+n,'rc',i,'RC1 opt-in',scope)
 for l in [1,3,9,19]:add('zstd_'+str(l),'zstd',l,'Competitor')
 add('lz4_fast','lz4',1,'Competitor');add('brotli_8','brotli',8,'Competitor');add('xz_6','xz',6,'Competitor')
 for f,scope in [('ozexact','typed'),('ozwhole','numeric'),('ozserial','all'),('ozcsv','csv')]:add(f+'_9',f,9,'Competitor',scope)
 add('zstd_3_duplicate','zstd',3,'Duplicate control')
 return out
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def spawn_env():
 e=os.environ.copy();e.update(OMP_NUM_THREADS='1',OPENBLAS_NUM_THREADS='1',MKL_NUM_THREADS='1');return e
CPU=min(os.sched_getaffinity(0))
def before():
 os.sched_setaffinity(0,{CPU});resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_CPU,(30,30));resource.setrlimit(resource.RLIMIT_AS,(3*1024**3,3*1024**3))

def execute(cmd,watch=False,timeout=40):
 t0=time.monotonic();samples=[]
 p=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=spawn_env(),preexec_fn=before)
 peak=0
 try:
  if not watch:
   out,err=p.communicate(timeout=timeout)
  else:
   # READY is emitted only after exec and file preparation; no encoding before it.
   line=p.stdout.readline()
   if line.strip()!='READY':
    out=line+p.stdout.read();err=p.stderr.read();p.wait(timeout=timeout)
   else:
    p.stdin.write('\n');p.stdin.flush()
    result={}
    def read_result():result['line']=p.stdout.readline()
    th=threading.Thread(target=read_result,daemon=True);th.start()
    while th.is_alive():
     if time.monotonic()-t0>timeout:raise subprocess.TimeoutExpired(cmd,timeout)
     try:
      txt=pathlib.Path(f'/proc/{p.pid}/smaps_rollup').read_text()
      rss=next(int(l.split()[1]) for l in txt.splitlines() if l.startswith('Rss:'))
      status=pathlib.Path(f'/proc/{p.pid}/status').read_text()
      hwm=next(int(l.split()[1]) for l in status.splitlines() if l.startswith('VmHWM:'))
      peak=max(peak,hwm,rss);samples.append((time.monotonic_ns(),rss))
      if peak>2*1024*1024:raise MemoryError('RSS safety limit 2 GiB exceeded')
     except (FileNotFoundError,ProcessLookupError,StopIteration):pass
     time.sleep(0.001)
    out=result.get('line','');p.stdin.write('\n');p.stdin.flush();p.stdin.close();p.wait(timeout=timeout);err=p.stderr.read()
  if p.returncode: return dict(status='FAILED',returncode=p.returncode,stdout=out[-1000:],stderr=err[-2000:])
  data=json.loads(out.strip().splitlines()[-1]);data['status']='OK'
  if watch:
   begin,end=data['begin_ns'],data['end_ns'];ss=[(max(begin,t),r) for t,r in samples if t<=end]
   # Zero-order-hold integration; first measured RSS extends back to begin.
   if ss:
    ss[0]=(begin,ss[0][1]);weight=0
    for i,(t,r) in enumerate(ss):weight+=max(0,(ss[i+1][0] if i+1<len(ss) else end)-t)*r
    data.update(avg_rss_kib=weight/(end-begin),peak_rss_kib=max(peak,data.get('phase_hwm_kib',0)),rss_samples=len(ss),sampled_peak_kib=max(r for _,r in ss),rss_trace=ss)
   else:data.update(avg_rss_kib=None,peak_rss_kib=None,rss_samples=0)
  return data
 except (subprocess.TimeoutExpired,MemoryError) as e:
  p.kill();out,err=p.communicate();return dict(status='LIMIT',reason=str(e),stderr=err[-1000:])
 finally:
  if p.poll() is None:p.kill();p.wait()


def main():
 ap=argparse.ArgumentParser();ap.add_argument('--binary',type=pathlib.Path,required=True);ap.add_argument('--inputs',type=pathlib.Path,required=True);ap.add_argument('--output',type=pathlib.Path,required=True);ap.add_argument('--rounds',type=int,default=3);args=ap.parse_args()
 out=args.output;out.mkdir(exist_ok=False,parents=True);(out/'frames').mkdir()
 ins=json.loads((args.inputs/'manifest.json').read_text());arms=registry();binary=args.binary.resolve()
 (out/'REGISTRY.json').write_text(json.dumps(arms,indent=2));(out/'INPUTS.json').write_text(json.dumps(ins,indent=2))
 env=dict(cpu=CPU,uname=list(os.uname()),binary_sha256=digest(binary),sampling_ms=1,performance_repeats=args.rounds,target_operation_seconds=0.025,memory_target_seconds=0.05,ultra_input_cap=512*1024,scope='observed-development',cpuinfo=pathlib.Path('/proc/cpuinfo').read_text().split('\n\n')[0])
 (out/'ENVIRONMENT.json').write_text(json.dumps(env,indent=2))
 log=(out/'EVENTS.jsonl').open('a',buffering=1);ready=[]
 for x in ins:
  p=args.inputs/x['file'];assert digest(p)==x['input_sha256'];suffix=p.suffix
  for a in arms:
   if a['scope']=='csv' and suffix!='.csv':continue
   if a['scope']=='numeric' and suffix!='.npy':continue
   if a['scope']=='typed' and suffix not in ('.npy','.wav'):continue
   base=dict(input_id=x['id'],arm=a['id'],input_bytes=x['input_bytes'])
   if 'ultra' in a['id'] and x['input_bytes']>=512*1024:
    log.write(json.dumps({**base,'phase':'generation','status':'NOT_RUN_LIMIT','reason':'registered >=512 KiB ultra safety cap'})+'\n');continue
   frame=out/'frames'/f"{x['id']}--{a['id']}.bin"
   res=execute([str(binary),'make',a['family'],str(a['param']),'encode',str(p),str(frame),'0','0'],timeout=45)
   log.write(json.dumps({**base,'phase':'generation',**res})+'\n')
   if res['status']=='OK':ready.append((x,a,p,frame))
  print('Generated',x['id'],len(ready),flush=True)
 for rnd in range(args.rounds):
  random.Random(20261113+rnd).shuffle(ready)
  for op in ['encode','decode']:
   for x,a,p,frame in ready:
    res=execute([str(binary),'run',a['family'],str(a['param']),op,str(p),str(frame),'0.025','0'],timeout=45)
    log.write(json.dumps(dict(input_id=x['id'],arm=a['id'],phase='timing',op=op,round=rnd,**res))+'\n')
   print('Timing',rnd,op,'done',flush=True)
 for op in ['encode','decode']:
  for idx,(x,a,p,frame) in enumerate(ready):
   res=execute([str(binary),'run',a['family'],str(a['param']),op,str(p),str(frame),'0.05','1'],watch=True,timeout=45)
   trace=res.pop('rss_trace',[]);tracep=out/f"rss-{x['id']}--{a['id']}--{op}.json";tracep.write_text(json.dumps(trace))
   log.write(json.dumps(dict(input_id=x['id'],arm=a['id'],phase='memory',op=op,trace_sha256=digest(tracep),**res))+'\n')
   if idx%100==0:print('Memory',op,idx,'/',len(ready),flush=True)
  print('Memory',op,'done',flush=True)
 log.close();(out/'COMPLETE.json').write_text(json.dumps(dict(status='executed',successful_generation=len(ready),registered_arms=len(arms),inputs=len(ins),rounds=args.rounds)))
if __name__=='__main__':main()
