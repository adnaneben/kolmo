"""Randomized paired same-preset full-frame PCM comparisons."""
import argparse
import csv
import hashlib
import os
from pathlib import Path
import random
import statistics
import struct
import subprocess
import tempfile
import time
import wave

def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def run(cmd, env):
    start=time.perf_counter_ns()
    p=subprocess.run(cmd,env=env,capture_output=True,check=True)
    return (time.perf_counter_ns()-start)/1e6,p.stderr
ap=argparse.ArgumentParser()
for arg in ('main','candidate','released','esc','apple','out'):
    ap.add_argument('--'+arg,type=Path,required=True)
ap.add_argument("--speech-only",action="store_true")
a=ap.parse_args()
a.out.mkdir(exist_ok=True,parents=True)
rng=random.Random(49230)
sources={p.stem:('development',p) for p in sorted(a.esc.glob('esc50*.wav'))}
assert len(sources)==4
sources.update({p.stem:('transfer',p) for p in sorted(a.apple.glob('*.wav'))})
rows=[]
with tempfile.TemporaryDirectory(prefix='kolmo-aud-') as d:
    w=Path(d); rgen=random.Random(66820)
    for name,samples in [('silence',[0]*32768),('repeated',[12000,-12000,500,-500,24,-24,2,-2]*4096),
                         ('noise',[rgen.randrange(-32768,32768) for _ in range(32768)]),('tiny',[2000,-1800]*1024)]:
        p=w/(name+'.wav')
        with wave.open(str(p),'wb') as fd:
            fd.setparams((1,2,44100,0,'NONE',''));fd.writeframes(struct.pack('<%dh'%len(samples),*samples))
        sources[name]=('control',p)
    if a.speech_only:
        sources = {p.stem: ("speech-transfer",p) for p in sorted(a.apple.glob("*.wav"))}
    with (a.out/'raw.csv').open('w',newline='',buffering=1) as f:
        writer=None
        for rep in range(6):
            cells=[(n,t,l) for n in sources for t in (1,4) for l in ('7','ultra')]
            rng.shuffle(cells)
            for name,nt,level in cells:
                family,p=sources[name]
                original=digest(p)
                arms=['main','candidate']
                if rep==0: arms.append('disabled')
                rng.shuffle(arms)
                for arm in arms:
                    frame=w/'frame'; out=w/'out'; exe=a.main if arm=='main' else a.candidate
                    env=dict(os.environ,KOLMO_AUDIO_SCREEN=str(int(arm=='candidate')))
                    env.pop('KOLMO_AUDIO_SCREEN_TRACE',None)
                    if rep==0: env['KOLMO_AUDIO_SCREEN_TRACE']='1'
                    enc,err=run([exe,'--level',level,f'-T{nt}','-q','-f',p,'-o',frame],env)
                    dec,_=run([a.released,'-d',f'-T{nt}','-q','-f',frame,'-o',out],env)
                    if digest(out)!=original: raise ValueError('not lossless')
                    b=frame.read_bytes()
                    row=dict(source=name,family=family,level=level,threads=nt,repeat=rep,arm=arm,
                             raw=p.stat().st_size,sha256=original,stored=len(b),frame_sha256=digest(frame),
                             header=b[:32].hex(),accepted=int(b'audio-screen accepted' in err) if not rep else '',
                             encode_ms=enc,release_decode_ms=dec,combined_MBps=p.stat().st_size/(1000*(enc+dec)))
                    rows.append(row)
                    if not writer:writer=csv.DictWriter(f,fieldnames=row);writer.writeheader()
                    writer.writerow(row)
            print('finished',rep,flush=True)
with (a.out/'score.csv').open('w',newline='') as f:
    writer=None
    for name in sources:
        for nt in (1,4):
            for level in ('7','ultra'):
                group=[r for r in rows if (r['source'],r['threads'],r['level'])==(name,nt,level)]
                arm=lambda n:[r for r in group if r['arm']==n]
                base,cand,off=arm('main'),arm('candidate'),arm('disabled')[0]
                if base[0]['frame_sha256']!=off['frame_sha256']: raise ValueError('disabled mismatch')
                gain=[c['combined_MBps']/b['combined_MBps'] for b,c in zip(base[1:],cand[1:])]
                r=dict(source=name,family=sources[name][0],level=level,threads=nt,accepted=cand[0]['accepted'],
                       baseline_bytes=base[0]['stored'],candidate_bytes=cand[0]['stored'],
                       identical=all(b['frame_sha256']==c['frame_sha256'] for b,c in zip(base,cand)),
                       before_encode_ms=statistics.median(x['encode_ms'] for x in base[1:]),
                       after_encode_ms=statistics.median(x['encode_ms'] for x in cand[1:]),
                       before_decode_ms=statistics.median(x['release_decode_ms'] for x in base[1:]),
                       after_decode_ms=statistics.median(x['release_decode_ms'] for x in cand[1:]),
                       before_combined_MBps=statistics.median(x['combined_MBps'] for x in base[1:]),
                       after_combined_MBps=statistics.median(x['combined_MBps'] for x in cand[1:]),
                       paired_speedup=statistics.median(gain),wins=sum(t>1 for t in gain),pairs=len(gain))
                if not writer:writer=csv.DictWriter(f,fieldnames=r);writer.writeheader()
                writer.writerow(r)
