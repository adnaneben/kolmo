#!/usr/bin/env python3
"""Stage exact prefixes from pinned source; do not modify any production files."""
import argparse, hashlib, json, pathlib, re, subprocess
REFERENCE='4c78ea9893a7882bc4d427cca29076623312d66a'
OPENZL='32246b48faee46807f84183dac4db479089f5445'
FILES=['research/codec/P2-E11/p2e11.c','research/codec/P2-E13/p2e13.c',
       'research/codec/P2-E13/rans1.inc','research/codec/P2-E11/tpframe.inc',
       'research/codec/P2-E9/fastint9.inc','research/codec/E02C-harness/harness/e02c.c',
       'research/codec/N1-native-exact/adapter.cpp','research/codec/N1-native-exact/header_parser.hpp',
       'src/kolmo.c','src/kgraph.inc']
def prefix(text, marker):
    if text.count(marker)!=1: raise ValueError('nonunique prefix boundary: '+marker)
    return text.split(marker)[0]
def absolute_includes(text, directory, overrides=None):
    overrides=overrides or {}
    def change(m):
        # OpenZL public headers are resolved by the compiler's include search
        # path, not relative to the file containing the directive.
        if m.group(1).startswith('openzl/') and m.group(1) not in overrides:
            return m.group(0)
        target=overrides.get(m.group(1), (directory/m.group(1)).resolve())
        return '#include '+json.dumps(str(target))
    return re.sub(r'#include\s+"([^"\n]+)"',change,text)
def main():
    p=argparse.ArgumentParser();p.add_argument('--repo',type=pathlib.Path,required=True);p.add_argument('--openzl',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True)
    a=p.parse_args();repo=a.repo.resolve();oz=a.openzl.resolve();out=a.output.resolve();out.mkdir(exist_ok=False)
    run=lambda cmd:subprocess.run(cmd,check=True,text=True)
    receipts={}
    source_dirs=['src','vendor','research/codec/P2-E9','research/codec/P2-E11','research/codec/P2-E13','research/codec/E02C-harness','research/codec/N1-native-exact']
    paths=subprocess.check_output(['git','-C',str(repo),'ls-tree','-r','--name-only',REFERENCE,'--',*source_dirs],text=True).splitlines()
    dependencies=sorted(set(FILES+[name for name in paths if pathlib.Path(name).suffix in ('.c','.h','.hpp','.inc','.cpp','.S')]))
    for name in dependencies:
        expected=subprocess.check_output(['git','-C',str(repo),'show',REFERENCE+':'+name])
        actual=(repo/name).read_bytes()
        if actual!=expected:raise ValueError('source changed: '+name)
        receipts[name]=hashlib.sha256(actual).hexdigest()
    if subprocess.check_output(['git','-C',str(oz),'rev-parse','HEAD'],text=True).strip()!=OPENZL:raise ValueError('OpenZL pin mismatch')
    run(['git','-C',str(oz),'diff','--exit-code','HEAD'])
    e11=repo/FILES[0];e13=repo/FILES[1];o=repo/'research/codec/N1-native-exact/adapter.cpp'
    (out/'e11.inc').write_text(absolute_includes(prefix(e11.read_text(),'\nint main('),e11.parent))
    (out/'e13.inc').write_text(absolute_includes(prefix(e13.read_text(),'\nstatic int p2e13_main(int argc, char **argv);'),e13.parent,{'../P2-E11/p2e11.c':out/'e11.inc'}))
    (out/'openzl_prefix.inc').write_text(absolute_includes(prefix(o.read_text(),'\nint main('),o.parent))
    (out/'bridge.c').write_text('''#include "e13.inc"
struct KBuf { unsigned char *data; size_t size; };
struct KBuf n2_kenc(const unsigned char*s,size_t n,int arm){
 for(int i=0;i<NC;i++)CONF[i]=-1;
 struct KBuf b={0};if(arm<0||arm>=N13)return b;b.size=enc13(s,n,arm,&b.data);return b;
}
struct KBuf n2_kdec(const unsigned char*s,size_t z,size_t n,int arm){
 struct KBuf b={0};if(arm<0||arm>=N13||n>16u*1024u*1024u)return b;
 b.data=malloc(n+64);if(!b.data)return b;
 if(dec13(s,z,arm,b.data,n)){free(b.data);b.data=NULL;return b;}b.size=n;return b;
}
''')
    flags='-O3 -DNDEBUG -fno-plt -ffp-contract=off'
    run(['make','-C',str(repo),'-j2','build/libvendor.a','CFLAGS='+flags])
    defs=['-DXXH_NAMESPACE=ZSTD_','-DZSTD_MULTITHREAD','-DZSTD_LEGACY_SUPPORT=0']
    includes=[str(repo/x) for x in ['src','vendor/zstd','vendor/lz4','vendor/libbsc','vendor/brotli/include']]
    run(['gcc',*flags.split(),*defs,*['-I'+x for x in includes],'-c',str(out/'bridge.c'),'-o',str(out/'bridge.o')])
    src=pathlib.Path(__file__).resolve().parent
    cmake='''cmake_minimum_required(VERSION 3.20.2)
project(n2_matched LANGUAGES C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
foreach(o BUILD_ALL BUILD_TESTS BUILD_BENCHMARKS BUILD_PARQUET_TOOLS BUILD_PYTHON_EXT BUILD_PYTHON_DEMO BUILD_CPP BUILD_CUSTOM_PARSERS BUILD_TOOLS BUILD_CLI BUILD_EXAMPLES INSTALL CPP_INSTALL ALLOW_INTROSPECTION)
 set(OPENZL_${o} OFF CACHE BOOL "" FORCE)
endforeach()
add_subdirectory("%s" openzl-build EXCLUDE_FROM_ALL)
add_executable(n2_native "%s" "%s")
target_include_directories(n2_native PRIVATE "%s" "%s")
target_link_libraries(n2_native PRIVATE openzl "%s" lzma pthread m stdc++)
'''%(oz,src/'native.cpp',out/'bridge.o',out,oz,repo/'build/libvendor.a')
    (out/'CMakeLists.txt').write_text(cmake)
    run(['cmake','-S',str(out),'-B',str(out/'cmake'),'-DCMAKE_BUILD_TYPE=Release','-DCMAKE_C_FLAGS_RELEASE='+flags,'-DCMAKE_CXX_FLAGS_RELEASE='+flags,'-DZSTD_BUILD_SHARED=OFF'])
    run(['cmake','--build',str(out/'cmake'),'--target','n2_native','--parallel','2'])
    binary=out/'cmake/n2_native'
    receipts.update({str(x.name):hashlib.sha256(x.read_bytes()).hexdigest() for x in [out/'bridge.c',out/'e11.inc',out/'e13.inc',out/'openzl_prefix.inc',src/'native.cpp',binary]})
    (out/'SOURCE_RECEIPT.json').write_text(json.dumps({'reference':REFERENCE,'openzl':OPENZL,'sha256':receipts},indent=2)+'\n')
if __name__=='__main__':main()
