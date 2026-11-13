#!/usr/bin/env python3
"""Replay archived R3 candidates through real CZN1 frames and unchanged CLI.
Development-only decoder ablation: encoding and selected bitstreams are frozen.
"""
from __future__ import annotations
import argparse, hashlib, json, os, random, statistics, struct, subprocess, time, zipfile
from pathlib import Path
EXPECTED='00ae71bdc5e4f3612d017dfeabb0f5154742feda67bde2be92f1bfeefb50ebe1'
def digest(data: bytes) -> str: return hashlib.sha256(data).hexdigest()
def read_uv(data: bytes, pos: int) -> tuple[int,int]:
    value=0
    for shift in range(0,64,7):
        if pos>=len(data): raise ValueError('truncated integer')
        byte=data[pos];pos+=1
        if shift==63 and byte>1: raise ValueError('overflow')
        value|=(byte&127)<<shift
        if byte<128: return value,pos
    raise ValueError('overlong integer')
def chunks(data: bytes) -> list[dict]:
    pos=0; result=[]
    while pos<len(data):
        n,pos=read_uv(data,pos)
        if not 0<n<=1048576: raise ValueError('chunk bound')
        counts=[]
        while len(counts)<256:
            f,pos=read_uv(data,pos)
            if f:
                if f>65536: raise ValueError('frequency')
                counts.append(f)
            else:
                if pos>=len(data): raise ValueError('truncated zero run')
                count=1+data[pos];pos+=1
                if len(counts)+count>256: raise ValueError('zero run')
                counts.extend([0]*count)
        if sum(counts)!=65536: raise ValueError('table sum')
        size,pos=read_uv(data,pos)
        if size<32 or size%4 or size>len(data)-pos: raise ValueError('coded bounds')
        states=struct.unpack_from('<4Q',data,pos)
        one=[i for i,f in enumerate(counts) if f]
        if len(one)==1 and (size!=32 or states!=(1<<31,)*4): raise ValueError('singleton terminal state')
        result.append(dict(raw_bytes=n,coded_bytes=size,symbols=len(one),singleton_symbol=one[0] if len(one)==1 else None))
        pos+=size
    return result
def median(records: list[dict],case: str,arm: str,regime: int,metric: str) -> float:
    return statistics.median(r[metric]/r['batch'] for r in records if r['case']==case and r['arm']==arm and r['displaced']==regime)
def main() -> None:
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ('source','build','artifact','output'): parser.add_argument('--'+key,type=Path,required=True)
    a=parser.parse_args();root=a.source.resolve();build=a.build.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    archive=a.artifact.read_bytes()
    if digest(archive)!=EXPECTED: raise ValueError('R3 archive digest mismatch')
    commands=[]; receipts=[]
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    def command(argv: list,log: Path|None=None) -> str:
        start=time.monotonic();p=subprocess.run(list(map(str,argv)),capture_output=True,timeout=45,env=env)
        text=p.stdout.decode();err=p.stderr
        commands.append(dict(argv=list(map(str,argv)),returncode=p.returncode,elapsed_seconds=time.monotonic()-start,stdout_sha256=digest(p.stdout),stderr_sha256=digest(err)))
        if log: log.write_bytes(p.stdout+err)
        if p.returncode: raise RuntimeError(f'command failed {argv}: {err.decode(errors="replace")}')
        return text
    with zipfile.ZipFile(a.artifact) as z:
        manifest=json.loads(z.read('MANIFEST.json'))
        for name,expected in manifest.items():
            if digest(z.read(name))!=expected: raise ValueError('bad manifest entry '+name)
        verified=len(manifest)
        entries=sorted(s for s in z.namelist() if s.startswith('first/') and s.endswith('/policy-9.frame'))
        if len(entries)!=8: raise ValueError('unexpected input set')
        for name in entries:
            case=name.split('/')[1];case_dir=out/'cases'/case;case_dir.mkdir(parents=True)
            data=z.read(name);src=case_dir/'source.frame';src.write_bytes(data)
            outputs=[]
            for variant in ('base','fast','asan'):
                dest=case_dir/variant
                result=json.loads(command([build/f'probe-{variant}','wrap',src,dest],case_dir/f'{variant}-wrap.json'))
                outputs.append({p.name:digest(p.read_bytes()) for p in dest.iterdir()})
            if outputs[0]!=outputs[1] or outputs[0]!=outputs[2]: raise ValueError('cross-build frame identity')
            raw=z.read(f'first/{case}/original.npy')
            if (case_dir/'base/original.bin').read_bytes()!=raw: raise ValueError('original identity')
            frame=case_dir/'base/frame.czn';decoded=case_dir/'production-cli.raw'
            command([root/'build/kolmo','-d','-T','1',frame,decoded],case_dir/'cli.log')
            if decoded.read_bytes()!=raw: raise ValueError('production CLI mismatch')
            payload=(case_dir/'base/payload.bin').read_bytes()
            rows=chunks(payload) if result['codec']==14 else []
            receipts.append(dict(case=case,source_frame_sha256=digest(data),frame_sha256=digest(frame.read_bytes()),original_sha256=digest(raw),**result,chunks=rows,
                                 singleton_chunks=sum(x['symbols']==1 for x in rows),singleton_raw_bytes=sum(x['raw_bytes'] for x in rows if x['symbols']==1)))
    (out/'inputs.json').write_text(json.dumps(receipts,indent=2)+'\n')
    for phase,rounds,seed in [('first',7,20261113),('confirmation',5,20261114)]:
        records=[];rand=random.Random(seed)
        # One process per timed batch; process startup and input loading excluded
        # by the C timer. No encoder immediately precedes any timed decode.
        for round_id in range(rounds):
            jobs=[(r,regime,arm) for r in receipts if r['codec']==14 for regime in (0,1) for arm in ('base','fast','base_duplicate')]
            rand.shuffle(jobs)
            for r,regime,arm in jobs:
                batch=min(256,max(1,(4*1048576+r['raw_bytes']-1)//r['raw_bytes']))
                variant='base' if arm=='base_duplicate' else arm
                row=json.loads(command([build/f'probe-{variant}','bench',out/'cases'/r['case']/'base',batch,regime]))
                records.append(dict(phase=phase,round=round_id,case=r['case'],arm=arm,seed=seed,**row))
        (out/(phase+'.jsonl')).write_text(''.join(json.dumps(r,sort_keys=True)+'\n' for r in records))
        summary=[]
        for r in receipts:
            if r['codec']!=14:continue
            item={'case':r['case'],'singleton_chunks':r['singleton_chunks'],'singleton_raw_bytes':r['singleton_raw_bytes']}
            for regime,label in ((0,'warm'),(1,'displaced_batch')):
                for metric in ('kernel_ns','full_ns'):
                    base=median(records,r['case'],'base',regime,metric)
                    item[label+'_'+metric]=dict(baseline=base,candidate=median(records,r['case'],'fast',regime,metric),
                       ratio=median(records,r['case'],'fast',regime,metric)/base,
                       identical_baseline_ratio=median(records,r['case'],'base_duplicate',regime,metric)/base)
            summary.append(item)
        (out/(phase+'-summary.json')).write_text(json.dumps(summary,indent=2)+'\n')
    (out/'commands.json').write_text(json.dumps(commands,indent=2)+'\n')
    result={'status':'passed_development_replay','archive_sha256':EXPECTED,'verified_manifest_entries':verified,'complete_inputs':len(receipts),'cli_roundtrips':len(receipts),
            'timed_rans_inputs':sum(r['codec']==14 for r in receipts),'rounds':[7,5],'sample_batches':sum(r['codec']==14 for r in receipts)*2*3*12,
            'new_source_data':False,'production_changed':False,'fresh_validation':False,'P2_competitor_acceptance':False,
            'binary_sha256':{v:digest((build/f'probe-{v}').read_bytes()) for v in ('base','fast','asan')},
            'cost_boundary':'decode only; leaf kernel or full CZN1 verified decoding. No encode/pipeline competitor timing. 32 MiB displacement before batch, not before every decode.'}
    (out/'receipt.json').write_text(json.dumps(result,indent=2)+'\n')
    (out/'MANIFEST.json').write_text(json.dumps({str(p.relative_to(out)):digest(p.read_bytes()) for p in sorted(out.rglob('*')) if p.is_file() and p.name!='MANIFEST.json'},indent=2)+'\n')
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
