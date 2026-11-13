#!/usr/bin/env python3
"""Matched native OpenZL development controls; no Kolmo/P2 acceptance claim."""
import argparse, hashlib, json, pathlib, random, resource, statistics, subprocess, time

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def execute(cmd):
    before=resource.getrusage(resource.RUSAGE_CHILDREN);start=time.perf_counter_ns()
    r=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=30)
    elapsed=time.perf_counter_ns()-start;after=resource.getrusage(resource.RUSAGE_CHILDREN)
    if r.returncode:raise RuntimeError(f'{cmd}: {r.returncode}: {r.stderr.decode(errors="replace")}')
    return {'wall_ns':elapsed,'cpu_ns':round((after.ru_utime+after.ru_stime-before.ru_utime-before.ru_stime)*1e9)}
def main():
    p=argparse.ArgumentParser();p.add_argument('--adapter',required=True,type=pathlib.Path);p.add_argument('--reader',required=True,type=pathlib.Path);p.add_argument('--inputs',required=True,type=pathlib.Path);p.add_argument('--output',required=True,type=pathlib.Path);a=p.parse_args()
    a.output.mkdir(parents=True,exist_ok=False)
    cases=sorted(a.inputs.glob('*.npy'));assert len(cases)==8,'frozen development cohort is eight R3 NPY files'
    policies=[(m+str(l),m,str(l)) for m in ('exact','whole','serial') for l in (6,9)]+[('exact9_duplicate','exact','9')]
    inputs={x.stem:{'bytes':x.stat().st_size,'sha256':sha(x)} for x in cases}
    records=[];frame_info={}
    for inp in cases:
        for name,mode,lev in policies:
            out=a.output/(inp.stem+'--'+name+'.zl');execute([str(a.adapter),'c',lev,mode,str(inp),str(out)])
            decoded=a.output/'verify.out';execute([str(a.reader),str(out),str(decoded)]);assert sha(decoded)==sha(inp);decoded.unlink()
            frame_info[(inp.stem,name)]={'path':out.name,'bytes':out.stat().st_size,'sha256':sha(out)}
        assert frame_info[(inp.stem,'exact9')]['sha256']==frame_info[(inp.stem,'exact9_duplicate')]['sha256']
    for pass_id,seed in enumerate((20261113,20261114)):
      rng=random.Random(seed)
      for op in ('encode','decode'):
       for rep in range(3):
        tasks=[(i,*q) for i in cases for q in policies];rng.shuffle(tasks)
        for inp,name,mode,lev in tasks:
            frame=a.output/frame_info[(inp.stem,name)]['path'];out=a.output/'work.tmp';out.unlink(missing_ok=True)
            cmd=[str(a.adapter),'c',lev,mode,str(inp),str(out)] if op=='encode' else [str(a.reader),str(frame),str(out)]
            t=execute(cmd);expected=frame_info[(inp.stem,name)]['sha256'] if op=='encode' else inputs[inp.stem]['sha256'];assert sha(out)==expected
            out.unlink();records.append({'pass':pass_id,'round':rep,'op':op,'input':inp.stem,'policy':name,**t})
    (a.output/'timings.json').write_text(json.dumps(records,indent=1)+'\n')
    summary=[]
    for inp in cases:
     for name,mode,lev in policies:
      row={'input':inp.stem,'policy':name,**frame_info[(inp.stem,name)]}
      for pid in (0,1):
       row['pass'+str(pid)]={op:{metric:statistics.median(x[metric] for x in records if x['pass']==pid and x['op']==op and x['input']==inp.stem and x['policy']==name) for metric in ('wall_ns','cpu_ns')} for op in ('encode','decode')}
      summary.append(row)
    (a.output/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    receipt={'status':'matched_native_comparator_development_only','inputs':inputs,'frames':len(frame_info),'samples':len(records),'rounds':3,'passes':2,'adapter_sha256':sha(a.adapter),'reader_sha256':sha(a.reader),'policies':policies,'boundary':'complete CLI; independent encode/decode, process startup and I/O included, numeric payload excludes header only in exact arm; no candidate Kolmo timing or fresh holdout'}
    (a.output/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    (a.output/'MANIFEST.json').write_text(json.dumps({x.name:sha(x) for x in sorted(a.output.iterdir()) if x.is_file()},indent=2)+'\n')
if __name__=='__main__': main()
