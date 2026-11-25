"""Repeat without --block-size, using the exact multi-block-experiment source slices."""
import csv,json,os,sys
from pathlib import Path
from fastq_panel import sha, offsets, inspect, timed, boundary_digest
out=Path(sys.argv[5]) if len(sys.argv)>5 else Path(__file__).resolve().parent
scratch,experiment,stock,reader=map(Path,sys.argv[1:5])
manifest=json.loads((out/"inputs.json").read_text())
env=dict(os.environ);env.pop("KOLMO_BENCH_READ_CUTS",None)
with (out/"automatic.csv").open("w",newline="") as dest:
    fields=(out/"raw.csv").open().readline().strip().split(",")
    w=csv.writer(dest);w.writerow(fields)
    for level in ["3","U"]:
        for run,meta in manifest["inputs"].items():
            sub=meta["slices"]["multi"];p=scratch/sub["file"];archive=scratch/"auto.kz";back=scratch/"auto-restored"
            assert sha(p.read_bytes())==sub["sha256"]
            cutset=set(offsets(p.read_bytes()))
            flags=["-q","-T1","-"+level,"-o",archive,p]
            timed([stock,*flags],env);reference=sha(archive.read_bytes());archive.unlink()
            for rnd in range(3):
                for order,arm in enumerate(["aligned","fixed"] if rnd%2 else ["fixed","aligned"]):
                    e=timed([experiment,*flags],env if arm=="fixed" else {**env,"KOLMO_BENCH_READ_CUTS":"1"})
                    d=timed([stock,"-q","-T1","-d","-o",back,archive],env)
                    h=sha(back.read_bytes());back.unlink();assert h==sub["sha256"]
                    data=archive.read_bytes();fhash=sha(data);ends,codecs=inspect(data)
                    assert arm!="fixed" or fhash==reference
                    n=sum(x in cutset for x in ends[1:-1])
                    assert ends[-1]==sub["input_bytes"] and (arm!="aligned" or level!="U" or n==len(ends)-2)
                    check=""
                    if rnd==0:
                        timed([reader,"-q","-T1","-d","-o",back,archive],env)
                        check=sha(back.read_bytes());back.unlink();assert check==h
                    w.writerow([run,"multi",level,rnd,order,arm,sub["input_bytes"],h,len(data),fhash,e,d,h,len(codecs),
                                "|".join(map(str,codecs)),"|".join(map(str,ends)),boundary_digest(ends),n,check])
                    dest.flush();archive.unlink()
