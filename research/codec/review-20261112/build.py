#!/usr/bin/env python3
"""Build isolated exact leaf-function tests; never write to production source.
Requires Linux, a source root containing src/ and vendor/, gcc, and clang.
The candidate changes only the generated rANS encoder's singleton decision.
Vendor objects are uninstrumented; sanitizer scope is the leaf/harness TU.
"""
from __future__ import annotations
import argparse,concurrent.futures,hashlib,json,os,platform,subprocess,time
from pathlib import Path
CORE='e06467c8872d494b97270811bedf6fd0bac2aa31'
GRAPH='91ad067dc583b9d9e6f060f214a0f7c402bb4a31'
def blob(b:bytes)->str:return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 n=a.source.resolve();b=a.output.resolve();b.mkdir(parents=True,exist_ok=False)
 c=(n/'src/kolmo.c').read_bytes();g=(n/'src/kgraph.inc').read_bytes()
 if blob(c)!=CORE or blob(g)!=GRAPH:raise ValueError('Pinned source mismatch')
 s=c.decode();i=s.index('static int kgu_get(');j=s.index('static size_t codec_compress(',i)
 (b/'pinned_leaves.inc').write_text('#include <math.h>\n#include "common/mem.h"\n'+s[i:j])
 i=s.index('static int rans_norm(');j=s.index('/* rans64',i);norm=s[i:j]
 if norm.count('if (used < 2) return -1;')!=1:raise ValueError('Unique anchor required')
 norm=norm.replace('rans_norm(', 'review_rans_norm(').replace('if (used < 2) return -1;', 'if (used < 2) return used == 1 ? 0 : -1;')
 i=s.index('static size_t rans_compress_all(');j=s.index('static inline uint32_t r64_rd(',i)
 enc=s[i:j].replace('rans_compress_all(', 'review_rans_compress_all(').replace('rans_norm(', 'review_rans_norm(')
 (b/'rans_singleton.inc').write_text('/* Generated research-only candidate. */\n'+norm+enc)
 flags=['-ffunction-sections','-fdata-sections','-DXXH_NAMESPACE=ZSTD_','-DZSTD_MULTITHREAD','-DZSTD_LEGACY_SUPPORT=0','-DZSTD_DISABLE_ASM']
 incs=[f'-I{n/"vendor/zstd"}'];sources=[p for d in ('common','compress','decompress') for p in sorted((n/'vendor/zstd'/d).glob('*.c'))]
 commands=[];state={'scope':'exact extracted leaf functions; NOT full codec, M5, fresh validation or release','core_blob':CORE,'graph_blob':GRAPH,'platform':platform.platform(),'commands':commands}
 def save(): (b/'receipt.json').write_text(json.dumps(state,indent=2)+'\n')
 def run(cmd,log):
  start=time.monotonic();r=subprocess.run(list(map(str,cmd)),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'))
  (b/log).write_bytes(r.stdout)
  return {'argv':list(map(str,cmd)),'returncode':r.returncode,'seconds':time.monotonic()-start,'log':log,'log_sha256':hashlib.sha256(r.stdout).hexdigest()}
 def compile_one(item):
  idx,src=item;obj=b/f'vendor-{idx}.o';r=run(['gcc','-O2',*flags,*incs,'-c',src,'-o',obj],f'vendor-{idx}.log');r['object']=str(obj);r['source_sha256']=hashlib.sha256(src.read_bytes()).hexdigest();return r
 try:
  with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:commands.extend(pool.map(compile_one,enumerate(sources)))
  save()
  if any(r['returncode'] for r in commands):raise RuntimeError('Vendor build failed')
  objs=[r['object'] for r in commands]
  for cc,san in [('gcc',False),('clang',True)]:
   name='compat-asan' if san else 'compat-release';exe=b/name
   opt=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if san else ['-O3']
   cmd=[cc,*opt,*flags,*incs,'-std=gnu11',f'-DREVIEW_CORE="{b}/pinned_leaves.inc"',f'-DREVIEW_RANS="{b}/rans_singleton.inc"',Path(__file__).with_name('compat_probe.c'),*objs,'-Wl,--gc-sections','-lm','-lpthread','-o',exe]
   for args,log in [(cmd,name+'-build.log'),([exe,'test'],name+'-test.json')]:
    r=run(args,log);commands.append(r);save()
    if r['returncode']:raise RuntimeError(f'Check failed: {log}')
  if blob((n/'src/kolmo.c').read_bytes())!=CORE or blob((n/'src/kgraph.inc').read_bytes())!=GRAPH:raise RuntimeError('Source changed')
  state['status']='passed';state['binaries']={k:hashlib.sha256((b/k).read_bytes()).hexdigest() for k in ['compat-release','compat-asan']};save()
 except BaseException as exc:state['status']='failed_or_interrupted';state['error']=str(exc);save();raise
if __name__=='__main__':main()
