"""Compare compiler output for a named public wrapper of each exact source on x86-64 SSE2."""
import argparse,hashlib,json,subprocess,tempfile
from pathlib import Path
ap=argparse.ArgumentParser()
for k in ['source','build','out']:ap.add_argument('--'+k,type=Path,required=True)
a=ap.parse_args()
with tempfile.TemporaryDirectory(dir=a.out) as td:
 d=Path(td);c=d/'wrap.c'
 c.write_text('#include <stdint.h>\n#include <stddef.h>\n#include <emmintrin.h>\n#include TRANSFORM\nvoid forward(uint8_t *b,size_t n){e8e9_fwd(b,n);}\nvoid inverse(uint8_t *b,size_t n){e8e9_inv(b,n);}\n')
 asm={}
 for label,src in [('baseline',a.source),('candidate',a.build)]:
  out=d/(label+'.s')
  subprocess.run(['cc','-O3','-arch','x86_64','-msse2','-S','-fno-asynchronous-unwind-tables','-DTRANSFORM="'+str(src/'src/transforms/x86.c')+'"',c,'-o',out],check=True)
  asm[label]=out.read_bytes()
 if asm['baseline']!=asm['candidate']:
  import difflib
  print(''.join(list(difflib.unified_diff(asm['baseline'].decode().splitlines(True),asm['candidate'].decode().splitlines(True)))[:70]))
 assert asm['baseline']==asm['candidate']
 obj={'target':'x86_64-apple-darwin, SSE2','compile':'Apple clang -O3 -arch x86_64 -msse2 -S -fno-asynchronous-unwind-tables','bytes':len(asm['baseline']),'both_assembly_sha256':hashlib.sha256(asm['baseline']).hexdigest(),'identical':True}
 (a.out/'x86-codegen.json').write_text(json.dumps(obj,indent=2)+'\n')
