#!/usr/bin/env python3
"""Compare original and current fast-mode libraries in randomized in-memory runs."""
import collections
import ctypes as C
import csv
import pathlib
import random
import sys
import time

root = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root/'bindings/python'))
from kolmo import _Params
libs = {}
for label, path in [('before','build/baseline/libkolmo.dylib'),('after','build/libkolmo.dylib')]:
    x = C.CDLL(str(root/path))
    x.czn_bound.argtypes = [C.c_size_t,C.POINTER(_Params)]; x.czn_bound.restype=C.c_size_t
    x.czn_compress.argtypes=[C.c_char_p,C.c_size_t,C.c_char_p,C.c_size_t,C.POINTER(_Params)]; x.czn_compress.restype=C.c_int64
    x.czn_decompress.argtypes=[C.c_char_p,C.c_size_t,C.c_char_p,C.c_size_t,C.c_int]; x.czn_decompress.restype=C.c_int64
    libs[label]=x
assert C.cast(libs['before'].czn_compress,C.c_void_p).value != C.cast(libs['after'].czn_compress,C.c_void_p).value
files=['silesia/mozilla','silesia/nci','owid-co2-data.csv']
jobs=[(label,f,t,i) for label in libs for f in files for t in [1,4,10] for i in range(5)]
random.Random(20261107).shuffle(jobs)
inputs={f:(root/'benchmark/audit/data'/f).read_bytes() for f in files}
refs={}; output=root/'benchmark/audit/milestone1/performance.csv'
with output.open('w') as out:
    w=csv.writer(out);w.writerow(['version','file','threads','repeat','input_bytes','compressed_bytes','compress_seconds','decode_seconds'])
    for label,f,t,i in jobs:
        lib=libs[label]; data=inputs[f]; p=_Params(1,0,t,0,1)
        buf=C.create_string_buffer(lib.czn_bound(len(data),C.byref(p)))
        start=time.perf_counter();n=lib.czn_compress(data,len(data),buf,len(buf),C.byref(p));elapsed=time.perf_counter()-start
        assert n>0
        payload=buf.raw[:n]; reference=refs.setdefault(f,payload)
        assert payload==reference,(label,f,t,'output changed')
        back=C.create_string_buffer(len(data))
        start=time.perf_counter();d=lib.czn_decompress(payload,len(payload),back,len(back),t);de=time.perf_counter()-start
        assert d==len(data) and back.raw==data
        w.writerow([label,f,t,i,len(data),n,elapsed,de]);out.flush()
print('PERFORMANCE COMPARISON PASSED (90 randomized runs; identical frames across versions and thread counts)')
