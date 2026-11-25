"""Record classifier activity separately from randomized full-file CLI timings."""
from cases import cases
from pathlib import Path
from statistics import median
import hashlib
import json
import os
import random
import subprocess
import sys
import tempfile
import time
out=Path(sys.argv[1]);work=Path(sys.argv[2]);reader=sys.argv[3]
scored = int(sys.argv[4]) if len(sys.argv)>4 else 5
selected = set(sys.argv[5:])
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(command):
    t=time.perf_counter_ns()
    proc=subprocess.Popen(list(map(str,command)),stderr=subprocess.DEVNULL,stdout=subprocess.DEVNULL)
    _,status,ru=os.wait4(proc.pid,0)
    wall=(time.perf_counter_ns()-t)/1e9
    proc.returncode=os.waitstatus_to_exitcode(status)
    if proc.returncode:raise subprocess.CalledProcessError(proc.returncode,command)
    return wall,ru.ru_utime+ru.ru_stime,ru.ru_maxrss
rng=random.Random(3625)
with tempfile.TemporaryDirectory() as tmp,(out/'counters.jsonl').open('x') as counts,(out/'observations.jsonl').open('x') as results:
    frame=Path(tmp)/'frame';restored=Path(tmp)/'plain'
    for name,src,lvl,threads in cases:
        if selected and name not in selected: continue
        c=[f'-{lvl}',f'-T{threads}','-f','-o',frame,src]
        counter=subprocess.run([work/'counter/kolmo',*c],capture_output=True,check=True)
        counter_rows=[list(map(int,line.split()[1:])) for line in counter.stderr.decode().splitlines() if line.startswith('B36 ')]
        expected=sha(src);framehash=sha(frame)
        counts.write(json.dumps(dict(case=name,file=str(src),level=lvl,threads=threads,source_sha256=expected,
                      passes=counter_rows,worker_hashes=sum(x[1] for x in counter_rows),
                      parse_hashes=sum(x[2] for x in counter_rows),frame_sha256=framehash))+'\n')
        counts.flush()
        for rep in range(scored+1):
            arms=['A','A2','B'];rng.shuffle(arms)
            for arm in arms:
                binary=work/('variant' if arm=='B' else 'reference')/'kolmo'
                e,ec,er=run([binary,*c])
                d,dc,dr=run([reader,'-d',f'-T{threads}','-f','-o',restored,frame])
                sh=sha(frame);actual=sha(restored)
                assert sh==framehash and actual==expected
                row=dict(case=name,level=lvl,threads=threads,rep=rep,arm=arm,n=src.stat().st_size,
                         size=frame.stat().st_size,input_sha256=expected,frame_sha256=sh,readback_sha256=actual,
                         enc=e,dec=d,enc_cpu=ec,dec_cpu=dc,enc_rss=er,dec_rss=dr)
                results.write(json.dumps(row)+'\n');results.flush()
        print(name,flush=True)
