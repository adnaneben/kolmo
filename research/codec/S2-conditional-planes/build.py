#!/usr/bin/env python3
"""Build S2 using frozen S1 code; never edit production or historical research."""
import argparse, hashlib, json, pathlib, subprocess, re
PIN='4c78ea9893a7882bc4d427cca29076623312d66a'
S1_SHA='4edf6d096c81b651e073080a77c3b497275b2b8c'
def blob(data):return hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
def stage_baseline(text):
    marker='\nint main('
    if text.count(marker)!=1:raise ValueError('S1 main boundary')
    text=text.split(marker)[0]
    split=text.index('using V=')
    head,body=text[:split],text[split:]
    for a,b in [('s_compress(','p_compress('),('s_forward(','p_forward('),('s_type(','p_type('),('s_crc(','p_crc(')]:body=body.replace(a,b)
    helpers=r'''
static bool profiling=false;
struct ProfileCounts { uint64_t type=0,forward=0,codec=0,crc=0,calls=0; };
static ProfileCounts pc;
static uint64_t pn(){timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return uint64_t(t.tv_sec)*1000000000ULL+t.tv_nsec;}
static int p_type(const unsigned char*p,size_t n,int*w,size_t*h){if(!profiling)return s_type(p,n,w,h);auto t=pn();int r=s_type(p,n,w,h);pc.type+=pn()-t;return r;}
static int p_forward(const unsigned char*p,size_t n,int w,size_t h,SProg*out){if(!profiling)return s_forward(p,n,w,h,out);auto t=pn();int r=s_forward(p,n,w,h,out);pc.forward+=pn()-t;return r;}
static SBuf p_compress(int c,const unsigned char*p,size_t n){if(!profiling)return s_compress(c,p,n);auto t=pn();auto r=s_compress(c,p,n);pc.codec+=pn()-t;pc.calls++;return r;}
static uint32_t p_crc(const unsigned char*p,size_t n){if(!profiling)return s_crc(p,n);auto t=pn();auto r=s_crc(p,n);pc.crc+=pn()-t;return r;}
'''
    return head+helpers+body

def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--bridge-build',type=pathlib.Path);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    repo=a.repo.resolve();out=a.output.resolve();out.mkdir(exist_ok=False);src=pathlib.Path(__file__).resolve().parent;s1=repo/'research/codec/S1-stack-order'
    data=(s1/'stack.cpp').read_bytes()
    if blob(data)!=S1_SHA:raise ValueError('S1 source identity')
    paths=['src','vendor','research/codec/P2-E9','research/codec/P2-E11','research/codec/P2-E13','research/codec/E02C-harness']
    names=subprocess.check_output(['git','-C',str(repo),'ls-tree','-r','--name-only',PIN,'--',*paths],text=True).splitlines();receipt={}
    for name in names:
        if pathlib.Path(name).suffix not in ('.c','.h','.inc','.hpp','.cpp','.S'):continue
        data=(repo/name).read_bytes();expected=subprocess.check_output(['git','-C',str(repo),'show',PIN+':'+name])
        if data!=expected:raise ValueError('changed frozen source '+name)
        receipt[name]=hashlib.sha256(data).hexdigest()
    (out/'baseline.inc').write_text(stage_baseline((s1/'stack.cpp').read_text()))
    flags=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if a.sanitize else ['-O3','-DNDEBUG','-fno-plt']
    flags+=['-ffp-contract=off'];cc='clang' if a.sanitize else 'gcc';cxx='clang++' if a.sanitize else 'g++';commands=[]
    if a.bridge_build:
        rb=json.loads((a.bridge_build/'SOURCE_RECEIPT.json').read_text())
        if rb['sanitized']!=a.sanitize or rb['reference']!=PIN:raise ValueError('bridge regime mismatch')
        for n,h in receipt.items():
            if rb['sha256'][n]!=h:raise ValueError('bridge source mismatch '+n)
        bridge=a.bridge_build.resolve()/'bridge.o'
    else:
        e=repo/'research/codec/P2-E11/p2e11.c';text=e.read_text();marker='\nint main('
        if text.count(marker)!=1:raise ValueError('research main boundary')
        text=text.split(marker)[0];text=re.sub(r'#include\s+"([^"\n]+)"',lambda m:'#include '+json.dumps(str((e.parent/m.group(1)).resolve())),text)
        (out/'frozen.inc').write_text(text)
        defs=['-DXXH_NAMESPACE=ZSTD_','-DZSTD_MULTITHREAD','-DZSTD_LEGACY_SUPPORT=0']
        inc=['-I'+str(repo/x) for x in ['src','vendor/zstd','vendor/lz4','vendor/libbsc','vendor/brotli/include','research/codec/P2-E13']]+['-I'+str(out)]
        bridge=out/'bridge.o';commands.append([cc,*flags,*defs,*inc,'-c',str(s1/'bridge.c'),'-o',str(bridge)])
    commands.append([cxx,*flags,'-std=c++17','-I'+str(out),str(src/'conditional.cpp'),str(bridge),str(repo/'build/libvendor.a'),'-o',str(out/'s2'),'-llzma','-lpthread','-lm','-lstdc++'])
    for cmd in commands:subprocess.run(cmd,check=True,timeout=240)
    receipt.update({str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in [src/'conditional.cpp',src/'build.py',s1/'stack.cpp',s1/'bridge.c',out/'s2',out/'baseline.inc',bridge]})
    (out/'SOURCE_RECEIPT.json').write_text(json.dumps({'reference':PIN,'sanitized':a.sanitize,'vendor_sanitized':False,'sha256':receipt,'commands':commands},indent=2)+'\n')
if __name__=='__main__':main()
