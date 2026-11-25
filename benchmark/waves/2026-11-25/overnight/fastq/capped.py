"""Exercise automatic block choice under an explicitly requested encoder memory cap, using the unchanged source slices."""
import csv,json,os,sys
from pathlib import Path
from fastq_panel import sha, offsets, inspect, timed, boundary_digest
out,scratch,experiment,stock,reader=map(Path,sys.argv[1:6])
manifest=json.loads((out/"inputs.json").read_text())
env=dict(os.environ);env.pop("KOLMO_BENCH_READ_CUTS",None)
larger="--larger" in sys.argv[6:]
capmeta={}
with (out/("capped-large.csv" if larger else "capped-small.csv")).open("w",newline="") as dest:
    fields=(out/"raw.csv").open().readline().strip().split(",")
    w=csv.writer(dest);w.writerow(fields+["encoder_cap_bytes"])
    for run,meta in manifest["inputs"].items():
        s=meta["slices"]["multi"]; p=scratch/s["file"]; z=scratch/"cap.kz"; d=scratch/"cap-restored"
        if larger:
            source=Path(meta["source_name"])
            assert sha(source.read_bytes())==meta["source_sha256"]
            limit=(8<<20)+(256<<10);payload=bytearray();num=0
            with source.open("rb") as f:
                while len(payload)<limit:
                    lines=[f.readline() for _ in range(4)]
                    assert all(x.endswith(b"\n") for x in lines) and lines[0].startswith(b"@"+run.encode()+b".") and lines[2].startswith(b"+")
                    assert len(lines[1].rstrip(b"\r\n"))==len(lines[3].rstrip(b"\r\n"))
                    payload.extend(b"".join(lines));num+=1
            p=scratch/(run+"-capped-large.fastq");p.write_bytes(payload)
            s=dict(sha256=sha(payload),input_bytes=len(payload),first_record=0,source_offset=0,records=num,record_boundaries_sha256=boundary_digest(offsets(payload)))
            capmeta[run]=s
            (out/"capped-large-inputs.json").write_text(json.dumps(capmeta,indent=2)+"\n")
        assert sha(p.read_bytes())==s["sha256"]
        exact=set(offsets(p.read_bytes()))
        args=["-q","-T1","-U","--memlimit","256M","-o",z,p]
        timed([stock,*args],env); ref=sha(z.read_bytes()); sc,_=inspect(z.read_bytes());z.unlink()
        trials=2 if larger else (3 if len(sc)>2 else 1)
        if larger: assert sc[1]==8<<20 and len(sc)==3
        for rnd in range(trials):
            for order,arm in enumerate(["aligned","fixed"] if rnd%2 else ["fixed","aligned"]):
                e=timed([experiment,*args],env if arm=="fixed" else {**env,"KOLMO_BENCH_READ_CUTS":"1"})
                de=timed([stock,"-q","-T1","-d","-o",d,z],env); ho=sha(d.read_bytes());d.unlink()
                frame=z.read_bytes();fh=sha(frame); cuts,codecs=inspect(frame)
                assert ho==s["sha256"] and cuts[-1]==s["input_bytes"] and (arm!="fixed" or fh==ref)
                aligned=sum(x in exact for x in cuts[1:-1]);assert arm!="aligned" or aligned==len(cuts)-2
                tag=""
                if not rnd:
                    timed([reader,"-q","-T1","-d","-o",d,z],env); tag=sha(d.read_bytes()); d.unlink();assert tag==ho
                w.writerow([run,"multi","U",rnd,order,arm,s["input_bytes"],ho,len(frame),fh,e,de,ho,len(codecs),"|".join(map(str,codecs)),
                            "|".join(map(str,cuts)),boundary_digest(cuts),aligned,tag,256<<20]);dest.flush();z.unlink()
                print(run,rnd,arm,cuts,len(frame),codecs,e,de,flush=True)
