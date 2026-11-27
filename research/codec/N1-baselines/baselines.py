#!/usr/bin/env python3
"""Pinned complete-CLI rival controls. Observed development, never P2 acceptance."""
from __future__ import annotations
import argparse, ast, hashlib, json, os, random, resource, signal, statistics
import struct, subprocess, sys, time, zipfile
from pathlib import Path

ARCHIVE_SHA = '00ae71bdc5e4f3612d017dfeabb0f5154742feda67bde2be92f1bfeefb50ebe1'
MAX_INPUT = 32 << 20
COLUMNS = {
 'covertype': ['elevation','road_distance','fire_distance','hillshade_control','cover_control'],
 'collegemsg': ['source_id','target_id','unix_timestamp'],
}

def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def dump(path: Path, value) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, allow_nan=False) + '\n')

def npy_u32(data: bytes) -> list[int]:
    if data[:8] != b'\x93NUMPY\x01\x00' or len(data) > MAX_INPUT:
        raise ValueError('expected bounded NPY v1 uint32')
    h = int.from_bytes(data[8:10], 'little'); end = 10+h
    if h > 65535 or end > len(data): raise ValueError('bad NPY header')
    meta = ast.literal_eval(data[10:end].decode('ascii').strip())
    shape = meta.get('shape')
    if meta.get('descr') != '<u4' or meta.get('fortran_order') is not False:
        raise ValueError('unexpected NPY type/order')
    if not isinstance(shape, tuple) or len(shape) != 1 or type(shape[0]) is not int or shape[0] < 0:
        raise ValueError('unexpected NPY shape')
    if len(data)-end != 4*shape[0]: raise ValueError('length mismatch')
    return [x[0] for x in struct.iter_unpack('<I', data[end:])]

def csv_bytes(headers: list[str], columns: list[list[int]]) -> bytes:
    if not columns or len(headers) != len(columns) or len({len(c) for c in columns}) != 1:
        raise ValueError('unequal columns')
    if any(not h or any(c in h for c in ',\r\n') for h in headers): raise ValueError('bad header')
    return (','.join(headers)+'\n'+''.join(','.join(map(str,row))+'\n' for row in zip(*columns))).encode('ascii')

def prepare(archive: Path, output: Path, expected_sha: str = ARCHIVE_SHA) -> dict:
    if sha(archive.read_bytes()) != expected_sha: raise ValueError('archive digest mismatch')
    output.mkdir(parents=True, exist_ok=False)
    inp = output/'inputs'; inp.mkdir()
    rows = []
    with zipfile.ZipFile(archive) as z:
        names = z.namelist()
        if len(names) != len(set(names)): raise ValueError('duplicate archive path')
        for name in names:
            p = Path(name)
            if p.is_absolute() or '..' in p.parts: raise ValueError('unsafe archive path')
        manifests = json.loads(z.read('MANIFEST.json'))
        for name, expected in manifests.items():
            if sha(z.read(name)) != expected: raise ValueError('artifact manifest mismatch: '+name)
        old = json.loads(z.read('prepared/manifest.json'))
        expected_ids = {fam+'__'+c for fam,cs in COLUMNS.items() for c in cs}
        records = {r['id']: r for r in old['inputs']}
        if set(records) != expected_ids: raise ValueError('source panel changed')
        columns = {}
        for fam, cs in COLUMNS.items():
            columns[fam] = []
            for col in cs:
                ident = fam+'__'+col; name=ident+'.npy'; member='prepared/inputs/'+name
                data=z.read(member)
                if sha(data)!=records[ident]['input_sha256'] or len(data)!=records[ident]['input_bytes']:
                    raise ValueError('input mismatch '+name)
                vals=npy_u32(data); columns[fam].append(vals)
                (inp/name).write_bytes(data)
                rows.append(dict(id=ident, family=fam, view='npy', profile='numpy', file='inputs/'+name,
                                 input_bytes=len(data),input_sha256=sha(data),rows=len(vals),partition='observed_development'))
            data=csv_bytes(cs,columns[fam]); name=fam+'__table.csv'
            if len(data)>MAX_INPUT: raise ValueError('CSV limit')
            (inp/name).write_bytes(data)
            rows.append(dict(id=fam+'__table',family=fam,view='csv',profile='csv',file='inputs/'+name,
                             input_bytes=len(data),input_sha256=sha(data),rows=len(columns[fam][0]),
                             columns=cs,partition='observed_development'))
        result=dict(partition='observed_development', source_archive_sha256=expected_sha,
                    verified_manifest_entries=len(manifests),sources=old['sources'],inputs=rows,
                    scope='Complete derived NPY and canonical CSV bytes, not original publisher text; views correlated.')
    dump(output/'manifest.json',result)
    return result

def policies(profile: str, tools: dict[str,str]) -> list[dict]:
    z,k,o = tools['zstd'],tools['kolmo'],tools['openzl']
    res=[dict(id='zstd_3',tool='zstd',level=3,enc=[z,'--single-thread','-3','-q','-f'],dec=[z,'-d','-q','-f'])]
    for mode, flags in [('fast_quick',['-m','fast','-q']),('fast',['-m','fast']),('hc',['-m','hc'])]:
        res.append(dict(id='kolmo_'+mode,tool='kolmo',enc=[k,*flags,'-T1'],dec=[k,'-d','-T1']))
    for prof in [profile,'serial']:
        for level in [6,9]:
            res.append(dict(id=f'openzl_{prof}_{level}',tool='openzl',profile=prof,level=level,
                            training_regime='untrained_per_source_builtin_models_allowed',
                            enc=[o,'compress','-p',prof,'-l',str(level),'-f'],dec=[o,'decompress','-f']))
    return res

def command(policy: dict, operation: str, src: Path, dst: Path) -> list[str]:
    cmd=list(policy['enc' if operation=='encode' else 'dec'])
    return cmd+[str(src),str(dst)] if policy['tool']=='kolmo' else cmd+[str(src),'-o',str(dst)]

def timed(command: list[str], logfile: Path, timeout: float = 90, memory_limit: int = 6 << 30) -> dict:
    """Linux child-specific wall/CPU/maxRSS. No cumulative CHILDREN-RSS substitution."""
    if timeout<=0: raise ValueError('timeout')
    env={**os.environ, 'OMP_NUM_THREADS':'1','OPENBLAS_NUM_THREADS':'1','MKL_NUM_THREADS':'1'}
    cpu=min(os.sched_getaffinity(0))
    def setup():
        os.sched_setaffinity(0,{cpu})
        resource.setrlimit(resource.RLIMIT_AS,(memory_limit,memory_limit))
        resource.setrlimit(resource.RLIMIT_CPU,(int(timeout)+1,int(timeout)+2))
    def alarm(_signum,_frame): raise TimeoutError('child wall-clock limit')
    logfile.parent.mkdir(parents=True,exist_ok=True)
    old=signal.signal(signal.SIGALRM,alarm)
    try:
        with logfile.open('wb') as log:
            start=time.perf_counter_ns()
            p=subprocess.Popen(command,stdout=log,stderr=subprocess.STDOUT,env=env,preexec_fn=setup,start_new_session=True)
            expired=False
            try:
                signal.setitimer(signal.ITIMER_REAL,timeout)
                _, status, usage=os.wait4(p.pid,0)
            except TimeoutError:
                expired=True
                os.killpg(p.pid,signal.SIGKILL)
                _, status, usage=os.wait4(p.pid,0)
            finally:
                signal.setitimer(signal.ITIMER_REAL,0)
            wall=(time.perf_counter_ns()-start)/1e9
            p.returncode=os.waitstatus_to_exitcode(status)
            return dict(command=command,returncode=p.returncode,timed_out=expired,wall_s=wall,
                        cpu_s=usage.ru_utime+usage.ru_stime,peak_rss_kib=usage.ru_maxrss,
                        assigned_cpu=cpu,thread_constraint='single_CPU_affinity_and_thread_environment',
                        rss_scope='individual_exec_child_process_not_codec_scratch')
    finally:
        signal.signal(signal.SIGALRM,old)

def cost(encode: dict, decode: dict, size: int) -> dict:
    # Exact P2 default constants; historical model values, not current price quotes.
    def charge(x): return x['cpu_s'] if x['cpu_s']>1.03*x['wall_s'] else x['wall_s']
    e,d=charge(encode),charge(decode)
    return dict(speed_cost_s=e+d+size/125_000_000,
                archive_cost_usd=(e+3.3*d)*(3.89/72)/3600+size/1e9*0.023)

def run(prepared: Path, output: Path, tools: dict[str,str], rounds: int=3, seed: int=20261112) -> dict:
    output.mkdir(parents=True,exist_ok=False)
    manifest=json.loads((prepared/'manifest.json').read_text())
    tool_receipt={k:dict(path=v,sha256=sha(Path(v).read_bytes())) for k,v in tools.items()}
    dump(output/'environment.json',dict(tools=tool_receipt,python=sys.version,platform=os.uname(),seed=seed,
                                       timing='complete_CLI_wall_and_child_CPU_includes_startup_IO_no_postverification',
                                       candidate_acceptance='NOT_EVALUATED_OBSERVED_BASELINE_CONTROLS'))
    cases={}; rows=[]
    for row in manifest['inputs']:
        src=(prepared/row['file']).resolve()
        if sha(src.read_bytes())!=row['input_sha256']: raise ValueError('changed prepared input')
        for pol in policies(row['profile'],tools):
            key=(row['id'],pol['id']); d=output/row['id']/pol['id']; d.mkdir(parents=True)
            frame=d/'retained.frame';back=d/'warmup.decoded'
            e=timed(command(pol,'encode',src,frame),d/'warmup.encode.log')
            rec=dict(input_id=row['id'],family=row['family'],view=row['view'],policy=pol,
                     input_bytes=row['input_bytes'],input_sha256=row['input_sha256'],warmup_encode=e,
                     status='ERROR_NOT_A_MEASUREMENT')
            if not e['returncode']:
                dec=timed(command(pol,'decode',frame,back),d/'warmup.decode.log');rec['warmup_decode']=dec
                if not dec['returncode'] and back.exists() and sha(back.read_bytes())==row['input_sha256']:
                    rec.update(status='READY',stored_bytes=frame.stat().st_size,frame_sha256=sha(frame.read_bytes()))
                    cases[key]=(row,pol,d,src,frame,rec)
                elif not dec['returncode']:
                    raise ValueError('CORRUPTION: warmup roundtrip '+str(key))
            if back.exists(): back.unlink()
            rows.append(rec)
    dump(output/'warmups.json',rows)
    rnd=random.Random(seed); samples={}; raw=output/'timings.jsonl'
    for op in ['encode','decode']:
        # All encode rounds finish before decode rounds, not an immediate encode/decode pair.
        for r in range(rounds):
            keys=list(cases);rnd.shuffle(keys)
            for key in keys:
                row,pol,d,src,frame,rec=cases[key]
                dst=d/f'tmp-{op}';a=src if op=='encode' else frame
                m=timed(command(pol,op,a,dst),d/f'{op}-{r}.log')
                m.update(input_id=row['id'],policy_id=pol['id'],operation=op,pair_id=str(r))
                with raw.open('a') as f: f.write(json.dumps(m,allow_nan=False)+'\n')
                if m['returncode']: raise RuntimeError('timed operation failed; evidence retained: '+str(key))
                expected=rec['frame_sha256'] if op=='encode' else row['input_sha256']
                if not dst.exists() or sha(dst.read_bytes())!=expected:
                    raise ValueError('CORRUPTION or nondeterministic frame: '+str(key))
                dst.unlink();samples[(key,op,r)]=m
    for key,entry in cases.items():
        row,pol,d,src,frame,rec=entry
        rec['runs']=[]
        for r in range(rounds):
            e=samples[(key,'encode',r)];de=samples[(key,'decode',r)]
            rec['runs'].append(dict(pair_id=str(r),encode_wall_s=e['wall_s'],encode_cpu_s=e['cpu_s'],
                                    decode_wall_s=de['wall_s'],decode_cpu_s=de['cpu_s'],
                                    peak_memory_bytes=1024*max(e['peak_rss_kib'],de['peak_rss_kib']),
                                    **cost(e,de,rec['stored_bytes'])))
        rec['status']='MEASURED_DEVELOPMENT_CONTROL';rec['byte_exact_verified']=True
        rec['medians']={k:statistics.median(r[k] for r in rec['runs']) for k in rec['runs'][0] if k!='pair_id'}
    result=dict(status='BASELINES_MEASURED_NOT_POLICY_ACCEPTANCE',records=rows,
                completed=len(cases),failed_profiles=len(rows)-len(cases),rounds=rounds,
                limitations=['Observed development; no fresh policy acceptance.',
                             'Full CLI scope differs from P2-E1 in-memory scope; do not pool timings.',
                             'NPY and derived CSV are correlated views, not independent families.',
                             'No trained OpenZL comparator or new column-first encoder yet.'])
    dump(output/'records.json',result)
    lines=['# N1 complete CLI development controls','', '| Input | Fixed configuration | Bytes | Encode ms | Decode ms | Archive model USD |', '|---|---|---:|---:|---:|---:|']
    for rec in rows:
        if rec['status']=='MEASURED_DEVELOPMENT_CONTROL':
            m=rec['medians'];lines.append(f"| {rec['input_id']} | {rec['policy']['id']} | {rec['stored_bytes']} | {1000*m['encode_wall_s']:.4f} | {1000*m['decode_wall_s']:.4f} | {m['archive_cost_usd']:.9g} |")
        else: lines.append(f"| {rec['input_id']} | {rec['policy']['id']} | ERROR | — | — | — |")
    (output/'tables.md').write_text('\n'.join(lines)+'\n')
    return result

def confirm(first: Path, second: Path) -> dict:
    a=json.loads((first/'records.json').read_text());b=json.loads((second/'records.json').read_text())
    def fixed(d): return {(r['input_id'],r['policy']['id']): (r['status'],r.get('stored_bytes'),r.get('frame_sha256'),r['input_sha256']) for r in d['records']}
    if fixed(a)!=fixed(b): raise ValueError('repeat identities differ')
    return dict(status='IDENTITIES_MATCH',configurations=len(a['records']),completed=a['completed'],failed_profiles=a['failed_profiles'])

def main():
    p=argparse.ArgumentParser(description=__doc__);s=p.add_subparsers(dest='action',required=True)
    q=s.add_parser('prepare');q.add_argument('archive',type=Path);q.add_argument('output',type=Path)
    q=s.add_parser('run');q.add_argument('prepared',type=Path);q.add_argument('output',type=Path)
    for name in ['zstd','openzl','kolmo']: q.add_argument('--'+name,required=True)
    q.add_argument('--seed',type=int,default=20261112);q.add_argument('--rounds',type=int,default=3)
    q=s.add_parser('confirm');q.add_argument('first',type=Path);q.add_argument('second',type=Path)
    a=p.parse_args()
    if a.action=='prepare': prepare(a.archive,a.output)
    elif a.action=='run': run(a.prepared,a.output,{n:str(Path(getattr(a,n)).resolve()) for n in ['zstd','openzl','kolmo']},a.rounds,a.seed)
    else: print(json.dumps(confirm(a.first,a.second)))
if __name__=='__main__': main()
