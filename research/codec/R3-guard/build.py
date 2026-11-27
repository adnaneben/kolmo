#!/usr/bin/env python3
"""Build R3 in an isolated output directory; production and earlier research untouched."""
from __future__ import annotations
import argparse,concurrent.futures,difflib,hashlib,json,os,platform,subprocess
from pathlib import Path
CORE='e06467c8872d494b97270811bedf6fd0bac2aa31'
GRAPH='91ad067dc583b9d9e6f060f214a0f7c402bb4a31'
def sha(b):return hashlib.sha256(b).hexdigest()
def blob(b):return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
def replace_once(s,a,b):
 if s.count(a)!=1:raise ValueError('nonunique source anchor: '+a[:100])
 return s.replace(a,b,1)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 root=a.source.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);here=Path(__file__).resolve().parent
 core=(root/'src/kolmo.c').read_bytes();graph=(root/'src/kgraph.inc').read_bytes()
 if blob(core)!=CORE or blob(graph)!=GRAPH:raise ValueError('production source identity')
 c=core.decode();g=graph.decode();leaves=c[c.index('static int kgu_get('):c.index('static size_t codec_compress(')]
 hstart=leaves.index('static size_t huf_compress_all(');hend=leaves.index('static int huf_decompress_all(',hstart)
 h=replace_once(leaves[hstart:hend],'for (size_t i = 0; i < n;) {','for (size_t i = 0; i < n;) { r3_huf_chunks++;')
 leaves=leaves[:hstart]+h+leaves[hend:]
 rstart=leaves.index('static size_t rans_compress_all(');rend=leaves.index('static inline uint32_t r64_rd(',rstart)
 r=replace_once(leaves[rstart:rend],'for (size_t i = 0; i < n;) {','for (size_t i = 0; i < n;) { r3_rans_chunks++;')
 leaves=leaves[:rstart]+r+leaves[rend:]
 (out/'leaves.inc').write_text('#include <math.h>\n#include "common/mem.h"\n'+leaves)
 n0=c.index('static int rans_norm(');n1=c.index('/* rans64',n0)
 norm=c[n0:n1].replace('rans_norm(', 'review_rans_norm(')
 norm=replace_once(norm,'if (used < 2) return -1;','if (used < 2) { if (used == 1) r3_singletons++; return used == 1 ? 0 : -1; }')
 enc=r.replace('rans_compress_all(', 'review_rans_compress_all(').replace('rans_norm(', 'review_rans_norm(')
 (out/'singleton.inc').write_text(norm+enc)
 shared=enc.replace('review_rans_compress_all(', 'shared_rans_compress_all(')
 shared=replace_once(shared,'uint8_t *out, size_t cap) {','uint8_t *out, size_t cap, uint32_t *tmp, size_t cw) {')
 shared=replace_once(shared,'size_t o = 0, cw = RANS_CHUNK + 64; uint32_t *tmp = malloc(cw * 4); if (!tmp) return 0;', 'size_t o = 0; if (!tmp || cw < (n < RANS_CHUNK ? n : RANS_CHUNK) + 64) return 0;')
 shared=shared.replace('free(tmp);','')
 (out/'shared.inc').write_text(shared)
 (out/'shared.diff').write_text(''.join(difflib.unified_diff(enc.splitlines(True),shared.splitlines(True),fromfile='singleton_encoder',tofile='shared_scratch_encoder')))
 a0=g.index('typedef struct { uint8_t *p; size_t n, cap; } kbuf;');a1=g.index('/* ---- DECIMAL ---- */',a0)
 b0=g.index('static int64_t kg_unzz(');b1=g.index('\n',b0);d0=g.index('static int kg_int_inv(');d1=g.index('/* ---- FLOAT ---- */',d0)
 (out/'inverse.inc').write_text(g[a0:a1]+g[b0:b1]+'\n'+g[d0:d1])
 old=here.parent/'R2-stack/probe.c';r1=here.parent/'review-20261112/compat_probe.c'
 if blob(old.read_bytes())!='cdfb42368567f7f63a1c45c63e72d1fb7f88d63b' or blob(r1.read_bytes())!='89678abe25dad6ee61006bd921f50ca9b64e872f':raise ValueError('frozen ancestor changed')
 (out/'r2.inc').write_text(replace_once(old.read_text(),'int main(int argc,char**argv){','int r2_unused_main(int argc,char**argv){'))
 # Complete production numeric algorithm; final serializer interception only.
 begin=g.index('static size_t kg_encode_num(');end=g.index('/* Float array:',begin)
 body=replace_once(g[begin:end],'out = kg_serialize(x, codec, level, &g, n, dst, cap);','out = r3_capture_num(&g, &par);')
 capture_graph=g[:begin]+'static size_t r3_capture_num(const kgb *, const kbuf *);\n'+body+g[end:]
 (out/'capture_graph.inc').write_text(capture_graph)
 (out/'capture_core.inc').write_text(replace_once(c,'#include "kgraph.inc"','#include "'+str(out/'capture_graph.inc')+'"'))
 commands=[];state={'status':'building','core_blob':CORE,'graph_blob':GRAPH,'research_files':{q.name:sha(q.read_bytes()) for q in here.iterdir() if q.is_file()},'generated':{q.name:sha(q.read_bytes()) for q in out.glob('*.inc')},'commands':commands,'platform':platform.platform(),'instrumentation':'tracked requested heap only; vendor uninstrumented; exact algorithm bodies with chunk counters; scratch-only rANS clone; original readers and inverse'}
 def save():(out/'receipt.json').write_text(json.dumps(state,indent=2)+'\n')
 def run(args,log):
  args=list(map(str,args));proc=subprocess.run(args,capture_output=True,timeout=240,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
  data=proc.stdout+proc.stderr;(out/log).write_bytes(data);return {'argv':args,'returncode':proc.returncode,'log':log,'sha256':sha(data)}
 flags=['-ffunction-sections','-fdata-sections','-DXXH_NAMESPACE=ZSTD_','-DZSTD_MULTITHREAD','-DZSTD_LEGACY_SUPPORT=0','-DZSTD_DISABLE_ASM'];includes=[f'-I{root/"vendor/zstd"}']
 sources=[q for d in ('common','compress','decompress') for q in sorted((root/'vendor/zstd'/d).glob('*.c'))]
 def vendor(item):
  i,q=item;r=run(['gcc','-O2',*flags,*includes,'-c',q,'-o',out/f'vendor-{i}.o'],f'vendor-{i}.log');r['object']=str(out/f'vendor-{i}.o');r['source_sha256']=sha(q.read_bytes());return r
 try:
  with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:commands.extend(pool.map(vendor,enumerate(sources)))
  save()
  if any(q['returncode'] for q in commands):raise RuntimeError('vendor compile failed')
  objects=[q['object'] for q in commands]
  for cc,sanitize in [('gcc',False),('clang',True)]:
   name='asan' if sanitize else 'release';opt=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if sanitize else ['-O3']
   defs=[f'-DREVIEW_CORE="{out}/leaves.inc"',f'-DREVIEW_RANS="{out}/singleton.inc"',f'-DR2_INVERSE="{out}/inverse.inc"',f'-DR2_REVIEW_SOURCE="{r1}"',f'-DR3_R2_SOURCE="{out}/r2.inc"',f'-DR3_SHARED_RANS="{out}/shared.inc"']
   exe=out/f'probe-{name}';cmd=[cc,*opt,*flags,*includes,'-std=gnu11','-ffp-contract=off',*defs,here/'probe.c',*objects,'-Wl,--gc-sections','-lm','-lpthread','-o',exe]
   for argv,log in [(cmd,name+'-build.log'),([exe,'test'],name+'-test.log')]:
    r=run(argv,log);commands.append(r);save()
    if r['returncode']:raise RuntimeError(log+' failed')
   state[name+'_binary_sha256']=sha(exe.read_bytes());state[name+'_compiler']=subprocess.check_output([cc,'--version'],text=True)
  inc=[f'-I{root/"src"}',*includes,f'-I{root/"vendor/lz4"}',f'-I{root/"vendor/libbsc"}',f'-I{root/"vendor/brotli/include"}']
  # Unreachable unrelated codecs are garbage-collected; numeric algorithm is complete.
  cmd=['gcc','-O0','-ffp-contract=off',*flags,*inc,'-std=gnu11',f'-DR3_CAPTURE_CORE="{out}/capture_core.inc"',here/'capture.c','-Wl,--gc-sections','-lm','-lpthread','-o',out/'capture']
  r=run(cmd,'capture-build.log');commands.append(r);save()
  if r['returncode']:raise RuntimeError('capture compile failed')
  state['capture_sha256']=sha((out/'capture').read_bytes())
  if blob((root/'src/kolmo.c').read_bytes())!=CORE or blob((root/'src/kgraph.inc').read_bytes())!=GRAPH:raise ValueError('source changed during build')
  state['status']='passed';save()
 except BaseException as e:state.update(status='failed_or_interrupted',error=str(e));save();raise
if __name__=='__main__':main()
