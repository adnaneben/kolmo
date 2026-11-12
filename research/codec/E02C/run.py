#!/usr/bin/env python3
"""E02C DEVELOPMENT matrix; forced numeric path, not production integration.
Native C timers exclude Python/process startup/I/O. Separate ordinary Kolmo/zstd
CLI measurements include those costs and must not be compared as kernel speeds.
"""
import argparse,csv,hashlib,io,json,os,platform,random,statistics,struct,subprocess,time
from pathlib import Path
from decimal import Decimal
import numpy as np
from reference import restore
ROOT=Path(__file__).resolve().parents[3]
METHODS={0:'upstream_layout',1:'global_bitpack',2:'local_bitpack',3:'patched',4:'local_byteplanes'}
CODERS={0:'stored',13:'huffman',14:'rans',3:'zstd_1'}
def sha(b):return hashlib.sha256(b).hexdigest()
def save(p,x):p.write_text(json.dumps(x,indent=2)+'\n')
def npy(a):
    stream=io.BytesIO();np.save(stream,a,allow_pickle=False);b=stream.getvalue()
    if b[:8]!=b'\x93NUMPY\x01\x00':raise ValueError('unexpected NPY version')
    return b,10+int.from_bytes(b[8:10],'little')
def cases():
    from sklearn.datasets import load_digits
    from plotly.data import gapminder
    import statsmodels.api as sm
    d=load_digits();g=gapminder();n=sm.datasets.nile.load_pandas().data;s=sm.datasets.sunspots.load_pandas().data
    # Conversions verified exact; these are derived typed arrays, NOT source CSVs.
    real=[('digits_pixels',d.data,'<u2','sklearn-digits'),('digits_labels',d.target,'<u2','sklearn-digits'),('gapminder_population',g['pop'],'<u4','plotly-gapminder'),('gapminder_year',g['year'],'<u2','plotly-gapminder'),('nile_volume',n['volume'],'<u2','statsmodels-nile'),('sunspots_year',s['YEAR'],'<u2','statsmodels-sunspots')]
    for name,v,dtype,family in real:
        a=np.asarray(v,dtype=dtype)
        if not np.array_equal(np.asarray(v),a):raise ValueError('inexact cast')
        data,head=npy(a);yield dict(name=name,family=family,kind='real_source_derived',width=a.dtype.itemsize,head=head,data=data,provenance=f'{family}; {name}; exact integral cast {dtype}; original row ordering; original CSV not reconstructed')
    activity=[Decimal(str(x))*10 for x in s['SUNACTIVITY']]
    if any(x!=x.to_integral_value() or x<0 or x>65535 for x in activity):raise ValueError('sunspot scaling is not exact')
    a=np.array([int(x) for x in activity],dtype='<u2');data,head=npy(a)
    yield dict(name='sunspots_activity_tenths',family='statsmodels-sunspots',kind='real_source_derived',width=2,head=head,data=data,provenance='statsmodels sunspots SUNACTIVITY; exact decimal-string scaling by 10; full derived NPY, not original CSV')
    for seed in (3101,3102,3103):
        rng=np.random.default_rng(seed);count=16384;i=np.arange(count,dtype=np.uint64)
        arrays={'counter_jitter':(i*1000+rng.integers(0,8,count,dtype=np.uint64)).astype('<u8'),'quadratic_jitter':(i*i*3+rng.integers(0,4,count,dtype=np.uint64)).astype('<u8'),'bounded_9bit':rng.integers(0,512,count,dtype=np.uint32).astype('<u4'),'full_range_u64':rng.integers(0,1<<64,count,dtype=np.uint64).astype('<u8'),'sparse_large_values':rng.integers(0,8,count,dtype=np.uint64).astype('<u8'),'changing_ranges':np.concatenate([rng.integers(0,8,count//2,dtype=np.uint32),rng.integers(0,65536,count//2,dtype=np.uint32)]).astype('<u4')}
        arrays['sparse_large_values'][::251]=(1<<61)+np.arange(len(arrays['sparse_large_values'][::251]),dtype=np.uint64)
        for family,a in arrays.items():
            data,head=npy(a);yield dict(name=f'{family}_{seed}',family=family,kind='synthetic',width=a.dtype.itemsize,head=head,data=data,provenance=f'NumPy PCG64 seed={seed}; generation in run.py')
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--probe',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--repeats',type=int,default=5);args=p.parse_args()
    if not 1<=args.repeats<=100:p.error('repeats outside bound')
    out=args.output.resolve();out.mkdir(parents=True,exist_ok=False);probe=args.probe.resolve();frames=out/'frames';frames.mkdir();corpus=out/'corpus';corpus.mkdir()
    status={'status':'running','phase':'E02C','scope':'DEVELOPMENT; fixed entropy controls, forced numeric pipeline, no production-format change','commands':[]}
    save(out/'status.json',status)
    def run(cmd,timeout=300):
        entry={'argv':list(map(str,cmd))};status['commands'].append(entry);save(out/'status.json',status);start=time.perf_counter_ns()
        proc=subprocess.run(entry['argv'],capture_output=True,text=True,timeout=timeout,env=dict(os.environ,OMP_NUM_THREADS='1',OPENBLAS_NUM_THREADS='1',MKL_NUM_THREADS='1'))
        entry.update(returncode=proc.returncode,process_wall_ns=time.perf_counter_ns()-start)
        (out/f'command-{len(status["commands"]):04d}.log').write_text(proc.stdout+proc.stderr)
        if proc.returncode:raise RuntimeError(f'command failed: {entry}; {proc.stderr[-2000:]}')
        return proc.stdout,entry['process_wall_ns']
    try:
        import sklearn,statsmodels,plotly
        env={'python':platform.python_version(),'platform':platform.platform(),'numpy':np.__version__,'sklearn':sklearn.__version__,'statsmodels':statsmodels.__version__,'plotly':plotly.__version__,'cpu':Path('/proc/cpuinfo').read_text().split('\n\n')[0],'commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'probe_sha256':sha(probe.read_bytes())}
        env['zstd_cli_version']=subprocess.check_output(['zstd','--version'],text=True).strip()
        save(out/'environment.json',env)
        manifest=[];inputs=list(cases());rows=[];captures={};rng=random.Random(81411)
        tasks=[]
        for case in inputs:
            (corpus/(case['name']+'.npy')).write_bytes(case['data'])
            manifest.append({**{k:v for k,v in case.items() if k!='data'},'sha256':sha(case['data']),'bytes':len(case['data']),'partition':'development'})
            tasks.extend((case,m,c) for m in METHODS for c in CODERS)
        save(out/'corpus_manifest.json',manifest);rng.shuffle(tasks)
        with (out/'timings.jsonl').open('w') as timings:
            for case,m,c in tasks:
                prefix=frames/f"{case['name']}-{m}-{c}"
                text,process_wall=run([probe,'bench',corpus/(case['name']+'.npy'),case['width'],case['head'],m,c,args.repeats,prefix])
                records=[json.loads(x) for x in text.splitlines() if x.startswith('{')];ss=[r for r in records if r['type']=='summary'];ts=[r for r in records if r['type']=='timing']
                if len(ss)!=1 or len(ts)!=args.repeats:raise RuntimeError('missing native measurements')
                summary=ss[0];f=Path(str(prefix)+'.frame').read_bytes();rep=Path(str(prefix)+'.represented').read_bytes()
                recovered,residual,meta=restore(f,rep)
                if recovered!=case['data'] or residual!=Path(str(prefix)+'.residual').read_bytes() or Path(str(prefix)+'.decoded').read_bytes()!=case['data']:raise RuntimeError('independent reconstruction failed')
                ident={'params':sha(Path(str(prefix)+'.params').read_bytes()),'residual':sha(residual),'rest':sha(Path(str(prefix)+'.rest').read_bytes())}
                if case['name'] in captures and captures[case['name']]!=ident:raise RuntimeError('predictor/residual changed across configurations')
                captures[case['name']]=ident
                for r in ts:timings.write(json.dumps(dict(case=case['name'],method=METHODS[m],backend=CODERS[c],**r))+'\n')
                row=dict(case=case['name'],kind=case['kind'],family=case['family'],method=METHODS[m],backend=CODERS[c],**{k:v for k,v in summary.items() if k not in ('type','count_scope')},frame_sha256=sha(f),order=meta['order'],gcd=meta['gcd'],process_batch_wall_ns=process_wall)
                for field in ('encode_ns','decode_ns','encode_cpu_ns','decode_cpu_ns','representation_encode_ns','representation_decode_ns'):row[field]=statistics.median(r[field] for r in ts)
                rows.append(row)
        save(out/'capture_hashes.json',captures)
        with (out/'measurements.csv').open('w',newline='') as f:
            w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
        # Separate product controls: real production planner, normal CLI and framing.
        product=[]
        for case in inputs:
            source=corpus/(case['name']+'.npy')
            for profile in ('balanced','hc'):
                dest=frames/(case['name']+'.kolmo-'+profile);decoded=Path(str(dest)+'.out');trials=[]
                for rep in range(3):
                    _,enc=run([ROOT/'build/kolmo','-m',profile,'-T','1',source,dest]);_,dec=run([ROOT/'build/kolmo','-d','-T','1',dest,decoded])
                    if decoded.read_bytes()!=case['data']:raise RuntimeError('production roundtrip')
                    trials.append({'encode_process_wall_ns':enc,'decode_process_wall_ns':dec})
                frame=dest.read_bytes();nb=int.from_bytes(frame[20:24],'little');blocks=[];offset=32+16*nb
                if frame[:4]!=b'CZN1' or offset>len(frame):raise RuntimeError('unexpected product frame')
                for i in range(nb):
                    e=frame[32+16*i:48+16*i];cn=int.from_bytes(e[4:8],'little');blocks.append({'codec':e[12],'tflag':e[13],'plan_prefix_hex':frame[offset:offset+min(4,cn)].hex()});offset+=cn
                if offset!=len(frame):raise RuntimeError('product index length')
                product.append({'case':case['name'],'tool':'kolmo','profile':profile,'stored_bytes':len(frame),'frame_sha256':sha(frame),'blocks':blocks,'trials':trials,'scope':'normal uninstrumented production CLI; startup and file I/O included'})
            dest=frames/(case['name']+'.zst');decoded=Path(str(dest)+'.out')
            _,enc=run(['zstd','--single-thread','-1','-q','-f',source,'-o',dest]);_,dec=run(['zstd','-d','-q','-f',dest,'-o',decoded])
            if decoded.read_bytes()!=case['data']:raise RuntimeError('direct zstd roundtrip')
            product.append({'case':case['name'],'tool':'system-zstd','profile':'single-thread-1','stored_bytes':dest.stat().st_size,'encode_process_wall_ns':enc,'decode_process_wall_ns':dec,'scope':'single CLI observation; size control only, not speed evidence'})
        save(out/'product_controls.json',product)
        aggregates=[]
        for kind in ('real_source_derived','synthetic'):
            for c,name in CODERS.items():
                base=sum(r['stored_bytes'] for r in rows if r['kind']==kind and r['backend']==name and r['method']=='upstream_layout')
                for method in METHODS.values():
                    rr=[r for r in rows if r['kind']==kind and r['backend']==name and r['method']==method];n=sum(r['stored_bytes'] for r in rr)
                    aggregates.append({'kind':kind,'backend':name,'method':method,'stored_bytes':n,'relative_change_pct':100*(n/base-1),'sum_median_encode_ns':sum(r['encode_ns'] for r in rr),'sum_median_decode_ns':sum(r['decode_ns'] for r in rr),'cases':len(rr)})
        save(out/'summary.json',{'configurations':len(rows),'inputs':len(inputs),'timing_pairs':len(rows)*args.repeats,'aggregates':aggregates,'limitations':['development-only','source families small/correlated; digits can dominate totals','forced numeric interpretation; no representation selector','research envelope is not Kolmo framing','peak RSS includes input/capture/retained frame/library overhead','one shared Linux host; no M5 results','independent reference covers packing and integer inverse, not entropy codec internals']})
        status['status']='passed_development_matrix_NOT_RELEASE_ACCEPTANCE';status['configurations']=len(rows);save(out/'status.json',status)
        print(json.dumps({'status':status['status'],'inputs':len(inputs),'configurations':len(rows)},indent=2))
        for a in aggregates:print(json.dumps(a))
    except BaseException as exc:
        status['status']='failed_or_interrupted';status['error']=str(exc);save(out/'status.json',status);raise
if __name__=='__main__':main()
