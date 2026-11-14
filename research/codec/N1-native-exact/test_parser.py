"""Native parser tests, without numpy or OpenZL dependencies."""
import json, os, pathlib, random, struct, subprocess, tempfile, unittest

def npy(data=b'\x01\0\0\0', dtype='<u4', shape='(1,)', version=1, tail=b'', header=None):
    h=header or "{'descr': '%s', 'fortran_order': False, 'shape': %s, }"%(dtype,shape)
    pre=10 if version==1 else 12
    h=h.encode();h+=b' '*((-len(h)-pre-1)%64)+b'\n'
    return b'\x93NUMPY'+bytes([version,0])+struct.pack('<H' if version==1 else '<I',len(h))+h+data+tail

def wav(data=b'\x01\0\x02\0',tail=b'',channels=1,extra=b''):
    fmt=struct.pack('<HHIIHH',1,channels,8000,8000*2*channels,2*channels,16)
    body=b'WAVE'+b'fmt '+struct.pack('<I',len(fmt))+fmt+extra+b'data'+struct.pack('<I',len(data))+data
    if len(data)%2:body+=b'\0'
    return b'RIFF'+struct.pack('<I',len(body))+body+tail

class ParserTests(unittest.TestCase):
    def invoke(self, b, valid=True):
        with tempfile.TemporaryDirectory() as td:
            p=pathlib.Path(td)/'input';p.write_bytes(b)
            r=subprocess.run([os.environ['PARSER'],str(p)],capture_output=True,timeout=10)
            self.assertEqual(r.returncode,0 if valid else 2,r.stderr.decode(errors='replace'))
            if valid:
                x=json.loads(r.stdout);self.assertEqual(x['prefix']+x['payload']+x['tail'],len(b));return x
    def test_integer_width_endian_version_matrix(self):
        for v in (1,2,3):
            for w in (1,2,4,8):
                for e in ('<','>'):
                    for k in ('i','u'):
                        with self.subTest(v=v,w=w,e=e,k=k):
                            x=self.invoke(npy(bytes(range(w*3)),e+k+str(w),'(3,)',v));self.assertEqual((x['width'],x['count'],x['big']),(w,3,int(e=='>')))
    def test_tail_and_reordered_header(self):
        x=self.invoke(npy(tail=b'\0TAIL\xff',header="{'shape':(1,), 'descr':'<u4','fortran_order':True}"));self.assertEqual(x['tail'],6)
    def test_zero_and_scalar(self):
        self.assertEqual(self.invoke(npy(b'',shape='(0,)'))['payload'],0)
        self.assertEqual(self.invoke(npy(shape='()'))['count'],1)
    def test_multidimensional(self):
        self.assertEqual(self.invoke(npy(b'\0'*24,shape='(2,3)'))['count'],6)
    def test_invalid_grammar(self):
        for h in ["{'descr':'<u4','descr':'<u4','shape':(1,)}", "{'descr':'<u4','fortran_order':0,'shape':(1,)}", "{'descr':'<u4','fortran_order':False,'shape':(1)}", "{'descr':'<u4','fortran_order':False,'shape':(-1,)}", "{'descr':'<u4','fortran_order':False,'shape':(999999999999999999999,)}", "{'descr':'<u4','fortran_order':False,'shape':(1,), 'x':4}"]:
            self.invoke(npy(header=h),False)
    def test_unsupported_dtype(self):
        for d in ('<f4','|O8','=u4','|u4','<i3'):
            self.invoke(npy(dtype=d),False)
    def test_bad_version_and_header(self):
        b=bytearray(npy());b[7]=1;self.invoke(b,False)
        b=bytearray(npy());b[127]=32;self.invoke(b,False)
    def test_truncated(self):
        for b in (npy(),wav()):
            for i in range(len(b)):self.invoke(b[:i],False)
    def test_wav_metadata_and_trailer(self):
        x=self.invoke(wav(extra=b'JUNK\x01\0\0\0x\0',tail=b'\xffEND'));self.assertEqual(x['tail'],4);self.assertEqual(x['width'],2)
    def test_wav_stereo(self):self.assertEqual(self.invoke(wav(channels=2))['count'],2)
    def test_wav_duplicate_data(self):
        b=bytearray(wav());b+=b'data\x02\0\0\0\0\0';struct.pack_into('<I',b,4,len(b)-8);self.invoke(b,False)
    def test_wav_inconsistent(self):
        for pos in (20,22,28,32,34):
            b=bytearray(wav());b[pos]^=1;self.invoke(b,False)
    def test_random_exact_layouts(self):
        rng=random.Random(20261113)
        for _ in range(128):
            w=rng.choice((1,2,4,8));n=rng.randrange(50);tail=rng.randbytes(rng.randrange(8));payload=rng.randbytes(w*n)
            b=npy(payload,'<u'+str(w),'(%d,)'%n,tail=tail);x=self.invoke(b)
            self.assertEqual(b[x['prefix']:x['prefix']+x['payload']],payload)
    def test_bounded_arbitrary_bytes(self):
        rng=random.Random(87)
        for _ in range(128):self.invoke(rng.randbytes(rng.randrange(256)),False)
if __name__=='__main__':unittest.main()
