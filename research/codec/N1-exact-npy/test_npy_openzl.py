"""N1 exact-NPY adapter tests; zlib-backed MOCK child is not real OpenZL."""
import importlib.util
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import npy_openzl as w

MOCK='''#!/usr/bin/env python3
import sys,zlib
from pathlib import Path
a=sys.argv[1:]; mode=a[0]
if mode=='compress':
    profile=a[a.index('-p')+1]
    assert profile in ('le-u32','le-i16','le-i64','be-u32','be-i16','be-i64','le-u16','be-u16','le-i32','be-i32','le-u64','be-u64')
    src=Path(a[a.index('-f')+1]);dst=Path(a[a.index('-o')+1])
    dst.write_bytes(zlib.compress(src.read_bytes()))
elif mode=='decompress':
    src=Path(a[a.index('-f')+1]);dst=Path(a[a.index('-o')+1])
    dst.write_bytes(zlib.decompress(src.read_bytes()))
else: raise ValueError(mode)
'''

def make_npy(payload,descr='<u4',shape=None,version=1,fortran=False):
    width=int(descr[-1])
    if shape is None:shape=(len(payload)//width,)
    meta={'descr':descr,'fortran_order':fortran,'shape':shape}
    header=str(meta).encode('ascii')+b'\n'
    prefix=10 if version==1 else 12
    header=header[:-1]+b' '*((-prefix-len(header))%64)+b'\n'
    return b'\x93NUMPY'+bytes((version,0))+len(header).to_bytes(2 if version==1 else 4,'little')+header+payload

class AdapterTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.zli=Path(self.tmp.name)/'zli'
        self.zli.write_text(MOCK);self.zli.chmod(0o755)
        self.original=make_npy(struct.pack('<5I',0,1,2,2**32-1,17))
    def test_version_endian_types(self):
        cases=[('<u4',struct.pack('<I',9),'le-u32'),
               ('>i2',b'\0\x01\xff\xff','be-i16'),
               ('<i8',b'\x01'+b'\0'*7,'le-i64')]
        for version in (1,2,3):
            for descr,payload,profile in cases:
                with self.subTest(version=version,descr=descr):
                    data=make_npy(payload,descr,version=version)
                    self.assertEqual(w.npy_header(data)[2],profile)
                    self.assertEqual(w.decode(w.encode(data,self.zli,9),self.zli),data)
    def test_levels(self):
        for level in (6,7,9):
            self.assertEqual(w.decode(w.encode(self.original,self.zli,level),self.zli),self.original)
    def test_scalar_empty(self):
        for shape,payload in (((),struct.pack('<I',1)),((0,),b'')):
            b=make_npy(payload,shape=shape)
            self.assertEqual(w.decode(w.encode(b,self.zli,9),self.zli),b)
    def test_reject_invalid_input(self):
        bad=[b'garbage',self.original+b'junk',
             make_npy(struct.pack('<I',9),fortran=True),
             make_npy(struct.pack('<I',9),descr='<f4'),
             make_npy(struct.pack('<I',9),descr='<u1'),
             make_npy(struct.pack('<I',9),shape=(3,))]
        for b in bad:
            with self.subTest(b=b[:15]),self.assertRaises(w.FormatError):
                w.encode(b,self.zli,9)
    def test_bad_level(self):
        with self.assertRaises(w.FormatError):w.encode(self.original,self.zli,8)
    def test_bad_frames(self):
        good=w.encode(self.original,self.zli,9)
        for x in (good[:-1],good+b'extra',b'\0'+good[1:]):
            with self.assertRaises(w.FormatError):w.decode(x,self.zli)
    def test_corrupt_payload_rejected(self):
        d=bytearray(w.encode(self.original,self.zli,9));d[-1]^=0x80
        with self.assertRaises(w.FormatError):w.decode(bytes(d),self.zli)
    def test_corrupt_header_rejected(self):
        d=bytearray(w.encode(self.original,self.zli,9));d[w.FRAME.size+8]^=0x10
        with self.assertRaises(w.FormatError):w.decode(bytes(d),self.zli)
    def test_full_cli(self):
        a=Path(self.tmp.name)/'in.npy';b=Path(self.tmp.name)/'frame';c=Path(self.tmp.name)/'out.npy'
        a.write_bytes(self.original)
        source=Path(w.__file__).resolve()
        for action,inp,out in [('compress',a,b),('decompress',b,c)]:
            subprocess.run([sys.executable,str(source),action,str(inp),str(out),'--zli',str(self.zli)],check=True)
        self.assertEqual(c.read_bytes(),self.original)
    def test_child_failure(self):
        bad=Path(self.tmp.name)/'bad-zli';bad.write_text('#!/bin/sh\nexit 2\n');bad.chmod(0o755)
        with self.assertRaises(w.FormatError):w.encode(self.original,bad,9)

STUB='''def policies(profile,tools):
 return [{'id':'zstd_3','tool':'zstd','enc':['zstd'],'dec':['zstd','-d']},
         {'id':'openzl_'+profile+'_9','tool':'openzl','enc':['zli'],'dec':['zli','-d']},
         {'id':'openzl_serial_9','tool':'openzl','enc':['zli'],'dec':['zli','-d']}]
def command(policy,op,src,dst):
 return policy['enc' if op=='encode' else 'dec']+[str(src),str(dst)]
'''

class RunnerTest(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        root=Path(self.tmp.name);base=root/'N1-baselines';base.mkdir()
        (base/'baselines.py').write_text(STUB)
        target=root/'N1-exact-npy';target.mkdir()
        shutil.copyfile(Path(__file__).resolve().parent/'run_npy_adapter.py',target/'run_npy_adapter.py')
        spec=importlib.util.spec_from_file_location('n1_compat_test',target/'run_npy_adapter.py')
        self.m=importlib.util.module_from_spec(spec)
        sys.modules['n1_compat_test']=self.m;spec.loader.exec_module(self.m)
        self.tools={'zstd':'/zstd','kolmo':'/kolmo','openzl':'/zli'}
    def test_filters_numpy_invalid(self):
        names={x['id'] for x in self.m.policies('numpy',self.tools)}
        self.assertNotIn('openzl_numpy_9',names)
        self.assertEqual({f'openzl_exact_npy_{x}' for x in (6,7,9)} & names,
                         {f'openzl_exact_npy_{x}' for x in (6,7,9)})
        self.assertIn('zstd_3',names)
    def test_keeps_csv(self):
        names={x['id'] for x in self.m.policies('csv',self.tools)}
        self.assertIn('openzl_csv_9',names)
        self.assertFalse(any('exact_npy' in n for n in names))
    def test_command_charges_python(self):
        p=next(p for p in self.m.policies('numpy',self.tools) if p['id']=='openzl_exact_npy_9')
        args=self.m.command(p,'encode',Path('/in'),Path('/out'))
        self.assertEqual(args[0],sys.executable)
        self.assertIn('--zli',args)
        self.assertEqual(args[args.index('--zli')+1],'/zli')
        self.assertIn('compress',args)
    def test_control_command_unchanged(self):
        p=next(p for p in self.m.policies('numpy',self.tools) if p['id']=='zstd_3')
        self.assertEqual(self.m.command(p,'encode',Path('/a'),Path('/b')),['zstd','/a','/b'])

if __name__=='__main__':unittest.main()
