import copy,json,pathlib,unittest
from run import ARMS,validate,PAIRS
from build import stage_baseline
class RunnerTests(unittest.TestCase):
    def data(self):
        m={a:{'bytes':100,'decisions':[]} for a in ARMS}
        r=[dict(arm=a,op=o,round=0,batch=8,input=200,bytes=100,wall_ns=1000,cpu_ns=1000) for a in ARMS for o in [0,1]]
        return r,m
    def test_valid(self):
        r,m=self.data();self.assertTrue(validate(r,m,1,200))
    def test_round_zero(self):
        _,m=self.data();self.assertTrue(validate([],m,0,200))
    def test_drop(self):
        r,m=self.data();self.assertRaises(ValueError,validate,r[:-1],m,1,200)
    def test_duplicate(self):
        r,m=self.data();self.assertRaises(ValueError,validate,r+[r[0]],m,1,200)
    def test_batch(self):
        r,m=self.data();r[0]['batch']=1;self.assertRaises(ValueError,validate,r,m,1,200)
    def test_nan(self):
        r,m=self.data();r[0]['wall_ns']=float('nan');self.assertRaises(ValueError,validate,r,m,1,200)
    def test_negative(self):
        r,m=self.data();r[0]['cpu_ns']=-1;self.assertRaises(ValueError,validate,r,m,1,200)
    def test_zero(self):
        r,m=self.data();r[0]['wall_ns']=0;self.assertRaises(ValueError,validate,r,m,1,200)
    def test_unknown(self):
        r,m=self.data();m['other']={};self.assertRaises(ValueError,validate,r,m,1,200)
    def test_expansion(self):
        r,m=self.data();m['gate_menu']['bytes']=101;self.assertRaises(ValueError,validate,r,m,1,200)
    def test_decisions(self):
        _,m=self.data();m['gate_menu']['decisions']=[dict(sampled=4097,selected='H',tried=0,rejected=0)]
        self.assertRaises(ValueError,validate,[],m,0,200)
    def test_rejection(self):
        _,m=self.data();m['gate_menu']['decisions']=[dict(sampled=4096,selected='HZ',tried=1,rejected=1)]
        self.assertRaises(ValueError,validate,[],m,0,200)
    def test_source_boundary(self):self.assertRaises(ValueError,stage_baseline,'no main')
    def test_ambiguous_source(self):self.assertRaises(ValueError,stage_baseline,'\nint main(\nint main(')
    def test_clock_instrumentation(self):
        t='extern "C" { int s_type(); }\nusing V=int; void f(){s_type();}\nint main(){}'
        s=stage_baseline(t);self.assertIn('int s_type();',s);self.assertIn('void f(){p_type();}',s)
if __name__=='__main__':unittest.main()
