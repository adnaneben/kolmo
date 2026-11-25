"""Whole-WAV FLAC and opt-in Kolmo comparison, including foreign metadata and stock-reader verification."""
import argparse
import csv
import hashlib
import os
from pathlib import Path
import random
import statistics
import subprocess
import tempfile
import time

ap=argparse.ArgumentParser()
for name in ("flac","candidate","release","out"):
    ap.add_argument('--'+name,type=Path,required=True)
ap.add_argument('sources',nargs='+',type=Path)
args=ap.parse_args()
def run(cmd):
    start=time.perf_counter_ns()
    subprocess.run(cmd,env=dict(os.environ,KOLMO_AUDIO_SCREEN="1"),capture_output=True,check=True)
    return (time.perf_counter_ns()-start)/1e6
seed=random.Random(932120)
rows=[]
with tempfile.TemporaryDirectory() as t, (args.out/'flac-raw.csv').open('w',newline='') as fd:
    w=Path(t);writer=None
    for rep in range(6):
        cells=[(s,a) for s in args.sources for a in ('K7','Kultra','F5','F8')];seed.shuffle(cells)
        for source,arm in cells:
            original=source.read_bytes()
            frame=w/('out.flac' if arm[0]=='F' else 'out.kol')
            decoded=w/'restored.wav'
            if arm[0]=='F':
                enc=run([args.flac,'-'+arm[1:],'--no-padding','--no-seektable','--keep-foreign-metadata','-s','-f','-o',frame,source])
                dec=run([args.flac,'-d','--keep-foreign-metadata','-s','-f','-o',decoded,frame])
            else:
                enc=run([args.candidate,'--level',arm[1:],'-T1','-q','-f',source,'-o',frame])
                dec=run([args.release,'-d','-T1','-q','-f',frame,'-o',decoded])
            if decoded.read_bytes()!=original: raise ValueError((source,arm))
            row=dict(source=source.name,sha256=hashlib.sha256(original).hexdigest(),rep=rep,arm=arm,raw=len(original),stored=frame.stat().st_size,encode_ms=enc,decode_ms=dec,combined_MBps=len(original)/(1000*(enc+dec)))
            rows.append(row)
            if writer is None:writer=csv.DictWriter(fd,lineterminator='\n',fieldnames=row); writer.writeheader()
            writer.writerow(row)
with (args.out/'flac-score.csv').open('w',newline='') as fd:
    writer=None
    for s in args.sources:
        for arm in ('K7','Kultra','F5','F8'):
            g=[x for x in rows if x['source']==s.name and x['arm']==arm and x['rep']>0]
            row=dict(source=s.name,arm=arm,stored=g[0]['stored'],encode_ms=statistics.median(x['encode_ms'] for x in g),decode_ms=statistics.median(x['decode_ms'] for x in g),combined_MBps=statistics.median(x['combined_MBps'] for x in g))
            if writer is None: writer=csv.DictWriter(fd,lineterminator='\n',fieldnames=row); writer.writeheader()
            writer.writerow(row)
