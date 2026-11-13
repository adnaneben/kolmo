#!/usr/bin/env python3
"""Build an isolated full-source research adapter; never edit repository src/."""
import argparse, hashlib, json, os, pathlib, subprocess, sys, difflib
ROOT=pathlib.Path(__file__).resolve().parents[3]
PIN='2f3308a3380a140ad0f6873ae228de7715e5eef5'
CORE='e06467c8872d494b97270811bedf6fd0bac2aa31'
GRAPH='91ad067dc583b9d9e6f060f214a0f7c402bb4a31'
def blob(b): return hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
def stage(root,out):
    core=(root/'src/kolmo.c').read_bytes(); graph=(root/'src/kgraph.inc').read_bytes()
    if blob(core)!=CORE or blob(graph)!=GRAPH: raise RuntimeError('Unreviewed production source: full core/graph blob mismatch')
    s=graph.decode();a=s.index('static size_t kg_encode_num(');b=s.index('/* Float array:',a)
    body=s[a:b];old='out = kg_serialize(x, codec, level, &g, n, dst, cap);'
    if body.count(old)!=1: raise RuntimeError('Numeric serializer anchor is not unique')
    replacement='out = e02c_capture_enabled ? e02c_capture_num(&g, &par) : kg_serialize(x, codec, level, &g, n, dst, cap);'
    modified=s[:a]+'static size_t e02c_capture_num(const kgb *, const kbuf *);\n'+body.replace(old,replacement)+s[b:]
    out.mkdir(parents=True,exist_ok=True)
    gp=out/'e02c_graph.inc';gp.write_text(modified)
    c=core.decode();anchor='#include "kgraph.inc"'
    if c.count(anchor)!=1: raise RuntimeError('Core graph include not unique')
    cp=out/'e02c_core.inc';cp.write_text(c.replace(anchor,'#include "'+str(gp)+'"'))
    (out/'capture.diff').write_text(''.join(difflib.unified_diff(s.splitlines(True),modified.splitlines(True),fromfile='src/kgraph.inc',tofile='GENERATED/e02c_graph.inc')))
    meta={'validated_reference':PIN,'core_blob':blob(core),'graph_blob':blob(graph),'capture':'complete production source with a numeric-only serialization interception; prediction/GCD/coder bodies unchanged','generated_graph_sha256':hashlib.sha256(gp.read_bytes()).hexdigest(),'production_source_changed':False}
    return cp,meta
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--cc',default='gcc');p.add_argument('--sanitize',action='store_true');p.add_argument('--jobs',type=int,default=4);a=p.parse_args()
    if sys.platform!='linux':p.error('This measurement build is Linux-only; macOS/M5 must be measured separately')
    subprocess.run(['git','diff','--exit-code',PIN,'--','src','vendor','Makefile','benchmark/accept/TARGETS.md'],cwd=ROOT,check=True)
    out=a.output.resolve();cp,meta=stage(ROOT,out)
    subprocess.run(['make',f'-j{a.jobs}','ARCH=','build/libvendor.a','build/kolmo'],cwd=ROOT,check=True)
    flags=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'] if a.sanitize else ['-O3']
    cmd=[a.cc,*flags,'-std=gnu11','-ffp-contract=off','-DXXH_NAMESPACE=ZSTD_','-DZSTD_MULTITHREAD','-DZSTD_LEGACY_SUPPORT=0',f'-DE02C_CORE="{cp}"','-Isrc','-Ivendor/zstd','-Ivendor/lz4','-Ivendor/libbsc','-Ivendor/brotli/include',str(pathlib.Path(__file__).with_name('probe.c')),'build/libvendor.a','-llzma','-lpthread','-lm','-lstdc++','-o',str(out/'probe')]
    meta.update(command=cmd,compiler=subprocess.check_output([a.cc,'--version'],text=True),research_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),sanitized=a.sanitize)
    (out/'build.json').write_text(json.dumps(meta,indent=2)+'\n');subprocess.run(cmd,cwd=ROOT,check=True)
    subprocess.run([str(out/'probe'),'test'],cwd=ROOT,check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
    if blob((ROOT/'src/kolmo.c').read_bytes())!=CORE or blob((ROOT/'src/kgraph.inc').read_bytes())!=GRAPH:raise RuntimeError('Production source changed during research build')
