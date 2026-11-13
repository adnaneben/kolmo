import os,copy,importlib.util,json,pathlib,shutil,subprocess,tempfile,unittest,sys
sys.path.insert(0,str(pathlib.Path(__file__).parent))
import build,run
class Tools(unittest.TestCase):
 def rows(self):
  return [dict(arm=a,operation=o,round=r,batch=1,input_bytes=100,stored_bytes=80,wall_ns=1000,cpu_ns=900) for a in run.ARMS for o in ('encode','decode') for r in range(2)]
 def test_prefix(self): self.assertEqual(build.prefix('abc\nint main(x)','\nint main('),'abc')
 def test_prefix_missing(self):
  with self.assertRaises(ValueError):build.prefix('abc','missing')
 def test_prefix_duplicate(self):
  with self.assertRaises(ValueError):build.prefix('abcabc','abc')
 def test_absolute_include(self): self.assertEqual(build.absolute_includes('#include "x.h"',pathlib.Path('/tmp')), '#include "/tmp/x.h"')
 def test_public_include_kept(self):
  text='#include "openzl/zl_compress.h"'
  self.assertEqual(build.absolute_includes(text,pathlib.Path('/missing')),text)
 def test_public_override(self):
  self.assertEqual(build.absolute_includes('#include "openzl/zl_compress.h"',pathlib.Path('/missing'),{'openzl/zl_compress.h':pathlib.Path('/verified/header.h')}),'#include "/verified/header.h"')
 def test_override(self):self.assertEqual(build.absolute_includes('#include "x.h"',pathlib.Path('/tmp'),{'x.h':pathlib.Path('/other')}),'#include "/other"')
 def test_valid_rows(self):self.assertEqual(len(run.validate_rows(self.rows(),2)),48)
 def test_missing(self):
  with self.assertRaises(ValueError):run.validate_rows(self.rows()[:-1],2)
 def test_duplicate(self):
  r=self.rows();r.append(r[0]);
  with self.assertRaises(ValueError):run.validate_rows(r,2)
 def test_unknown_arm(self):
  r=self.rows();r[0]['arm']='oops'
  with self.assertRaises(ValueError):run.validate_rows(r,2)
 def test_nonfinite(self):
  r=self.rows();r[0]['wall_ns']=float('nan')
  with self.assertRaises(ValueError):run.validate_rows(r,2)
 def test_boolean(self):
  r=self.rows();r[0]['batch']=True
  with self.assertRaises(ValueError):run.validate_rows(r,2)
 def test_zero_wall(self):
  r=self.rows();r[0]['wall_ns']=0
  with self.assertRaises(ValueError):run.validate_rows(r,2)
 def test_batch_cap(self):
  r=self.rows();r[0]['batch']=17
  with self.assertRaises(ValueError):run.validate_rows(r,2)
 def test_frame_length_changed(self):
  r=self.rows();r[0]['stored_bytes']=81
  with self.assertRaises(ValueError):run.validate_rows(r,2)
 def test_input_changed(self):
  r=self.rows();r[0]['input_bytes']=101
  with self.assertRaises(ValueError):run.validate_rows(r,2)
 def test_policy_digest(self):
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d)/'x.py';p.write_text('')
   with self.assertRaises(ValueError):run.load_policy(p)
 @unittest.skipUnless(shutil.which('g++'),'C++ compiler required')
 def test_native_control_flow_only(self):
  # Deliberately labelled mock codecs: this tests batching, timers, validation,
  # output preservation and the bridge ABI, NOT compression performance.
  fake=r'''#include <vector>
#include <memory>
#include <stdexcept>
#include <string>
#include <fstream>
#include <cstdint>
#include <cstring>
using Bytes=std::vector<uint8_t>;
namespace exact {void need(bool x,const char*m){if(!x)throw std::runtime_error(m);}}
static size_t checked(size_t x){return x;}
static Bytes readfile(const char*p){std::ifstream f(p,std::ios::binary);return Bytes(std::istreambuf_iterator<char>(f),{});}
static void writefile(const char*p,const Bytes&b){std::ofstream f(p,std::ios::binary);f.write((const char*)b.data(),b.size());}
static Bytes encode(const Bytes&b,int,const std::string&){Bytes o=b;o.insert(o.begin(),0);return o;}
static size_t ZL_getDecompressedSize(const void*,size_t n){return n-1;}
static size_t ZL_getCompressedSize(const void*,size_t n){return n;}
static size_t ZL_decompress(void*d,size_t,const void*s,size_t n){memcpy(d,(const char*)s+1,n-1);return n-1;}
'''
  bridge=r'''#include <stdlib.h>
#include <string.h>
struct KBuf {unsigned char*data;size_t size;};
extern "C" KBuf n2_kenc(const unsigned char*s,size_t n,int){KBuf b{(unsigned char*)malloc(n+1),n+1};b.data[0]=0;memcpy(b.data+1,s,n);return b;}
extern "C" KBuf n2_kdec(const unsigned char*s,size_t,size_t n,int){KBuf b{(unsigned char*)malloc(n),n};memcpy(b.data,s+1,n);return b;}
'''
  with tempfile.TemporaryDirectory() as d:
   p=pathlib.Path(d);(p/'openzl_prefix.inc').write_text(fake);(p/'bridge.cpp').write_text(bridge);(p/'frames').mkdir();(p/'input').write_bytes(bytes(range(256))*256)
   src=pathlib.Path(__file__).parent/'native.cpp'
   extra=['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if os.environ.get('N2_FIXTURE_SANITIZE')=='1' else []
   subprocess.run(['g++','-std=c++17','-O1',*extra,'-Wall','-Wextra','-I'+str(p),str(src),str(p/'bridge.cpp'),'-o',str(p/'native')],check=True,capture_output=True)
   x=subprocess.run([str(p/'native'),str(p/'input'),str(p/'frames'),'2','41'],check=True,capture_output=True,text=True)
   rows=[json.loads(line) for line in x.stdout.splitlines()];run.validate_rows(rows,2)
   self.assertEqual(len(list((p/'frames').glob('*.frame'))),12)
   self.assertTrue(all((f.read_bytes()==b'\0'+(p/'input').read_bytes()) for f in (p/'frames').iterdir()))
if __name__=='__main__':unittest.main()
