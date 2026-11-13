#!/usr/bin/env python3
"""E02D: frozen, source-disjoint numeric validation. No codec tuning."""
from __future__ import annotations
import argparse, csv, gzip, hashlib, io, json, os, platform, random, shutil
import statistics, subprocess, sys, time, urllib.request, zipfile
from array import array
from decimal import Decimal, InvalidOperation
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[3]
FROZEN = '706cdab65caace2ce698e5dfe4c5dd99e8fcbe00'
REGISTRATION = 'e6bf5644a25cb39be7ccf22f3c899f127628e338'
METHODS = {0:'upstream_layout',1:'global_bitpack',2:'local_bitpack',3:'patched',4:'local_byteplanes'}
CODERS = {0:'stored',13:'huffman',14:'rans',3:'zstd_1'}
SOURCES = [
    dict(id=235, family='uci_household_power', slug='individual+household+electric+power+consumption', member='household_power_consumption.txt', fields=['Sub_metering_1','Sub_metering_2','Sub_metering_3'], limit=524288, delimiter=';', attribution='Hebrail & Berard; DOI 10.24432/C58K54; CC BY 4.0'),
    dict(id=492, family='uci_metro', slug='metro+interstate+traffic+volume', member='Metro_Interstate_Traffic_Volume.csv.gz', fields=['traffic_volume','clouds_all'], limit=None, delimiter=',', attribution='Hogue; DOI 10.24432/C5X60B; CC BY 4.0'),
    dict(id=275, family='uci_bike', slug='bike+sharing+dataset', member='hour.csv', fields=['casual','registered','cnt','instant'], limit=None, delimiter=',', attribution='Fanaee-T; UCI Bike Sharing 275; CC BY 4.0'),
    dict(id=374, family='uci_appliances', slug='appliances+energy+prediction', member='energydata_complete.csv', fields=['Appliances','lights'], limit=None, delimiter=',', attribution='Candanedo; DOI 10.24432/C5VC8G; CC BY 4.0'),
]
LIMIT = 200 << 20

def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def save(path: Path, data) -> None:
    path.write_text(json.dumps(data, indent=2, allow_nan=False)+'\n')

def file_sha(path: Path) -> str:
    h=hashlib.sha256()
    with path.open('rb') as f:
        for b in iter(lambda:f.read(1<<20),b''): h.update(b)
    return h.hexdigest()

def assert_frozen() -> dict:
    paths=['research/codec/E02C','src','vendor','Makefile','benchmark/accept/TARGETS.md']
    subprocess.run(['git','diff','--exit-code',FROZEN,'--',*paths],cwd=ROOT,check=True)
    protocol='research/codec/E02D/PREREGISTRATION.md'
    registered=subprocess.check_output(['git','show',f'{REGISTRATION}:{protocol}'],cwd=ROOT)
    if (ROOT/protocol).read_bytes()!=registered: raise ValueError('Preregistration changed')
    return dict(codec_reference=FROZEN,registration_commit=REGISTRATION,
                registration_sha256=sha(registered),head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip())

def download(source: dict, cache: Path) -> Path:
    path=cache/f"{source['id']}.zip"
    if path.exists(): raise ValueError('Refusing an unverified prior cache')
    url=f"https://archive.ics.uci.edu/static/public/{source['id']}/{source['slug']}.zip"
    req=urllib.request.Request(url,headers={'User-Agent':'Kolmo-E02D-frozen-research/1.0'})
    with urllib.request.urlopen(req,timeout=120) as r, path.open('xb') as f:
        if not r.geturl().startswith('https://archive.ics.uci.edu/'): raise ValueError('Unexpected download redirect')
        total=0
        for b in iter(lambda:r.read(1<<20),b''):
            total+=len(b)
            if total>LIMIT: raise ValueError('Archive download exceeds bound')
            f.write(b)
    if not zipfile.is_zipfile(path): raise ValueError('Publisher did not return a ZIP')
    return path

def parse_csv(stream, source: dict):
    reader=csv.DictReader(stream,delimiter=source['delimiter'])
    fields=source['fields']
    if not reader.fieldnames or any(k not in reader.fieldnames for k in fields): raise ValueError('Missing declared columns')
    if len(reader.fieldnames)!=len(set(reader.fieldnames)): raise ValueError('Duplicate CSV columns')
    values={k:array('I') for k in fields}; indices={k:array('I') for k in fields}
    if array('I').itemsize!=4: raise ValueError('32-bit array type required')
    missing={k:0 for k in fields}; rows=0
    for index,row in enumerate(reader):
        if source['limit'] is not None and index>=source['limit']: break
        if None in row or any(row.get(k) is None for k in fields): raise ValueError('Ragged source row')
        rows+=1
        for k in fields:
            text=row[k].strip()
            if text in ('','?'): missing[k]+=1; continue
            try: d=Decimal(text)
            except InvalidOperation as exc: raise ValueError('Invalid numeric token') from exc
            if not d.is_finite() or d!=d.to_integral_value() or not 0<=d<=4294967295: raise ValueError('Inexact uint32 conversion')
            values[k].append(int(d)); indices[k].append(index)
    if source['limit'] is not None and rows!=source['limit']: raise ValueError('Source shorter than preregistered window')
    return values,indices,missing,rows

def npy_bytes(values) -> tuple[bytes,int]:
    b=io.BytesIO(); np.save(b,np.asarray(values,dtype='<u4'),allow_pickle=False); data=b.getvalue()
    if data[:8]!=b'\x93NUMPY\x01\x00': raise ValueError('Unexpected NPY version')
    return data,10+int.from_bytes(data[8:10],'little')

def prepare(out: Path) -> None:
    identity=assert_frozen(); out.mkdir(parents=True,exist_ok=False)
    corpus=out/'corpus';corpus.mkdir(); cache=out/'downloads';cache.mkdir(); provenance=out/'provenance';provenance.mkdir()
    state=dict(status='preparing',identity=identity,sources=[])
    save(out/'preparation.json',state); manifest=[]
    try:
        for source in SOURCES:
            path=download(source,cache)
            with zipfile.ZipFile(path) as z:
                matches=[i for i in z.infolist() if Path(i.filename).name==source['member']]
                if len(matches)!=1 or matches[0].file_size>LIMIT: raise ValueError('Unexpected archive member/bound')
                info=matches[0]
                # Hash publisher member without parsing or selecting outcomes.
                h=hashlib.sha256()
                with z.open(info) as f:
                    for b in iter(lambda:f.read(1<<20),b''): h.update(b)
                with z.open(info) as f:
                    raw=gzip.GzipFile(fileobj=f) if source['member'].endswith('.gz') else f
                    with io.TextIOWrapper(raw,encoding='utf-8-sig',newline='') as text:
                        values,indices,missing,rows=parse_csv(text,source)
            source_record={**source,'archive_sha256':file_sha(path),'archive_bytes':path.stat().st_size,'member_sha256':h.hexdigest(),'selected_source_rows':rows,'missing':missing,
                           'url':f"https://archive.ics.uci.edu/static/public/{source['id']}/{source['slug']}.zip"}
            state['sources'].append(source_record)
            for field in source['fields']:
                if len(values[field])<1024: raise ValueError('Insufficient valid values')
                stream_id=source['family']+'_'+field.lower()
                ix=np.asarray(indices[field],dtype='<u4').tobytes();(provenance/(stream_id+'.rows-u32le')).write_bytes(ix)
                for label,n in [('full',len(values[field])),('small128',128),('small1024',1024)]:
                    data,head=npy_bytes(values[field][:n]); name=stream_id+'_'+label; (corpus/(name+'.npy')).write_bytes(data)
                    manifest.append(dict(name=name,stream=stream_id,family=source['family'],field=field,size_class=label,count=n,width=4,head=head,bytes=len(data),sha256=sha(data),row_map_sha256=sha(ix[:n*4]),
                                         source_archive_sha256=source_record['archive_sha256'],non_target_control=(source['id'],field) in ((492,'clouds_all'),(275,'instant')),
                                         scope='derived valid-value NPY; not source CSV or missing-position reconstruction'))
            save(out/'preparation.json',state)
        if len(manifest)!=33: raise ValueError('Incomplete registered corpus')
        save(out/'corpus_manifest.json',manifest)
        state.update(status='prepared_before_measurement',cases=len(manifest),families=len(SOURCES));save(out/'preparation.json',state)
        print(json.dumps({'prepared':len(manifest),'full_stream_bytes':sum(x['bytes'] for x in manifest if x['size_class']=='full')}))
    except BaseException as exc:
        state.update(status='failed_NOT_VALIDATED',error=str(exc));save(out/'preparation.json',state);raise

def phenotype(residual: bytes, meta: dict) -> dict:
    n=meta['count']-meta['order']; ow=meta['output_width']
    if len(residual)!=n*ow: raise ValueError('Bad residual size')
    # Read original production residual representation; never tune it here.
    values=[]
    for i in range(n):
        values.append(sum(residual[j*n+i if meta['flags']&8 else i*ow+j]<<(8*j) for j in range(ow)))
    return phenotype_values(values)

def phenotype_values(values: list[int]) -> dict:
    widths=[v.bit_length() for v in values];n=len(widths)
    if not n:return dict(count=0,H1=False,H2=False,H4=False)
    ordered=sorted(widths);q99=ordered[int(.99*(n-1))];mx=max(widths);tail=sum(w>q99 for w in widths)/n
    local=sorted(max(widths[i:i+128]) for i in range(0,n-n%128,128))
    spread=local[int(.9*(len(local)-1))]-local[int(.1*(len(local)-1))] if local else 0
    return dict(count=n,max_bits=mx,p99_bits=q99,exception_fraction=tail,complete_blocks=len(local),local_width_spread=spread,
                H1=n>=4096 and q99<=8 and mx-q99>=4 and 0<tail<=.01,
                H2=len(local)>=32 and spread>=4,H4=1<=mx<=16 and mx%8!=0)

def measured_run(probe: Path, prepared: Path, out: Path, repeats: int, seed: int, subset: str='all') -> None:
    identity=assert_frozen(); out.mkdir(parents=True,exist_ok=False)
    sys.path.insert(0,str(ROOT/'research/codec/E02C'))
    from reference import restore
    manifest=json.loads((prepared/'corpus_manifest.json').read_text())
    if subset!='all':manifest=[c for c in manifest if c['size_class']==subset]
    for c in manifest:
        if file_sha(prepared/'corpus'/(c['name']+'.npy'))!=c['sha256']:raise ValueError('Prepared input changed')
    status=dict(status='running',identity=identity,subset=subset,repeats=repeats,seed=seed)
    save(out/'status.json',status)
    save(out/'environment.json',dict(identity=identity,platform=platform.platform(),python=sys.version,numpy=np.__version__,probe_sha256=file_sha(probe),cpu=Path('/proc/cpuinfo').read_text().split('\n\n')[0]))
    rows=[]; captures={}; phenotypes={};scratch=out/'scratch';scratch.mkdir()
    def command(cmd):
        args=list(map(str,cmd));start=time.perf_counter_ns()
        proc=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,timeout=900,
                            env=dict(os.environ,OMP_NUM_THREADS='1',OPENBLAS_NUM_THREADS='1',MKL_NUM_THREADS='1'))
        elapsed=time.perf_counter_ns()-start
        with (out/'commands.jsonl').open('a') as f:f.write(json.dumps(dict(argv=args,returncode=proc.returncode,process_wall_ns=elapsed,stdout=proc.stdout,stderr=proc.stderr))+'\n')
        if proc.returncode: raise RuntimeError('Command failed: '+proc.stderr[-2000:])
        return proc.stdout
    tasks=[(c,m,b) for c in manifest for m in METHODS for b in CODERS];random.Random(seed).shuffle(tasks)
    try:
        with (out/'timings.jsonl').open('w') as tf, (out/'measurements.jsonl').open('w') as rf:
            for c,m,b in tasks:
                source=prepared/'corpus'/(c['name']+'.npy');prefix=scratch/'sample';data=source.read_bytes()
                text=command([probe,'bench',source,4,c['head'],m,b,repeats,prefix]);records=[json.loads(s) for s in text.splitlines() if s.startswith('{')]
                times=[r for r in records if r['type']=='timing'];summaries=[r for r in records if r['type']=='summary']
                if len(times)!=repeats or len(summaries)!=1: raise ValueError('Missing native observations')
                frame=Path(str(prefix)+'.frame').read_bytes();represented=Path(str(prefix)+'.represented').read_bytes()
                recovered,residual,meta=restore(frame,represented)
                if recovered!=data or Path(str(prefix)+'.decoded').read_bytes()!=data or Path(str(prefix)+'.residual').read_bytes()!=residual:raise ValueError('Independent full-input reconstruction failed')
                cap={s:file_sha(Path(str(prefix)+'.'+s)) for s in ('params','rest','residual')}
                if c['name'] in captures and cap!=captures[c['name']]:raise ValueError('Predictor/residual drift')
                captures[c['name']]=cap
                if c['name'] not in phenotypes:phenotypes[c['name']]=phenotype(residual,meta)
                summary=summaries[0]
                row={**c,**{k:v for k,v in summary.items() if k not in ('type','count_scope')},'method':METHODS[m],'backend':CODERS[b],'frame_sha256':sha(frame),'order':meta['order'],'gcd':meta['gcd']}
                for k in ('encode_ns','decode_ns','encode_cpu_ns','decode_cpu_ns','representation_encode_ns','representation_decode_ns'):row[k]=statistics.median(t[k] for t in times)
                rows.append(row);rf.write(json.dumps(row)+'\n');rf.flush()
                for t in times:tf.write(json.dumps(dict(case=c['name'],method=METHODS[m],backend=CODERS[b],**t))+'\n')
                for p in scratch.glob('sample.*'):p.unlink()
        save(out/'capture_hashes.json',captures);save(out/'phenotypes.json',phenotypes)
        product=[]
        if subset=='all':
            for c in manifest:
                source=prepared/'corpus'/(c['name']+'.npy');data=source.read_bytes()
                for profile in ('balanced','hc','zstd_1'):
                    dest=scratch/'product';decoded=scratch/'decoded'
                    if profile=='zstd_1':
                        command(['zstd','--single-thread','-1','-q','-f',source,'-o',dest]);command(['zstd','-d','-q','-f',dest,'-o',decoded])
                    else:
                        command([ROOT/'build/kolmo','-m',profile,'-T','1',source,dest]);command([ROOT/'build/kolmo','-d','-T','1',dest,decoded])
                    if decoded.read_bytes()!=data:raise ValueError('Product-control reconstruction failed')
                    product.append(dict(case=c['name'],family=c['family'],size_class=c['size_class'],profile=profile,stored_bytes=dest.stat().st_size,frame_sha256=file_sha(dest),scope='complete-tool SIZE control; not in-process speed'))
                    dest.unlink();decoded.unlink()
            save(out/'product_controls.json',product)
        if len(rows)!=len(manifest)*20:raise ValueError('Incomplete matrix')
        status.update(status='passed_frozen_matrix_NOT_RELEASE_ACCEPTANCE',cases=len(manifest),configurations=len(rows),timing_pairs=len(rows)*repeats);save(out/'status.json',status)
        scratch.rmdir();print(json.dumps(status))
    except BaseException as exc:
        status.update(status='failed_NOT_VALIDATED',error=str(exc));save(out/'status.json',status);raise

def load_rows(folder: Path) -> list[dict]:
    return [json.loads(s) for s in (folder/'measurements.jsonl').read_text().splitlines()]

def analyze(first: Path, second: Path, out: Path) -> None:
    a,b=load_rows(first),load_rows(second);key=lambda r:(r['name'],r['method'],r['backend'])
    aa={key(r):r for r in a};bb={key(r):r for r in b}
    if len(a)!=660 or len(b)!=660 or len(aa)!=660 or aa.keys()!=bb.keys():raise ValueError('Incomplete or duplicate matrix')
    for k in aa:
        for field in ('stored_bytes','frame_sha256','sha256','requested_codec','actual_codec'):
            if aa[k][field]!=bb[k][field]:raise ValueError('Confirmation changed '+field)
    for name in ('capture_hashes.json','phenotypes.json','product_controls.json'):
        if json.loads((first/name).read_text())!=json.loads((second/name).read_text()):raise ValueError('Confirmation changed '+name)
    phen=json.loads((first/'phenotypes.json').read_text());cases={r['name']:r for r in a}
    totals=[]
    for size in ('full','small128','small1024'):
        for backend in CODERS.values():
            for method in METHODS.values():
                per=[]
                for family in sorted({r['family'] for r in a}):
                    rr=[r for r in a if r['family']==family and r['size_class']==size and r['backend']==backend and r['method']==method]
                    base=[aa[(r['name'],'upstream_layout',backend)] for r in rr]
                    n=sum(r['stored_bytes'] for r in rr);d=sum(r['stored_bytes'] for r in base)
                    per.append(dict(family=family,stored_bytes=n,control_bytes=d,change_pct=100*(n/d-1),worst_file_change_pct=max(100*(r['stored_bytes']/q['stored_bytes']-1) for r,q in zip(rr,base))))
                n=sum(x['stored_bytes'] for x in per);d=sum(x['control_bytes'] for x in per)
                totals.append(dict(size_class=size,backend=backend,method=method,stored_bytes=n,control_bytes=d,pooled_change_pct=100*(n/d-1),family_mean_change_pct=statistics.mean(x['change_pct'] for x in per),families=per))
    decisions=[]
    def evaluate(hyp,method,backend,controls,small=False):
        eligible=[c for c in cases.values() if ((c['size_class']!='full') if small else (c['size_class']=='full' and c['count']>=16384 and phen[c['name']][hyp]))]
        family_results=[]
        for family in sorted({c['family'] for c in eligible}):
            cc=[c for c in eligible if c['family']==family];comparisons=[]
            for control_method,control_backend in controls:
                rr=[aa[(c['name'],method,backend)] for c in cc];base=[aa[(c['name'],control_method,control_backend)] for c in cc]
                n=sum(r['stored_bytes'] for r in rr);d=sum(r['stored_bytes'] for r in base);timing=[]
                for index in (aa,bb):
                    rs=[index[(c['name'],method,backend)] for c in cc];bs=[index[(c['name'],control_method,control_backend)] for c in cc]
                    ratios={f:sum(r[f] for r in rs)/sum(r[f] for r in bs) for f in ('encode_ns','decode_ns')}
                    memory_key='post_exec_image_peak_rss_kib'
                    if any(r.get(memory_key,0)<=0 for r in rs+bs): raise ValueError('Missing native post-exec VmHWM')
                    ratios['native_image_memory_ratio']=max(r[memory_key]/q[memory_key] for r,q in zip(rs,bs))
                    timing.append(ratios)
                size_pass=d-n>=64 and n<=.95*d
                fast_pass=size_pass and all(t['decode_ns']<=1.10 and t['encode_ns']<=2 and t['native_image_memory_ratio']<=1.25 for t in timing)
                comparisons.append(dict(control_method=control_method,control_backend=control_backend,stored_bytes=n,control_bytes=d,saving_pct=100*(1-n/d),size_screen=size_pass,fast_screen=fast_pass,two_pass_cost_ratios=timing))
            family_results.append(dict(family=family,cases=[c['name'] for c in cc],comparisons=comparisons,size_screen=all(x['size_screen'] for x in comparisons),fast_screen=all(x['fast_screen'] for x in comparisons)))
        support=sum(x['size_screen'] for x in family_results);fast=sum(x['fast_screen'] for x in family_results)
        verdict='NOT_EXERCISED' if not family_results else ('SIZE_SIGNAL_NOT_INTEGRATION' if support>=2 else ('ONE_FAMILY_PROVISIONAL' if support else 'NO_SIZE_SIGNAL'))
        decisions.append(dict(hypothesis=hyp,method=method,backend=backend,eligible_families=len(family_results),supporting_families=support,fast_screen_families=fast,verdict=verdict,families=family_results))
    for backend in CODERS.values():
        evaluate('H1','patched',backend,[('local_byteplanes',backend)])
        evaluate('H2','local_bitpack',backend,[('local_byteplanes',backend),('global_bitpack',backend)])
        evaluate('H4','global_bitpack',backend,[('upstream_layout',backend)])
    for method in ('global_bitpack','local_bitpack','patched'):
        for control in ('huffman','rans'):evaluate('H3',method,'stored',[('upstream_layout',control)],small=True)
    result=dict(status='COMPLETE_FROZEN_VALIDATION_NOT_RELEASE_ACCEPTANCE',confirmed_configurations=660,full_input_bytes=sum(c['bytes'] for c in cases.values() if c['size_class']=='full'),full_values=sum(c['count'] for c in cases.values() if c['size_class']=='full'),families=4,aggregates=totals,decisions=decisions,
                limitations=['four source families, correlated channels and nested prefixes','derived valid-value NPY only; CSV spelling/missing maps excluded','shared Linux runner; no M5/default promotion','independent reference excludes entropy decoding','observed validation is not a future untouched holdout'])
    save(out,result)
    print(json.dumps({k:v for k,v in result.items() if k not in ('aggregates','decisions')},indent=2))
    for d in decisions:print(json.dumps({k:v for k,v in d.items() if k!='families'}))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);sub=p.add_subparsers(dest='action',required=True)
    q=sub.add_parser('prepare');q.add_argument('--output',type=Path,required=True)
    q=sub.add_parser('run');q.add_argument('--probe',type=Path,required=True);q.add_argument('--prepared',type=Path,required=True);q.add_argument('--output',type=Path,required=True);q.add_argument('--repeats',type=int,required=True);q.add_argument('--seed',type=int,required=True);q.add_argument('--subset',choices=['all','small128'],default='all')
    q=sub.add_parser('analyze');q.add_argument('first',type=Path);q.add_argument('confirmation',type=Path);q.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    if a.action=='prepare':prepare(a.output.resolve())
    elif a.action=='run':
        if not 1<=a.repeats<=100:p.error('repeat bound')
        measured_run(a.probe.resolve(),a.prepared.resolve(),a.output.resolve(),a.repeats,a.seed,a.subset)
    else:analyze(a.first.resolve(),a.confirmation.resolve(),a.output.resolve())
