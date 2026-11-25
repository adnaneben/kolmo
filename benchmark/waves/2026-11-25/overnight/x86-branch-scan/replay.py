"""Benchmark the two x86-data scans and verify original plus stock CLI frame parity."""
import argparse,csv,hashlib,json,os,random,shutil,subprocess,tempfile,time
from pathlib import Path
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(cmd):
 t=time.monotonic_ns();subprocess.run(cmd,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE);return time.monotonic_ns()-t
def main():
 ap=argparse.ArgumentParser()
 for n in ['source','build','out','reader']:ap.add_argument('--'+n,required=True,type=Path)
 ap.add_argument('--phase',choices=['micro','cli'],required=True);a=ap.parse_args();a.out.mkdir(exist_ok=True,parents=True)
 with tempfile.TemporaryDirectory(dir=a.out) as dirname:
  d=Path(dirname);s={}
  for name,source,arch in [('sqlite-x86','/usr/bin/sqlite3','x86_64'),('awk-x86','/usr/bin/awk','x86_64'),('diff-x86','/usr/bin/diff','x86_64'),('sqlite-arm','/usr/bin/sqlite3','arm64e')]:
   dest=d/name;subprocess.run(['/usr/bin/lipo',source,'-thin',arch,'-output',dest],check=True)
   s[name]={'source':source,'original_sha256':sha(Path(source)),'slice':arch,'input_sha256':sha(dest),'input_bytes':dest.stat().st_size}
  code=a.source/'src/transforms/x86.c';shutil.copy(code,d/'c-source');s['c-source']={'source':'src/transforms/x86.c','original_sha256':sha(code),'slice':'whole','input_sha256':sha(code),'input_bytes':code.stat().st_size}
  old=a.source/'build/kolmo';new=a.build/'kolmo'
  if a.phase=='micro':
   with (a.out/'micro.csv').open('x') as f:
    f.write('source,round,order,arm,input_B,changed_bytes,iterations,encode_ns,decode_ns\n');f.flush()
    for name in s:subprocess.run([a.build/'micro',name,d/name],stdout=f,check=True)
   return
  data={'commit':subprocess.check_output(['git','-C',a.source,'rev-parse','HEAD'],text=True).strip(),'sources':s,'binaries':{'baseline':sha(old),'candidate':sha(new),'stock_reader':sha(a.reader),'micro':sha(a.build/'micro')},'seed':251121,'native_arch':os.uname().machine,'scored_trials':9}
  (a.out/'manifest.json').write_text(json.dumps(data,indent=2)+'\n')
  wire={};rng=random.Random(data['seed'])
  with (a.out/'cli.csv').open('x',newline='') as f:
   w=csv.writer(f);w.writerow('source level threads round order arm input_B frame_B frame_sha256 restored_sha256 encode_ns decode_ns'.split())
   trials=[(name,level,nt) for name in s for level in ([3,7] if name in ('sqlite-x86','awk-x86') else [3]) for nt in ([1,4] if name=='sqlite-x86' else [1])]
   for rep in range(10):
    rng.shuffle(trials)
    for name,level,nt in trials:
     for order,arm in enumerate(rng.sample(['A','AA','B'],3)):
      bin=new if arm=='B' else old;frame=d/'compressed';rest=d/'restored'
      e=run([bin,'--level',str(level),'-T',str(nt),'-q','-f',d/name,'-o',frame])
      dec=['-d','-T',str(nt),'-q','-f',frame,'-o',rest]
      dt=run([bin,*dec]);assert sha(rest)==s[name]['input_sha256']
      if not rep:
       run([a.reader,*dec]);assert sha(rest)==s[name]['input_sha256']
      z=frame.read_bytes();nb=int.from_bytes(z[20:24],'little');digest=hashlib.sha256(z).hexdigest()
      key=f'{name}/{level}/{nt}';entry={'sha256':digest,'frame_B':len(z),'blocks':[{'raw_B':int.from_bytes(z[32+i*16:36+i*16],'little'),'transform_flag':z[32+i*16+13]} for i in range(nb)]}
      if key in wire:assert wire[key]==entry
      wire[key]=entry
      w.writerow([name,level,nt,rep,order,arm,s[name]['input_bytes'],len(z),digest,sha(rest),e,dt]);f.flush()
    print('round',rep,'complete',flush=True)
  (a.out/'wire.json').write_text(json.dumps(wire,indent=2)+'\n')
if __name__=='__main__':main()
