#!/usr/bin/env python3
"""Stage the frozen full-source codecs without modifying production files."""
import argparse, hashlib, json, pathlib, re, subprocess
PIN='4c78ea9893a7882bc4d427cca29076623312d66a'
def main():
 p=argparse.ArgumentParser();p.add_argument('--repo',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
 repo=a.repo.resolve();out=a.output.resolve();out.mkdir(exist_ok=False)
 paths=['src','vendor','research/codec/P2-E9','research/codec/P2-E11','research/codec/P2-E13','research/codec/E02C-harness']
 names=subprocess.check_output(['git','-C',str(repo),'ls-tree','-r','--name-only',PIN,'--',*paths],text=True).splitlines();receipt={}
 for name in names:
  if pathlib.Path(name).suffix not in ('.c','.h','.inc','.hpp','.cpp','.S'):continue
  data=(repo/name).read_bytes();expected=subprocess.check_output(['git','-C',str(repo),'show',PIN+':'+name])
  if data!=expected:raise ValueError('source differs: '+name)
  receipt[name]=hashlib.sha256(data).hexdigest()
 e=repo/'research/codec/P2-E11/p2e11.c';text=e.read_text();marker='\nint main('
 if text.count(marker)!=1:raise ValueError('ambiguous main')
 text=text.split(marker)[0]
 text=re.sub(r'#include\s+"([^"\n]+)"',lambda m:'#include '+json.dumps(str((e.parent/m.group(1)).resolve())),text)
 (out/'frozen.inc').write_text(text)
 src=pathlib.Path(__file__).resolve().parent
 flags=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=all'] if a.sanitize else ['-O3','-DNDEBUG','-fno-plt']
 flags+=['-ffp-contract=off'];cc='clang' if a.sanitize else 'gcc';cxx='clang++' if a.sanitize else 'g++'
 defs=['-DXXH_NAMESPACE=ZSTD_','-DZSTD_MULTITHREAD','-DZSTD_LEGACY_SUPPORT=0']
 inc=['-I'+str(repo/x) for x in ['src','vendor/zstd','vendor/lz4','vendor/libbsc','vendor/brotli/include','research/codec/P2-E13']]+['-I'+str(out)]
 commands=[[cc,*flags,*defs,*inc,'-c',str(src/'bridge.c'),'-o',str(out/'bridge.o')],
 [cxx,*flags,'-std=c++17',str(src/'stack.cpp'),str(out/'bridge.o'),str(repo/'build/libvendor.a'),'-o',str(out/'stack'),'-llzma','-lpthread','-lm','-lstdc++']]
 for cmd in commands:subprocess.run(cmd,check=True)
 receipt.update({p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [src/'bridge.c',src/'stack.cpp',out/'stack']})
 (out/'SOURCE_RECEIPT.json').write_text(json.dumps({'reference':PIN,'sanitized':a.sanitize,'vendor_sanitized':False,'sha256':receipt,'commands':commands},indent=2)+'\n')
if __name__=='__main__':main()
