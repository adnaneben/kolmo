"""Run a valid-input, full CLI A/A/B comparison. Normalized input prefixes are hashed."""
import csv
import hashlib
from pathlib import Path
import random
import resource
import subprocess
import sys
import tempfile
import time

def phase(argv):
    start = time.perf_counter_ns()
    before = resource.getrusage(resource.RUSAGE_CHILDREN)
    subprocess.run(argv,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    after = resource.getrusage(resource.RUSAGE_CHILDREN)
    return time.perf_counter_ns()-start, round(1e9*(after.ru_utime-before.ru_utime+after.ru_stime-before.ru_stime))

def run(base,cand,paths):
    rng = random.Random(291125)
    w = csv.writer(sys.stdout)
    w.writerow('name bytes input_sha256 level round arm stored frame_sha256 enc_ns dec_ns enc_cpu_ns dec_cpu_ns'.split())
    with tempfile.TemporaryDirectory() as tmp:
        root=Path(tmp); src=root/'data'; frame=root/'data.kolmo'; out=root/'restored'
        for path in paths:
            data=(bytes(range(64))*512+b'\x00'*(64<<10))*8 if path=='synthetic-run' else Path(path).read_bytes()[:1<<20]
            src.write_bytes(data)
            digest=hashlib.sha256(data).hexdigest()
            for level in ('1','2','3'):
                first=None
                for rep in range(12):
                    for arm in rng.sample(['A','A2','B'],3):
                        exe=cand if arm=='B' else base
                        enc,ecpu=phase([exe,'-'+level,'-T1','-f','-o',str(frame),str(src)])
                        dec,dcpu=phase([exe,'-d','-T1','-f','-o',str(out),str(frame)])
                        packed=frame.read_bytes()
                        if first is None: first=packed
                        assert first==packed and out.read_bytes()==data
                        w.writerow([Path(path).name,len(data),digest,level,rep,arm,len(packed),
                                    hashlib.sha256(packed).hexdigest(),enc,dec,ecpu,dcpu])
                sys.stdout.flush()
if __name__=='__main__':
    run(*sys.argv[1:3],sys.argv[3:])
