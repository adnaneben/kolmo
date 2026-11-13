#!/usr/bin/env python3
"""Build R2-stack against pinned source: no production-file edits.
Vendor objects are uninstrumented; exact leaf/inverse functions and harness
are instrumented by the Clang ASan/UBSan build. Requires Linux.
"""
from __future__ import annotations
import argparse, concurrent.futures, hashlib, json, os, platform, subprocess
from pathlib import Path
CORE='e06467c8872d494b97270811bedf6fd0bac2aa31'
GRAPH='91ad067dc583b9d9e6f060f214a0f7c402bb4a31'
def blob(b): return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
def sha(b): return hashlib.sha256(b).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 root=a.source.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
 core=(root/'src/kolmo.c').read_bytes();graph=(root/'src/kgraph.inc').read_bytes()
 if blob(core)!=CORE or blob(graph)!=GRAPH:raise ValueError('unreviewed source')
 c=core.decode();g=graph.decode();begin=c.index('static int kgu_get(');end=c.index('static size_t codec_compress(',begin)
 leaves='#include <math.h>\n#include "common/mem.h"\n'+c[begin:end]
 (out/'leaves.inc').write_text(leaves)
 begin=c.index('static int rans_norm(');end=c.index('/* rans64',begin);norm=c[begin:end]
 if norm.count('if (used < 2) return -1;')!=1:raise ValueError('singleton anchor not unique')
 norm=norm.replace('rans_norm(', 'review_rans_norm(').replace('if (used < 2) return -1;', 'if (used < 2) return used == 1 ? 0 : -1;')
 begin=c.index('static size_t rans_compress_all(');end=c.index('static inline uint32_t r64_rd(',begin)
 enc=c[begin:end].replace('rans_compress_all(', 'review_rans_compress_all(').replace('rans_norm(', 'review_rans_norm(')
 (out/'singleton.inc').write_text(norm+enc)
 # Exact functions, no reimplementation of the predictor inverse.
 a0=g.index('typedef struct { uint8_t *p; size_t n, cap; } kbuf;');a1=g.index('/* ---- DECIMAL ---- */',a0)
 b0=g.index('static int64_t kg_unzz(');b1=g.index('\n',b0)
 d0=g.index('static int kg_int_inv(');d1=g.index('/* ---- FLOAT ---- */',d0)
 inverse=g[a0:a1]+g[b0:b1]+'\n'+g[d0:d1];(out/'inverse.inc').write_text(inverse)
 commands=[];state=dict(status='building',scope='exact leaf and KG_INT inverse; residual-to-frame encode, not full production encoder',core_blob=CORE,graph_blob=GRAPH,generated_sha256={x:sha((out/x).read_bytes()) for x in ('leaves.inc','singleton.inc','inverse.inc')},platform=platform.platform(),commands=commands)
 def save(): (out/'receipt.json').write_text(json.dumps(state,indent=2)+'\n')
 def run(cmd,log):
  proc=subprocess.run([str(x) for x in cmd],capture_output=True,timeout=150,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
  data=proc.stdout+proc.stderr;(out/log).write_bytes(data)
  return dict(argv=list(map(str,cmd)),returncode=proc.returncode,log=log,log_sha256=sha(data))
 flags=['-ffunction-sections','-fdata-sections','-DXXH_NAMESPACE=ZSTD_','-DZSTD_MULTITHREAD','-DZSTD_LEGACY_SUPPORT=0','-DZSTD_DISABLE_ASM']
 includes=[f'-I{root/"vendor/zstd"}'];sources=[s for d in ('common','compress','decompress') for s in sorted((root/'vendor/zstd'/d).glob('*.c'))]
 def compile_vendor(item):
  i,s=item;o=out/f'vendor-{i}.o';r=run(['gcc','-O2',*flags,*includes,'-c',s,'-o',o],f'vendor-{i}.log');r.update(object=str(o),source_sha256=sha(s.read_bytes()));return r
 try:
  with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:commands.extend(pool.map(compile_vendor,enumerate(sources)))
  save()
  if any(x['returncode'] for x in commands):raise RuntimeError('vendor build')
  objects=[x['object'] for x in commands]
  here=Path(__file__).resolve().parent;review=here.parent/'review-20261112/compat_probe.c'
  if blob(review.read_bytes())!='89678abe25dad6ee61006bd921f50ca9b64e872f':raise ValueError('changed R1 selftest')
  state['research_files']={p.name:sha(p.read_bytes()) for p in here.iterdir() if p.is_file()}
  for cc,san in [('gcc',False),('clang',True)]:
   label='asan' if san else 'release';opt=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if san else ['-O3']
   exe=out/f'stack-{label}';cmd=[cc,*opt,*flags,*includes,'-std=gnu11',f'-DREVIEW_CORE="{out}/leaves.inc"',f'-DREVIEW_RANS="{out}/singleton.inc"',f'-DR2_INVERSE="{out}/inverse.inc"',f'-DR2_REVIEW_SOURCE="{review}"',here/'probe.c',*objects,'-Wl,--gc-sections','-lm','-lpthread','-o',exe]
   for args,log in [(cmd,label+'-build.log'),([exe,'test'],label+'-test.log')]:
    r=run(args,log);commands.append(r);save()
    if r['returncode']:raise RuntimeError('failed '+log)
   state[label+'_binary_sha256']=sha(exe.read_bytes());state[label+'_compiler']=subprocess.check_output([cc,'--version'],text=True)
  if blob((root/'src/kolmo.c').read_bytes())!=CORE or blob((root/'src/kgraph.inc').read_bytes())!=GRAPH:raise ValueError('source changed')
  state['status']='passed';save()
 except BaseException as e: state['status']='failed_or_interrupted';state['error']=str(e);save();raise
if __name__=='__main__':main()
