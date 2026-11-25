"""Time 32 stock raw KLZ payload decodes per arm, then compare exact output."""
import csv
import ctypes as c
import hashlib
from pathlib import Path
import random
import sys
import time
def main():
    rng=random.Random(291126)
    libs=[c.CDLL(path) for path in sys.argv[1:3]]
    for lib in libs:
        lib.kolmo_klz_compress.argtypes=[c.c_void_p,c.c_size_t,c.c_void_p,c.c_size_t,c.c_int]
        lib.kolmo_klz_compress.restype=c.c_size_t
        lib.kolmo_klz_decompress.argtypes=[c.c_void_p,c.c_size_t,c.c_void_p,c.c_size_t]
    writer=csv.writer(sys.stdout)
    writer.writerow('name input_sha256 bytes frame_sha256 stored round arm batch wall_ns cpu_ns'.split())
    for name in sys.argv[3:]:
        data=(bytes(range(64))*512+b'\x00'*(64<<10))*8 if name=='synthetic-run' else Path(name).read_bytes()[:1<<20]
        n=len(data)
        compressed=c.create_string_buffer(n*2+1024)
        size=libs[0].kolmo_klz_compress(data,n,compressed,len(compressed),1)
        assert size>0
        packed=compressed.raw[:size]
        restored=c.create_string_buffer(n)
        for rep in range(12):
            for arm in rng.sample(['A','A2','B'],3):
                dec=libs[int(arm=='B')].kolmo_klz_decompress
                cpu=time.process_time_ns();wall=time.perf_counter_ns()
                for _ in range(32):
                    assert dec(packed,size,restored,n)==0
                dt=time.perf_counter_ns()-wall;used=time.process_time_ns()-cpu
                assert restored.raw==data
                writer.writerow([Path(name).name,hashlib.sha256(data).hexdigest(),n,hashlib.sha256(packed).hexdigest(),size,rep,arm,32,dt,used])
if __name__=='__main__':
    main()
