import copy,unittest
from run import validate
class RunnerTests(unittest.TestCase):
 def setUp(self):
  self.frames={'a':{'bytes':100},'b':{'bytes':101}};self.arms=['a','b'];self.rows=[{'arm':a,'op':o,'round':r,'batch':8,'input':1000,'bytes':self.frames[a]['bytes'],'wall_ns':100,'cpu_ns':99} for a in self.arms for o in (0,1) for r in range(2)]
 def test_good(self):self.assertTrue(validate(self.rows,self.frames,self.arms,2,1000))
 def test_missing(self):
  with self.assertRaises(ValueError):validate(self.rows[:-1],self.frames,self.arms,2,1000)
 def test_duplicate(self):
  with self.assertRaises(ValueError):validate(self.rows+[self.rows[0]],self.frames,self.arms,2,1000)
 def test_input(self):
  self.rows[0]['input']=1001
  with self.assertRaises(ValueError):validate(self.rows,self.frames,self.arms,2,1000)
 def test_batch(self):
  self.rows[0]['batch']=9
  with self.assertRaises(ValueError):validate(self.rows,self.frames,self.arms,2,1000)
 def test_stored(self):
  self.rows[0]['bytes']+=1
  with self.assertRaises(ValueError):validate(self.rows,self.frames,self.arms,2,1000)
 def test_zero(self):
  self.rows[0]['wall_ns']=0
  with self.assertRaises(ValueError):validate(self.rows,self.frames,self.arms,2,1000)
 def test_negative(self):
  self.rows[0]['cpu_ns']=-1
  with self.assertRaises(ValueError):validate(self.rows,self.frames,self.arms,2,1000)
 def test_nan(self):
  self.rows[0]['cpu_ns']=float('nan')
  with self.assertRaises(ValueError):validate(self.rows,self.frames,self.arms,2,1000)
 def test_bool(self):
  self.rows[0]['op']=True
  with self.assertRaises(ValueError):validate(self.rows,self.frames,self.arms,2,1000)
 def test_no_frames(self):
  with self.assertRaises(ValueError):validate(self.rows,{},self.arms,2,1000)
if __name__=='__main__':unittest.main()
