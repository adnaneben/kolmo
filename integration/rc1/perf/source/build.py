#!/usr/bin/env python3
"""Build the frozen matched RC1/release/competitor native pilot in a NEW directory.

Requires local complete checkouts; performs no fetch/push and changes no source files.
OpenZL may instead be a tar extraction accompanied by the pinned git ls-tree output.
"""
from __future__ import annotations
import argparse, hashlib, json, os, pathlib, re, subprocess
RC='ae3957ad80d134506ce37b59857af25fe8826062'
RELEASE='a1d26f2075120f2a48e72e7026754d1a17bb2e8f'
OPENZL='32246b48faee46807f84183dac4db479089f5445'
FLAGS=['-O3','-DNDEBUG','-fno-plt','-ffp-contract=off']
def run(cmd,**kw):return subprocess.run([str(x) for x in cmd],check=True,timeout=900,**kw)
def output(cmd):return subprocess.check_output([str(x) for x in cmd],text=True)
def check_files(repo,pin,roots):
    paths=output(['git','-C',repo,'ls-tree','-r','--name-only',pin,'--',*roots]).splitlines()
    if not paths:raise ValueError('empty frozen source scope')
    checks={}
    for name in paths:
        want=subprocess.check_output(['git','-C',str(repo),'show',pin+':'+name])
        have=(repo/name).read_bytes()
        if have!=want:raise ValueError('frozen source differs: '+name)
        checks[name]=hashlib.sha256(have).hexdigest()
    return checks

def check_openzl(root,tree):
    if tree is None:
        if output(['git','-C',root,'rev-parse','HEAD']).strip()!=OPENZL:raise ValueError('OpenZL pin mismatch')
        text=output(['git','-C',root,'ls-tree','-r',OPENZL])
    else:text=tree.read_text()
    count=0
    for line in text.splitlines():
        fields,name=line.split('\t',1);mode,kind,sha=fields.split()
        if kind!='blob':continue
        p=root/name
        b=os.readlink(p).encode() if mode=='120000' else p.read_bytes()
        got=hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()
        if got!=sha:raise ValueError('OpenZL blob differs: '+name)
        count+=1
    if count!=3674:raise ValueError('unexpected OpenZL source-tree coverage')
    return {'pin':OPENZL,'blobs_verified':count,'tree_sha256':hashlib.sha256(text.encode()).hexdigest()}

def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=pathlib.Path,required=True);p.add_argument('--release',type=pathlib.Path,required=True)
    p.add_argument('--openzl',type=pathlib.Path,required=True);p.add_argument('--openzl-tree',type=pathlib.Path)
    p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--stage-only',action='store_true');a=p.parse_args()
    rc,rel,oz=(x.resolve() for x in (a.repo,a.release,a.openzl));out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    receipt={'rc_pin':RC,'release_pin':RELEASE,'rc_files':check_files(rc,RC,['src','vendor','Makefile']),
             'release_files':check_files(rel,RELEASE,['src','vendor','Makefile']), 'openzl':check_openzl(oz,a.openzl_tree)}
    source=pathlib.Path(__file__).resolve().parent
    # Copy the already tested driver, not a newly synthesized codec implementation.
    (out/'driver.cpp').write_bytes((source/'driver.cpp').read_bytes())
    adapter=(rc/'research/codec/N1-native-exact/adapter.cpp').read_text()
    if adapter.count('\nint main(')!=1:raise ValueError('adapter entry boundary changed')
    (out/'openzl_prefix.inc').write_text(adapter.split('\nint main(')[0])
    (out/'header_parser.hpp').write_bytes((rc/'research/codec/N1-native-exact/header_parser.hpp').read_bytes())
    cmake='''cmake_minimum_required(VERSION 3.20.2)
project(rc1_pilot LANGUAGES C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
foreach(o BUILD_ALL BUILD_TESTS BUILD_BENCHMARKS BUILD_PARQUET_TOOLS BUILD_PYTHON_EXT BUILD_PYTHON_DEMO BUILD_TOOLS BUILD_CLI BUILD_EXAMPLES INSTALL CPP_INSTALL ALLOW_INTROSPECTION)
 set(OPENZL_${o} OFF CACHE BOOL "" FORCE)
endforeach()
set(OPENZL_BUILD_CPP ON CACHE BOOL "" FORCE)
set(OPENZL_BUILD_CUSTOM_PARSERS ON CACHE BOOL "" FORCE)
add_subdirectory("%s" openzl-build EXCLUDE_FROM_ALL)
add_executable(rc1_perf driver.cpp release.o czn_xxh3.o)
target_include_directories(rc1_perf PRIVATE . "%s" "%s/src" "%s/src" "%s/vendor/zstd" "%s/vendor/lz4" "%s/vendor/brotli/include")
target_link_libraries(rc1_perf PRIVATE csv_parser openzl "%s/build/libkolmo.a" "%s/build/libvendor.a" lzma pthread m stdc++)
'''%(oz,oz,rel,rc,rc,rc,rc,rc,rc)
    (out/'CMakeLists.txt').write_text(cmake)
    if a.stage_only:
        receipt['status']='STAGED_NOT_BUILT';(out/'BUILD_RECEIPT.json').write_text(json.dumps(receipt,indent=2));return
    run(['make','-C',rc,'-j2','ARCH=','CFLAGS='+' '.join(FLAGS),'build/libkolmo.a','build/libvendor.a'])
    run(['make','-C',rel,'-j2','ARCH=','CFLAGS='+' '.join(FLAGS),'build/czn.o'])
    (out/'release.o').write_bytes((rel/'build/czn.o').read_bytes())
    def symbols(path):return {l.split()[-1] for l in output(['nm','-g','--defined-only',path]).splitlines() if len(l.split())>=3}
    overlap=sorted(symbols(rc/'build/kolmo.o')&symbols(out/'release.o'))
    if any(s.startswith(('czn_','kolmo_')) for s in overlap):raise ValueError('API symbol collision')
    cmd=['objcopy']
    for s in overlap:cmd+=['--redefine-sym',s+'=rc1_release_'+s]
    run(cmd+[out/'release.o'])
    run(['gcc',*FLAGS,'-DXXH_NAMESPACE=ZSTD_','-c',rel/'vendor/xxhash/czn_xxh3.c','-o',out/'czn_xxh3.o'])
    run(['cmake','-S',out,'-B',out/'build','-DCMAKE_BUILD_TYPE=Release','-DCMAKE_C_FLAGS_RELEASE='+' '.join(FLAGS),'-DCMAKE_CXX_FLAGS_RELEASE='+' '.join(FLAGS)])
    run(['cmake','--build',out/'build','--target','rc1_perf','--parallel','2'])
    receipt.update(status='BUILT',release_helpers_renamed=overlap,binary_sha256=hashlib.sha256((out/'build/rc1_perf').read_bytes()).hexdigest())
    (out/'BUILD_RECEIPT.json').write_text(json.dumps(receipt,indent=2)+'\n')
if __name__=='__main__':main()
