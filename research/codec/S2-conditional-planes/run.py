#!/usr/bin/env python3
"""Execute frozen S2 arms and fail closed on incomplete or changed evidence."""
import argparse,hashlib,json,os,pathlib,subprocess

def sha(data):return hashlib.sha256(data).hexdigest()
CONTROLS={'zstd3':'zstd3','zstd3_duplicate':'zstd3_duplicate','s1_H':'split_H','opt_H':'split_H','s1_Z':'split_Z','opt_Z':'split_Z','s1_HZ':'split_HZ','opt_HZ':'split_HZ','s1_HL':'split_HL','opt_HL':'split_HL'}
PAIRS=[('zstd3','zstd3_duplicate'),('s1_H','opt_H'),('s1_Z','opt_Z'),('s1_HZ','opt_HZ'),('s1_HL','opt_HL')]
ARMS=['zstd3','zstd3_duplicate','s1_H','opt_H','s1_Z','opt_Z','s1_HZ','opt_HZ','s1_HL','opt_HL','low_HZ','high_HZ','low_HL','high_HL','gate_HZ','gate_menu']

def validate(rows,meta,rounds,n):
    if set(meta)!=set(ARMS):raise ValueError('missing/extra frames')
    batch=max(1,min(8,(2<<20)//max(1,n)))
    required={(a,o,r) for a in ARMS for o in (0,1) for r in range(rounds)};got=set()
    for v in rows:
        for k in ['op','round','batch','input','bytes','wall_ns','cpu_ns']:
            if type(v[k])!=int or v[k]<0:raise ValueError('invalid numeric record')
        key=(v['arm'],v['op'],v['round'])
        if key not in required or key in got:raise ValueError('duplicate/extra sample')
        got.add(key)
        if v['input']!=n or v['batch']!=batch or v['bytes']!=meta[v['arm']]['bytes']:raise ValueError('sample shape')
        if not v['wall_ns'] or not v['cpu_ns']:raise ValueError('nonpositive time')
    if got!=required:raise ValueError('incomplete samples')
    for arm in ['gate_HZ','gate_menu']:
        if meta[arm]['bytes']>meta['opt_H']['bytes']:raise ValueError('local H nonexpansion')
        for d in meta[arm]['decisions']:
            if d['sampled']>4096 or d['selected'] not in ['H','Z','HZ','HL']:raise ValueError('invalid gate decision')
            if d['rejected'] and (not d['tried'] or d['selected']!='H'):raise ValueError('invalid rejected trial')
    return True

def main():
    p=argparse.ArgumentParser();p.add_argument('--binary',type=pathlib.Path,required=True);p.add_argument('--s1',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--rounds',type=int,required=True);p.add_argument('--seed',type=int,required=True);p.add_argument('--reference',type=pathlib.Path);a=p.parse_args()
    out=a.output.resolve();out.mkdir(exist_ok=False);binary=a.binary.resolve();s1=a.s1.resolve()
    inputs=json.loads((s1/'evidence/INPUTS.json').read_text());arms=subprocess.check_output([binary,'arms'],text=True).splitlines()
    if arms!=ARMS:raise ValueError('fixed arms changed')
    stat={'status':'RUNNING','binary_sha256':sha(binary.read_bytes()),'rounds':a.rounds,'seed':a.seed,'inputs':inputs,'arms':arms,'completed':0};metas={};hashes={}
    try:
        for i,inp in enumerate(inputs):
            path=s1/'inputs'/inp['name'];data=path.read_bytes()
            if len(data)!=inp['bytes'] or sha(data)!=inp['sha256']:raise ValueError('input identity')
            dest=out/inp['name'];dest.mkdir()
            cmd=[str(binary),str(path),str(dest),str(a.rounds),str(a.seed+i)]
            proc=subprocess.run(cmd,capture_output=True,text=True,timeout=240,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=1:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1','OMP_NUM_THREADS':'1'})
            (dest/'stdout.jsonl').write_text(proc.stdout);(dest/'stderr.log').write_text(proc.stderr)
            if proc.returncode:raise RuntimeError('codec failure '+inp['name']+' '+str(proc.returncode))
            rows=[];meta={}
            for line in proc.stdout.splitlines():
                v=json.loads(line)
                if v['kind']=='frame':
                    if v['arm'] in meta:raise ValueError('duplicate frame')
                    meta[v['arm']]=v
                elif v['kind']=='sample':rows.append(v)
                else:raise ValueError('unknown kind')
            validate(rows,meta,a.rounds,len(data));hs={}
            for arm in arms:
                frame=(dest/(arm+'.frame')).read_bytes()
                if len(frame)!=meta[arm]['bytes']:raise ValueError('wrong frame length')
                hs[arm]=sha(frame)
                if arm in CONTROLS and frame!=(s1/'first'/inp['name']/(CONTROLS[arm]+'.frame')).read_bytes():raise ValueError('S1 reference changed '+arm)
                if a.reference and frame!=(a.reference/inp['name']/(arm+'.frame')).read_bytes():raise ValueError('repeat changed '+arm)
            for x,y in PAIRS:
                if hs[x]!=hs[y]:raise ValueError('output-identical ablation differs')
            metas[inp['name']]=meta;hashes[inp['name']]=hs;stat['completed']+=1
            print('verified',inp['name'],len(meta),flush=True)
        stat['status']='PASS_DEVELOPMENT_NOT_RELEASE'
    except BaseException as e:stat['status']='FAIL';stat['error']=repr(e);raise
    finally:
        for name,obj in [('STATUS',stat),('FRAMES',metas),('HASHES',hashes)]:
            (out/(name+'.json')).write_text(json.dumps(obj,indent=2)+'\n')
if __name__=='__main__':main()
