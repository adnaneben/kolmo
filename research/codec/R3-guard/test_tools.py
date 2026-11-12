"""Synthetic infrastructure tests, not new-source measurements."""
import gzip,struct,unittest
from prepare import npy_u32,parse_collegemsg,parse_covertype,gunzip
from decide import family_pass
class PreparationTests(unittest.TestCase):
 def test_npy_exact(self):
  b=npy_u32([0,1,2**32-1]);h=10+int.from_bytes(b[8:10],'little');self.assertEqual(b[:8],b'\x93NUMPY\x01\x00');self.assertEqual(h%64,0);self.assertEqual(struct.unpack('<3I',b[h:]),(0,1,2**32-1))
 def test_invalid_uint(self):
  for v in (-1,2**32,1.5):
   with self.subTest(v=v),self.assertRaises(ValueError):npy_u32([v])
 def test_temporal(self):self.assertEqual(parse_collegemsg(b'# comment\n1 2 123\n2 1 125\n',2),[[1,2],[2,1],[123,125]])
 def test_temporal_invalid(self):
  for s in (b'1 2 3.0\n',b'1 2\n',b'1 2 -1\n',b'1 2 4294967296\n',b'1 2 3\n'):
   with self.subTest(s=s),self.assertRaises(ValueError):parse_collegemsg(s,2)
 def test_terrain(self):
  row=','.join(str(i) for i in range(55));data=(row+'\n'+row+'\n').encode();r=parse_covertype(data,2);self.assertEqual(r[54],[54,54]);self.assertEqual(r[5],[5,5])
 def test_terrain_invalid(self):
  with self.assertRaises(ValueError):parse_covertype(b'1,2\n',1)
 def test_gzip_bound(self):
  b=gzip.compress(b'x'*17);self.assertEqual(gunzip(b,17),b'x'*17)
  with self.assertRaises(ValueError):gunzip(b,16)
 def test_profile_pass_and_fail(self):
  d={}
  for name in ('a','b'):
   for p in (0,7,8,9,10):
    d[name,p]={'family':'unit','planes':2,'input_bytes':100000,'bytes':9500 if p==9 else 10000,'aligned':p==9}
    for f in ('encode','decode_verified_warm','decode_kernel_warm','decode_verified_displaced32MiB','decode_kernel_displaced32MiB'):d[name,p][f+'_wall_ns']=1000
  self.assertTrue(family_pass(d,'unit')['supports']);d['a',9]['encode_wall_ns']=2000;self.assertFalse(family_pass(d,'unit')['supports']);d['a',10]['encode_wall_ns']=2000;self.assertFalse(family_pass(d,'unit')['checks']['noise'])
if __name__=='__main__':unittest.main()
