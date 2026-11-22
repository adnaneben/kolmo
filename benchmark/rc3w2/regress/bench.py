# bench.py OUT.jsonl ROUNDS T "mode args" BIN:label ... -- FILES
# one process per compression and decompression, labels alternate order per round, fastest of ROUNDS, every output
# decoded and compared with the input; prints sizes (vs the first label) and times
import sys, os, subprocess, time, json
out=sys.argv[1]; R=int(sys.argv[2]); T=sys.argv[3]; margs=sys.argv[4].split()
i=sys.argv.index('--'); bins=[b.split(':') for b in sys.argv[5:i]]; files=sys.argv[i+1:]
W='/home/user/data/w'; os.makedirs(W,exist_ok=True)
def run(cmd):
    t=time.perf_counter(); subprocess.run(cmd,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); return time.perf_counter()-t
f=open(out,'a')
for fn in files:
  res={}
  for r in range(R):
    for b,lab in (bins if r%2==0 else bins[::-1]):
      o=f'{W}/{lab}.k'; back=f'{W}/{lab}.out'
      c=run([b]+margs+['-T',T,fn,o])
      d=run([b,'-d','-T',T,o,back])
      e=res.setdefault(lab,{'c':9e9,'d':9e9})
      e['c']=min(e['c'],c); e['d']=min(e['d'],d); e['size']=os.path.getsize(o)
      if open(back,'rb').read()!=open(fn,'rb').read(): print('MISMATCH',lab,fn); sys.exit(1)
      os.remove(o); os.remove(back)
  row={'file':os.path.basename(fn),'mode':' '.join(margs),'T':T,**{k:v for k,v in res.items()}}
  f.write(json.dumps(row)+'\n'); f.flush()
  base=bins[0][1]
  print(row['file'], ' '.join(f"{l}:{res[l]['size']}({res[l]['size']/res[base]['size']-1:+.2%}) c{res[l]['c']:.3f} d{res[l]['d']:.3f}" for _,l in bins), flush=True)
