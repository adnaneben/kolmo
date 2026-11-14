import io,json,math,unittest,zipfile
from audit import entropy,plane_stats,validate_zip,sha
class AuditTests(unittest.TestCase):
 def test_empty_entropy(self): self.assertEqual(entropy(b''),0)
 def test_constant_entropy(self): self.assertEqual(entropy(b'x'*80),0)
 def test_binary_entropy(self): self.assertAlmostEqual(entropy(b'\x00\x01'*32),1)
 def test_uniform_entropy(self): self.assertAlmostEqual(entropy(bytes(range(256))),8)
 def test_plane_gain(self): self.assertAlmostEqual(plane_stats(b'\x00'*80+b'\xff'*80,2)['ideal_plane_conditioning_bytes'],20)
 def test_single_plane(self): self.assertEqual(plane_stats(bytes(range(256)),1)['ideal_plane_conditioning_bytes'],0)
 def test_invalid_planes(self):
  for data,p in [(b'abc',2),(b'abc',0)]:
   with self.assertRaises(ValueError):plane_stats(data,p)
 def test_constant_count(self):self.assertEqual(plane_stats(b'\x00'*20+b'ab'*10,2)['constant_planes'],1)
 def test_manifest(self):
  b=io.BytesIO()
  with zipfile.ZipFile(b,'w') as z:z.writestr('a',b'data');z.writestr('MANIFEST.json',json.dumps({'a':sha(b'data')}))
  with zipfile.ZipFile(io.BytesIO(b.getvalue())) as z:self.assertEqual(validate_zip(z),1)
 def test_bad_manifest(self):
  b=io.BytesIO()
  with zipfile.ZipFile(b,'w') as z:z.writestr('a',b'data');z.writestr('MANIFEST.json',json.dumps({'a':'bad'}))
  with zipfile.ZipFile(io.BytesIO(b.getvalue())) as z:
   with self.assertRaises(ValueError):validate_zip(z)
 def test_unsafe_member(self):
  b=io.BytesIO()
  with zipfile.ZipFile(b,'w') as z:z.writestr('../a',b'data');z.writestr('MANIFEST.json',json.dumps({'../a':sha(b'data')}))
  with zipfile.ZipFile(io.BytesIO(b.getvalue())) as z:
   with self.assertRaises(ValueError):validate_zip(z)
if __name__=='__main__':unittest.main()
