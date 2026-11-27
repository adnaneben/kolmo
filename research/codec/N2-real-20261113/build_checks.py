"""Build instrumented real adapters and separate unmodified readers, without editing codecs."""
import argparse,pathlib,shlex,subprocess,json,hashlib,os
p=argparse.ArgumentParser();p.add_argument('--root',type=pathlib.Path,required=True);a=p.parse_args();r=a.root.resolve();repo=r/'source/kolmo';oz=r/'source/openzl';build=r/'build-release2';cm=build/'cmake';out=r/'build-checks';out.mkdir(exist_ok=False)
commands=[]
def run(cmd,cwd=None):
 commands.append(cmd);print(shlex.join(cmd),flush=True);subprocess.run(cmd,cwd=cwd,check=True,timeout=240)
incs=[repo/x for x in ['src','vendor/zstd','vendor/lz4','vendor/libbsc','vendor/brotli/include']]
defs=['-DXXH_NAMESPACE=ZSTD_','-DZSTD_MULTITHREAD','-DZSTD_LEGACY_SUPPORT=0']
flags=['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-sanitize-recover=undefined','-ffp-contract=off']
run(['clang',*flags,*defs,*['-I'+str(x) for x in incs],'-c',str(build/'bridge.c'),'-o',str(out/'bridge.o')])
ff=(cm/'CMakeFiles/n2_native.dir/flags.make').read_text();includes=shlex.split(next(x.split('=',1)[1] for x in ff.splitlines() if x.startswith('CXX_INCLUDES =')))
libs=[str(cm/'openzl-build/libopenzl.a'),str(repo/'build/libvendor.a'),'-llzma','-lpthread','-lm','-lstdc++',str(cm/'openzl-build/zstd_build/lib/libzstd.a'),str(cm/'openzl-build/lz4_build/liblz4.a'),'-lm']
run(['clang++',*flags,'-std=c++17',*includes,str(repo/'research/codec/N2-matched-native/native.cpp'),str(out/'bridge.o'),*libs,'-o',str(out/'n2_asan')])
run(['make','-C',str(repo),'-j2','build/kolmo','CFLAGS=-O3 -DNDEBUG -fno-plt -ffp-contract=off'])
run(['gcc','-O3',*includes,str(repo/'research/codec/N1-native-exact/stock_reader.c'),str(cm/'openzl-build/libopenzl.a'),str(cm/'openzl-build/zstd_build/lib/libzstd.a'),str(cm/'openzl-build/lz4_build/liblz4.a'),'-lm','-lpthread','-o',str(out/'stock_reader')])
(out/'zstd_reader.c').write_text('''#include <zstd.h>\n#include <stdio.h>\n#include <stdlib.h>\nint main(int c,char**v){if(c!=3)return 2;FILE*f=fopen(v[1],"rb");if(!f)return 2;fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);if(n<0||n>32*1024*1024)return 2;void*s=malloc(n?n:1);if(fread(s,1,n,f)!=(size_t)n)return 2;fclose(f);unsigned long long z=ZSTD_getFrameContentSize(s,n);if(z>16*1024*1024)return 2;void*d=malloc(z?z:1);size_t k=ZSTD_decompress(d,z,s,n);free(s);if(ZSTD_isError(k)||k!=z){free(d);return 2;}f=fopen(v[2],"wbx");if(!f){free(d);return 2;}int ok=fwrite(d,1,z,f)==z;free(d);if(fclose(f))ok=0;return ok?0:2;}\n''')
run(['gcc','-O3','-I'+str(repo/'vendor/zstd'),str(out/'zstd_reader.c'),str(repo/'build/libvendor.a'),'-lpthread','-o',str(out/'zstd_reader')])
receipt={'commands':commands,'instrumented':'C++ driver/header adapter and full Kolmo core/research bridge','uninstrumented':'OpenZL and vendor static libraries','binary_sha256':{x.name:hashlib.sha256(x.read_bytes()).hexdigest() for x in [out/'n2_asan',out/'stock_reader',out/'zstd_reader',repo/'build/kolmo']}}
(out/'BUILD_CHECKS.json').write_text(json.dumps(receipt,indent=2)+'\n')
