import unittest,math
from audit import finite,charged,costs,metrics,delta
class AuditTests(unittest.TestCase):
 def test_bad_numbers(self):
  for x in [True,-1,float('nan'),float('inf'),'1']:
   with self.assertRaises(ValueError):finite(x)
 def test_cpu(self):
  r=dict(wall_ns=100,cpu_ns=104,loops=2);self.assertAlmostEqual(charged(r),52e-9)
  r['cpu_ns']=103;self.assertAlmostEqual(charged(r),50e-9)
 def test_cost(self):
  r=dict(wall_ns=10**9,cpu_ns=10**9,loops=1)
  self.assertAlmostEqual(costs(r,r,125000000)['speed'],3)
 def test_empty(self):
  with self.assertRaises(ValueError):metrics([])
 def test_aggregate(self):
  rows=[dict(input_bytes=100,stored_bytes=10,encode_s=1,decode_s=2,enc_avg_mib=4,enc_peak_mib=8,dec_avg_mib=5,dec_peak_mib=6,speed_cost=3,archive_cost=2),dict(input_bytes=100,stored_bytes=50,encode_s=2,decode_s=1,enc_avg_mib=6,enc_peak_mib=7,dec_avg_mib=5,dec_peak_mib=10,speed_cost=3,archive_cost=2)]
  m=metrics(rows);self.assertAlmostEqual(m['ratio'],200/60);self.assertEqual(m['enc_avg_mib'],5);self.assertEqual(m['enc_peak_mib'],8);self.assertEqual(m['dec_peak_mib'],10);self.assertAlmostEqual(m['encode_mib_s'],200/1048576/3)
  self.assertTrue(all(x==0 for x in delta(m,m).values()))
if __name__=='__main__':unittest.main()
