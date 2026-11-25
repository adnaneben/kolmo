"""Compare historical, qualified and integrated builds using their ordinary CLI paths."""
import argparse,csv,hashlib,json,os,random,shutil,subprocess,tempfile,time
from collections import Counter,defaultdict
from pathlib import Path
def sha(v):return hashlib.sha256(v).hexdigest()
def timed(argv,cwd):
    start=time.monotonic_ns()
    child=subprocess.Popen([str(x) for x in argv],cwd=cwd,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    _,status,ru=os.wait4(child.pid,0); elapsed=time.monotonic_ns()-start
    child.returncode=os.waitstatus_to_exitcode(status)
    if child.returncode:raise RuntimeError((argv,status))
    return elapsed,ru.ru_utime+ru.ru_stime,ru.ru_maxrss
def main():
    p=argparse.ArgumentParser()
    for fld in ['before','main','combined','archive','csv','prose','out']:p.add_argument('--'+fld,type=Path,required=True)
    p.add_argument('--rounds',type=int,default=11);a=p.parse_args()
    arm={'pre':a.before.resolve(),'main':a.main.resolve(),'main-AA':a.main.resolve(),'combined':a.combined.resolve()}
    original={'enwik8.zip':a.archive.read_bytes(),'owid-co2.csv':a.csv.read_bytes(),'prose-64k':a.prose.read_bytes()}
    raw={k:(v[:65536] if k=='prose-64k' else v) for k,v in original.items()}
    source_meta={k:{'source_sha256':sha(v),'source_bytes':len(v),'input_sha256':sha(raw[k]),'input_bytes':len(raw[k])} for k,v in original.items()}
    settings=[(s,l,t) for s,lvls in [('enwik8.zip',['1']),('owid-co2.csv',['1','2']),('prose-64k',['3','9','ultra'])] for l in lvls for t in [1,4]]
    manifest={'binaries':{k:sha(v.read_bytes()) for k,v in arm.items()},'sources':source_meta,'randomization_seed':2026112508,'rounds':a.rounds,'settings':settings,'archive_mode_and_mtime':[420,1600000000],'cross_reader': 'untimed first-pass qualified main decoding all arms per setting'}
    (a.out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    rng=random.Random(manifest['randomization_seed'])
    with tempfile.TemporaryDirectory() as name,(a.out/'raw.csv').open('x',newline='',buffering=1) as f:
        d=Path(name);restore=d/'restore';restore.mkdir()
        for fn,data in raw.items():
            fn=d/fn;fn.write_bytes(data);fn.chmod(0o644);os.utime(fn,(1600000000,1600000000))
        writer=None
        for rep in range(a.rounds+1):
            cells=list(settings);rng.shuffle(cells)
            for source,level,threads in cells:
                labels=list(arm);rng.shuffle(labels)
                for order,label in enumerate(labels):
                    exe=arm[label];fr=d/'frame';arc=source=='enwik8.zip'
                    restored=restore/source if arc else restore/'out'
                    enc=[exe,'archive','-1','-T',threads,fr,source] if arc else [exe,'--level',level,'-T',threads,'-q','-f',source,'-o',fr]
                    dec=[exe,'extract','-T',threads,'-C',restore,fr] if arc else [exe,'-d','-T',threads,'-q','-f',fr,'-o',restored]
                    en=timed(enc,d);de=timed(dec,d)
                    assert restored.read_bytes()==raw[source]
                    if arc:
                        stat=restored.stat();assert stat.st_mode&0o777==0o644 and int(stat.st_mtime)==1600000000
                    restored.unlink()
                    if rep==0:
                        dec[0]=arm['main'];timed(dec,d);assert restored.read_bytes()==raw[source];restored.unlink()
                    v=fr.read_bytes();fr.unlink()
                    tags={} if arc else dict(Counter(v[44+16*i]&63 for i in range(int.from_bytes(v[20:24],'little'))))
                    r={'source':source,'level':level,'threads':threads,'repeat':rep,'order':order,'arm':label,'input_bytes':len(raw[source]),'sha256':source_meta[source]['input_sha256'],'stored_bytes':len(v),'frame_sha256':sha(v),'codecs':json.dumps(tags),'encode_ns':en[0],'decode_ns':de[0],'encode_cpu_s':en[1],'decode_cpu_s':de[1],'encode_rss_B':en[2],'decode_rss_B':de[2],'load1':os.getloadavg()[0]}
                    if not writer:writer=csv.DictWriter(f,fieldnames=r);writer.writeheader()
                    writer.writerow(r)
            print('pass',rep,'complete',flush=True)
if __name__=='__main__':main()
