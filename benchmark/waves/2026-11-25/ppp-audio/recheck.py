"""Interleave two disabled controls and the enabled screen on the same binary."""
import argparse
import csv
import hashlib
import os
from pathlib import Path
import random
import resource
import statistics
import subprocess
import tempfile
import time

ap=argparse.ArgumentParser()
for n in ('candidate','release','out','mono','pcm'):
    ap.add_argument('--'+n,type=Path,required=True)
a=ap.parse_args()
cells=[(a.mono/'Purr.wav','ultra',4),(a.mono/'Purr.wav','7',1),(a.mono/'Tink.wav','7',1),(a.pcm/'test4.wav','7',1)]
seed=random.Random(73211)
rows=[]
with tempfile.TemporaryDirectory() as t, (a.out/'fallback-raw.csv').open('w',newline='',buffering=1) as fd:
    writer=None;frame=Path(t)/'f';out=Path(t)/'o'
    for rep in range(16):
        seed.shuffle(cells)
        for src,level,nt in cells:
            order=['A','AA','B'];seed.shuffle(order)
            frames=[]
            for arm in order:
                env=dict(os.environ,KOLMO_AUDIO_SCREEN=str(int(arm=='B')))
                cpu0=resource.getrusage(resource.RUSAGE_CHILDREN);start=time.perf_counter_ns()
                subprocess.run([a.candidate,'--level',level,f'-T{nt}','-q','-f',src,'-o',frame],env=env,capture_output=True,check=True)
                mid=time.perf_counter_ns()
                subprocess.run([a.release,'-d',f'-T{nt}','-q','-f',frame,'-o',out],env=env,capture_output=True,check=True)
                end=time.perf_counter_ns();cpu1=resource.getrusage(resource.RUSAGE_CHILDREN)
                if src.read_bytes()!=out.read_bytes(): raise ValueError('readback failed')
                frames.append(hashlib.sha256(frame.read_bytes()).hexdigest())
                r=dict(src=src.name,level=level,threads=nt,rep=rep,arm=arm,frame_sha256=frames[-1],encode_ms=(mid-start)/1e6,decode_ms=(end-mid)/1e6,total_ms=(end-start)/1e6,cpu_ms=1000*(cpu1.ru_utime+cpu1.ru_stime-cpu0.ru_utime-cpu0.ru_stime))
                if writer is None:writer=csv.DictWriter(fd,lineterminator='\n',fieldnames=r);writer.writeheader()
                writer.writerow(r);rows.append(r)
            if len(set(frames))!=1: raise ValueError('frame mismatch')
with (a.out/'fallback-score.csv').open('w',newline='') as fd:
    writer=None
    for src,level,nt in cells:
        g=[x for x in rows if x['src']==src.name and x['level']==level and x['threads']==nt and x['rep']]
        arms=[[x for x in g if x['arm']==k] for k in ('A','AA','B')]
        r=dict(source=src.name,level=level,threads=nt,pairs=15)
        for metric in ('total_ms','cpu_ms'):
            cost=[z[metric]/((a[metric]+aa[metric])/2) for a,aa,z in zip(*arms)]
            r[metric+'_B_to_controls']=statistics.median(cost)
            r[metric+'_AA_to_A']=statistics.median(aa[metric]/a[metric] for a,aa in zip(arms[0],arms[1]))
            r[metric+'_slower_count']=sum(v>1 for v in cost)
        if writer is None:writer=csv.DictWriter(fd,lineterminator='\n',fieldnames=r);writer.writeheader()
        writer.writerow(r)
