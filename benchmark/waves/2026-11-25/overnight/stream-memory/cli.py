"""Measure the stock CLI, including its input-dispatch policy, in a separate process."""
import argparse,csv,hashlib,os,random,subprocess,tempfile,time
from pathlib import Path
def sha(p):
    with p.open('rb') as f:
        h=hashlib.sha256()
        for b in iter(lambda:f.read(1<<20),b''):h.update(b)
        return h.hexdigest()
def main():
    ap=argparse.ArgumentParser()
    for n in ['binary','csv','zip','out']:ap.add_argument('--'+n,type=Path,required=True)
    a=ap.parse_args();rng=random.Random(2026112511)
    with tempfile.TemporaryDirectory(dir=a.out) as td,(a.out/'cli.csv').open('x',newline='') as raw:
        td=Path(td);cases=[];out=td/'restored';meta={}
        w=csv.writer(raw);w.writerow('source format input_kind threads repeat frame_bytes frame_sha256 restored_sha256 rss_peak_B elapsed_ns'.split())
        for name,source,level in [('ghcn',a.csv,3),('silesia-zip',a.zip,1)]:
            for fmt,extra in [('frame',[]),('stream',['--stream=2'])]:
                f=td/(name+'.'+fmt)
                subprocess.run([a.binary,'--level',str(level),'--block-size','1','-T','1','-q','-f',*extra,source,'-o',f],check=True)
                meta[name,fmt]=(f,sha(f),sha(source))
                for kind in ['regular','pipe']: cases.append((name,fmt,kind))
        for rep in range(3):
            rng.shuffle(cases)
            for name,fmt,kind in cases:
                f,fsha,original=meta[name,fmt]
                with f.open('rb') as data:
                    feeder=None
                    if kind=='pipe':feeder=subprocess.Popen(['/bin/cat'],stdin=data,stdout=subprocess.PIPE)
                    ap=[str(a.binary),'-d','-q','-f','-T','4',str(f) if kind=='regular' else '-','-o',str(out)]
                    t=time.monotonic_ns()
                    child=subprocess.Popen(ap,stdin=feeder.stdout if feeder else subprocess.DEVNULL)
                    _,status,ru=os.wait4(child.pid,0);dt=time.monotonic_ns()-t
                    child.returncode=os.waitstatus_to_exitcode(status);assert child.returncode==0
                    if feeder:feeder.stdout.close();assert feeder.wait()==0
                restored=sha(out);assert restored==original
                w.writerow([name,fmt,kind,4,rep,f.stat().st_size,fsha,restored,ru.ru_maxrss,dt]);raw.flush()
if __name__=='__main__':main()
