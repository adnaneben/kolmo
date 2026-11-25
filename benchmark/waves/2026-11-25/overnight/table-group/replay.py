"""Compare production grouping and a single isolated no-group switch using source-verified frames."""
import argparse,csv,hashlib,json,os,random,re,subprocess,tempfile,time
from pathlib import Path
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def timed(cmd,env):
    start=time.monotonic_ns()
    result=subprocess.run(cmd,env=env,capture_output=True,check=True,text=True)
    return time.monotonic_ns()-start,result.stderr
def main():
    ap=argparse.ArgumentParser()
    for name in ['source','bin','reader','owid','noaa','out']:
        ap.add_argument('--'+name,type=Path,required=True)
    a=ap.parse_args();stock=a.source/'build/kolmo'
    inputs={'owid':a.owid,'noaa':a.noaa,'source-control':a.source/'src/core/pool.c'}
    sig={name:sha(p) for name,p in inputs.items()}
    base=os.environ.copy();base.pop('KOLMO_B40_TRACE',None);base.pop('KOLMO_B40_NO_GROUP',None)
    meta={'commit':subprocess.check_output(['git','-C',a.source,'rev-parse','HEAD'],text=True).strip(),'sources':{k:{'path':str(p),'bytes':p.stat().st_size,'sha256':sig[k]} for k,p in inputs.items()},
          'original_table_c_sha256':sha(a.source/'src/transforms/table.c'),'experiment_sha256':sha(a.bin),'stock_sha256':sha(stock),'stock_reader_sha256':sha(a.reader),'preset':3,'thread_counts':[1,4],'seed':251140,'scored_trials':3}
    (a.out/'manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
    rng=random.Random(meta['seed']);cases=[(n,nt) for n in inputs for nt in (1,4)]
    with tempfile.TemporaryDirectory(dir=a.out) as temp,(a.out/'raw.csv').open('x',newline='') as f:
        t=Path(temp);frame=t/'frame';reference=t/'reference';restored=t/'restored'
        w=csv.writer(f);w.writerow('source threads trial arm input_B frame_B frame_sha256 restored_sha256 encode_ns decode_ns'.split())
        diagnostics={}
        for trial in range(4):
            rng.shuffle(cases)
            for name,nt in cases:
                enc=['-3','-T',str(nt),'-f','-q',inputs[name],'-o',frame]
                dec=['-d','-T',str(nt),'-f','-q',frame,'-o',restored]
                for arm in rng.sample(['chosen','no-group'],2):
                    env=base.copy()
                    if arm=='no-group':env['KOLMO_B40_NO_GROUP']='1'
                    en,_=timed([a.bin,*enc],env)
                    de,_=timed([a.reader,*dec],base)
                    actual=sha(restored);assert actual==sig[name]
                    framehash=sha(frame);w.writerow([name,nt,trial,arm,inputs[name].stat().st_size,frame.stat().st_size,framehash,actual,en,de]);f.flush()
                    if not trial:
                        observed=frame.read_bytes();nb=int.from_bytes(observed[20:24],'little')
                        _,trace=timed([a.bin,*dec],{**env,'KOLMO_B40_TRACE':'1'})
                        assert sha(restored)==sig[name]
                        hdr={'blocks':nb,'tflags':[(observed[32+16*i+13]) for i in range(nb)],'wire':[dict(zip(['columns','rows','group','groups','grouped_predictors','formulas'],map(int,m))) for m in re.findall(r'B40 wire cols=(\d+) rows=(\d+) group=(-?\d+) groups=(\d+) grouped_modes=(\d+) formulas=(\d+)',trace)]}
                        assert arm!='no-group' or all(cell['group']==-1 for cell in hdr['wire'])
                        if arm=='chosen':
                            timed([stock,*enc[:-1],reference],base)
                            assert framehash==sha(reference)
                        diagnostics[f'{name}/{nt}/{arm}']=hdr
            print('pass',trial,'complete',flush=True)
        (a.out/'wire.json').write_text(json.dumps(diagnostics,indent=2)+'\n')
if __name__=='__main__':main()
