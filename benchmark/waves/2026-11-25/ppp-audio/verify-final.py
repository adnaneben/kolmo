"""Replay every retained cell after the allocation-error fallback change."""
import argparse,csv,hashlib,os,random,struct,subprocess,tempfile,wave
from pathlib import Path
p=argparse.ArgumentParser()
for n in ('candidate','reader','results'): p.add_argument('--'+n,type=Path,required=True)
p.add_argument('source_dirs',nargs='+',type=Path)
a=p.parse_args()
paths={f.stem:f for d in a.source_dirs for f in d.glob('*.wav')}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
with tempfile.TemporaryDirectory() as t, (a.results/'final-equivalence.csv').open('w',newline='') as out:
    work=Path(t);w=csv.writer(out,lineterminator='\n')
    w.writerow(['source','level','threads','activated','exact_frame_sha256','exact_input_sha256'])
    gen=random.Random(66820)
    for n,s in [('tiny',[2000,-1800]*1024),('silence',[0]*32768),('repeated',[12000,-12000,500,-500,24,-24,2,-2]*4096),('noise',[gen.randrange(-32768,32768) for _ in range(32768)])]:
        f=work/(n+'.wav')
        with wave.open(str(f),'wb') as wf:
            wf.setparams((1,2,44100,0,'NONE',''));wf.writeframes(struct.pack('<%dh'%len(s),*s))
        paths[n]=f
    for csvpath in (a.results/'raw.csv',a.results/'speech/raw.csv',a.results/'frozen-recheck/raw.csv'):
        for r in csv.DictReader(csvpath.open()):
            if r['repeat']!='0' or r['arm']!='candidate':continue
            f=paths[r['source']];frame=work/'frame';decoded=work/'decoded'
            assert sha(f)==r['sha256']
            proc=subprocess.run([a.candidate,'--level',r['level'],'-T'+r['threads'],'-q','-f',f,'-o',frame],
                                env=dict(os.environ,KOLMO_AUDIO_SCREEN='1',KOLMO_AUDIO_SCREEN_TRACE='1'),capture_output=True,check=True)
            active=int(b'audio-screen accepted' in proc.stderr)
            assert active==int(r['accepted']) and sha(frame)==r['frame_sha256']
            subprocess.run([a.reader,'-d','-T'+r['threads'],'-q','-f',frame,'-o',decoded],check=True)
            assert sha(decoded)==r['sha256']
            w.writerow([r['source'],r['level'],r['threads'],active,r['frame_sha256'],r['sha256']])
