"""Link-level exact reconstruction; independent stock API decoder."""
import os,pathlib,random,struct,subprocess,tempfile,unittest
from test_parser import npy,wav
class Frames(unittest.TestCase):
 def call(self,args,ok=True):
  p=subprocess.run(args,capture_output=True,timeout=20)
  if ok:self.assertEqual(p.returncode,0,p.stderr.decode(errors='replace'))
  return p
 def roundtrip(self,data):
  with tempfile.TemporaryDirectory() as td:
   r=pathlib.Path(td);inp=r/'input';inp.write_bytes(data)
   for lev in ('6','9'):
    frame=r/('c'+lev);out=r/('d'+lev);stock=r/('s'+lev);again=r/('a'+lev)
    self.call([os.environ['ADAPTER'],'c',lev,'exact',str(inp),str(frame)])
    self.call([os.environ['ADAPTER'],'c',lev,'exact',str(inp),str(again)])
    self.assertEqual(frame.read_bytes(),again.read_bytes())
    self.call([os.environ['ADAPTER'],'d',str(frame),str(out)])
    self.call([os.environ['STOCK_READER'],str(frame),str(stock)])
    self.assertEqual(out.read_bytes(),data);self.assertEqual(stock.read_bytes(),data)
 def test_npy_matrix(self):
  rng=random.Random(144)
  for w in (1,2,4,8):
   for endian in ('<','>'):
    for ver in (1,2,3):self.roundtrip(npy(rng.randbytes(w*33),endian+'i'+str(w),'(33,)',ver,tail=b'\0EXACT\xff'))
 def test_wav(self):
  for ch in (1,2):self.roundtrip(wav(bytes(range(200)),channels=ch,extra=b'JUNK\x01\0\0\0x\0',tail=b'UNTOUCHED'))
 def test_empty_numeric(self):self.roundtrip(npy(b'',shape='(0,)',tail=b'TAIL'))
 def test_archived_inputs(self):
  for p in sorted(pathlib.Path(os.environ['INPUTS']).glob('*.npy')):self.roundtrip(p.read_bytes())
 def test_malformed_and_overwrite(self):
  with tempfile.TemporaryDirectory() as td:
   r=pathlib.Path(td);p=r/'in';f=r/'frame';p.write_bytes(npy(bytes(range(128))*8,'<u4','(256,)'))
   self.call([os.environ['ADAPTER'],'c','9','exact',str(p),str(f)])
   self.assertNotEqual(self.call([os.environ['ADAPTER'],'c','9','exact',str(p),str(f)],False).returncode,0)
   data=f.read_bytes();bad=r/'bad';out=r/'out';rng=random.Random(11)
   tests=[data[:i] for i in sorted(set(range(min(20,len(data))))|{len(data)-1,len(data)//2})]+[data+b'junk']
   for _ in range(64):
    b=bytearray(data);j=rng.randrange(len(b));b[j]^=1<<rng.randrange(8);tests.append(bytes(b))
   for b in tests:
    bad.write_bytes(b);out.unlink(missing_ok=True);res=self.call([os.environ['ADAPTER'],'d',str(bad),str(out)],False)
    if res.returncode==0:self.assertEqual(out.read_bytes(),p.read_bytes())
if __name__=='__main__':unittest.main()
