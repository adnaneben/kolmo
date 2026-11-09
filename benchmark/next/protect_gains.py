#!/usr/bin/env python3
"""Verify checkpoint integrity, default frames and paired throughput regressions."""
import argparse,collections,ctypes as C,csv,hashlib,json,pathlib,random,statistics,sys,tarfile,time
root=pathlib.Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--sources-only',action='store_true');p.add_argument('--repeats',type=int,default=5);a=p.parse_args()
checkpoint=root/'benchmark/checkpoints/cpu-planning-v1';manifest=json.loads((checkpoint/'manifest.json').read_text())
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
assert sha(checkpoint/'source.tar.gz')==manifest['source_archive_sha256'],'Checkpoint archive changed'
with tarfile.open(checkpoint/'source.tar.gz') as archive:
 for entry in manifest['files']:
  path=root/entry['path'];saved=archive.extractfile(entry['path']).read();assert hashlib.sha256(saved).hexdigest()==entry['sha256']
  if entry['path'] in ['Makefile','README.md','.github/workflows/ci.yml']:
   assert path.read_bytes().startswith(saved),f'Protected build/docs changed instead of appending: {path}'
  else:assert sha(path)==entry['sha256'],f'Protected source changed: {path}'
print('CHECKPOINT SOURCE GATE PASSED',flush=True)
if a.sources_only:raise SystemExit(0)
assert a.repeats>=5
reference=checkpoint/'libkolmo.dylib';assert sha(reference)==manifest['reference_library_sha256']
sys.path.insert(0,str(root/'bindings/python'))
from kolmo import _Params
libs={}
for label,path in [('checkpoint',reference),('current',root/'build/libkolmo.dylib')]:
 x=C.CDLL(str(path));x.czn_bound.argtypes=[C.c_size_t,C.POINTER(_Params)];x.czn_bound.restype=C.c_size_t
 x.czn_compress.argtypes=[C.c_char_p,C.c_size_t,C.c_char_p,C.c_size_t,C.POINTER(_Params)];x.czn_compress.restype=C.c_int64
 x.czn_decompress.argtypes=[C.c_char_p,C.c_size_t,C.c_char_p,C.c_size_t,C.c_int];x.czn_decompress.restype=C.c_int64;libs[label]=x
assert C.cast(libs['checkpoint'].czn_compress,C.c_void_p).value!=C.cast(libs['current'].czn_compress,C.c_void_p).value
files=['silesia/mozilla','silesia/nci','owid-co2-data.csv'];inputs={f:(root/'benchmark/audit/data'/f).read_bytes() for f in files}
jobs=[(label,f,t,i) for label in libs for f in files for t in [1,4,10] for i in range(a.repeats)]
random.Random(20261107).shuffle(jobs);refs={};times=collections.defaultdict(list)
with (root/'benchmark/next/protected-performance.csv').open('w') as out:
 w=csv.writer(out);w.writerow(['version','file','threads','repeat','input_bytes','compressed_bytes','compress_seconds'])
 for label,f,t,i in jobs:
  lib=libs[label];data=inputs[f];par=_Params(1,0,t,0,1);buf=C.create_string_buffer(lib.czn_bound(len(data),C.byref(par)))
  begin=time.perf_counter();n=lib.czn_compress(data,len(data),buf,len(buf),C.byref(par));elapsed=time.perf_counter()-begin;assert n>0
  blob=buf.raw[:n];assert blob==refs.setdefault(f,blob),(f,t,'default frame changed')
  back=C.create_string_buffer(len(data));assert lib.czn_decompress(blob,n,back,len(back),t)==len(data) and back.raw==data
  times[(label,f,t)].append(elapsed);w.writerow([label,f,t,i,len(data),n,elapsed]);out.flush()
regressions=[];summary=[]
for f in files:
 for t in [1,4,10]:
  old=statistics.median(times[('checkpoint',f,t)]);new=statistics.median(times[('current',f,t)]);ratio=new/old
  summary.append({'file':f,'threads':t,'checkpoint_mbps':len(inputs[f])/old/1e6,'current_mbps':len(inputs[f])/new/1e6,'elapsed_ratio':ratio})
  if ratio>1.05:regressions.append((f,t,ratio))
(root/'benchmark/next/protected-gate.json').write_text(json.dumps({'default_frames_identical':True,'repeats':a.repeats,'max_slowdown':1.05,'passed':not regressions,'measurements':summary},indent=2)+'\n')
assert not regressions,f'Investigate and repeat before promotion; do not replace checkpoint: {regressions}'
print('DEFAULT FRAME AND PERFORMANCE GATES PASSED (paired medians; slowdown threshold 5%)')
