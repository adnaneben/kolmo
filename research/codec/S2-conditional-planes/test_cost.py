import unittest
from audit import charged
class CostTests(unittest.TestCase):
 def test_wall(self):self.assertAlmostEqual(charged(dict(wall_ns=1000000,cpu_ns=900000,batch=2)),0.0005)
 def test_cpu(self):self.assertAlmostEqual(charged(dict(wall_ns=1000000,cpu_ns=1040000,batch=2)),0.00052)
 def test_boundary_below(self):self.assertAlmostEqual(charged(dict(wall_ns=1000000,cpu_ns=1029999,batch=1)),0.001)
 def test_linear_batch(self):
  r=dict(wall_ns=10000000,cpu_ns=11000000,batch=1);s={**r,'batch':5};self.assertAlmostEqual(charged(r),5*charged(s))
 def test_transfer(self):self.assertAlmostEqual(125000/125000000,0.001)
if __name__=='__main__':unittest.main()
