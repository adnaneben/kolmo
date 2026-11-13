#!/usr/bin/env python3
"""Cross-check scalar C packing against an independently written Python inverse."""
import ctypes,random,subprocess,tempfile
from pathlib import Path
from reference import unpack,crc
HERE=Path(__file__).resolve().parent
with tempfile.TemporaryDirectory() as td:
    t=Path(td);c=t/'wrapper.c';so=t/'packing.so'
    c.write_text('#include "packing.h"\nint wrap(const uint64_t*v,size_t n,unsigned m,uint8_t*p,size_t cap,size_t*z){ec_buf b={0};int r=ec_pack(v,n,m,&b);if(!r&&b.n>cap)r=-1;if(!r){memcpy(p,b.p,b.n);*z=b.n;}ec_free(&b);return r;}\n')
    subprocess.run(['cc','-O2','-shared','-fPIC','-I'+str(HERE),str(c),'-o',str(so)],check=True)
    lib=ctypes.CDLL(str(so));lib.wrap.argtypes=[ctypes.POINTER(ctypes.c_uint64),ctypes.c_size_t,ctypes.c_uint,ctypes.POINTER(ctypes.c_ubyte),ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)];lib.wrap.restype=ctypes.c_int
    rng=random.Random(731094);total=0
    for n in (0,1,2,15,16,31,32,127,128,129,255,256,257,513):
        for bits in (0,1,3,8,9,16,17,32,33,63,64):
            v=[rng.getrandbits(bits) for _ in range(n)];values=(ctypes.c_uint64*max(n,1))(*v);output=(ctypes.c_ubyte*(n*16+1024))()
            for method in (1,2,3,4):
                size=ctypes.c_size_t();assert lib.wrap(values,n,method,output,len(output),ctypes.byref(size))==0
                assert unpack(bytes(output[:size.value]),n,method)==v;total+=1
    assert crc(b'123456789')==0xe3069283
    print(f'independent Python packing inverse: {total} cases passed')
