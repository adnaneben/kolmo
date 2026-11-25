"""Benchmark opaque, complete four-line record slices. All stored receipts contain only hashes, sizes and timings."""
import argparse, csv, hashlib, json, os, struct, subprocess, time
from pathlib import Path

def sha(data):
    return hashlib.sha256(data).hexdigest()

def offsets(buf):
    out=[0]; pos=0
    while pos<len(buf):
        boundary=pos
        for _ in range(4):
            lf=buf.find(b"\n",boundary)
            if lf<0: raise ValueError("incomplete four-line record")
            boundary=lf+1
        out.append(boundary); pos=boundary
    return out

def boundary_digest(cuts):
    return sha(b"".join(struct.pack("<Q",x) for x in cuts))

def prepare(accession, source, dest):
    # The local files are retained public run prefixes. These IDs and complete byte spans authenticate the slices.
    full=source.read_bytes()
    with source.open("rb") as f:
        pos=0; index=0; spans={}
        for alias,first_read,minimum in [("multi",0,(2<<20)+(256<<10)),("single",20100,160<<10)]:
            while index<first_read:
                for _ in range(4): pos+=len(f.readline())
                index+=1
            start=pos; first=index
            while pos-start<minimum:
                group=[f.readline() for _ in range(4)]
                assert all(line.endswith(b"\n") for line in group)
                assert group[0].startswith(b"@"+accession.encode()+b".") and group[2].startswith(b"+")
                assert len(group[1].rstrip(b"\r\n"))==len(group[3].rstrip(b"\r\n"))
                pos+=sum(map(len,group));index+=1
            sub=full[start:pos];cut=offsets(sub)
            rel=f"{accession}-{alias}.fastq"; (dest/rel).write_bytes(sub)
            spans[alias]=dict(file=rel,first_record=first,records=index-first,source_offset=start,input_bytes=len(sub),
                              sha256=sha(sub),all_record_boundaries_sha256=boundary_digest(cut))
    return dict(source_name=str(source),source_bytes=len(full),source_sha256=sha(full),slices=spans)

def timed(cmd,env):
    t0=time.perf_counter_ns()
    subprocess.run(cmd,env=env,check=True,stdout=subprocess.DEVNULL)
    return time.perf_counter_ns()-t0

def inspect(frame):
    assert frame[:4]==b"CZN1" and frame[4]==12
    count=struct.unpack_from("<I",frame,20)[0]
    ends=[0];codecs=[]
    for i in range(count):
        ix=32+i*16
        ends.append(ends[-1]+struct.unpack_from("<I",frame,ix)[0])
        codecs.append(frame[ix+12])
    return ends,codecs

def main():
    ap=argparse.ArgumentParser()
    for arg in ("out","scratch","experiment","stock","reader","srr1770413","srr2584863"):
        ap.add_argument("--"+arg,required=True,type=Path)
    ap.add_argument("--source-commit", default="ac10ce80048ab0664074f7ba031634055f4d2225")
    a=ap.parse_args();a.out.mkdir(exist_ok=True,parents=True);a.scratch.mkdir(exist_ok=True,parents=True)
    manifest=dict(source_commit=a.source_commit,
                  patch_sha256=sha((a.out/"experiment.patch").read_bytes()),
                  binary_hashes={k:sha(p.read_bytes()) for k,p in [("experiment",a.experiment),("stock",a.stock),("tag_v002",a.reader)]},
                  boundary_encoding="little-endian 64-bit offsets, including zero and end", inputs={})
    for run,p in [("SRR1770413",a.srr1770413),("SRR2584863",a.srr2584863)]:
        manifest["inputs"][run]=prepare(run,p,a.scratch)
    (a.out/"inputs.json").write_text(json.dumps(manifest,indent=2)+"\n")
    fields="accession slice level round order arm raw_bytes sha256 stored_bytes stored_sha256 encode_ns stock_decode_ns restored_sha256 blocks codecs outer_offsets outer_offsets_sha256 cuts_on_record tagged_reader_restore".split()
    with (a.out/"raw.csv").open("w",newline="") as out:
        w=csv.writer(out);w.writerow(fields)
        for name in ["single","multi"]:
            for level in ["3","U"]:
                for run,meta in manifest["inputs"].items():
                    sub=meta["slices"][name]; p=a.scratch/sub["file"]; archive=a.scratch/"frame.kz"; back=a.scratch/"restored"
                    cutset=set(offsets(p.read_bytes()))
                    env=dict(os.environ);env.pop("KOLMO_BENCH_READ_CUTS",None)
                    flags=["-q","-T1","-"+level,"--block-size","1","-o",archive,p]
                    # Tie the fixed arm explicitly to the frozen stock binary before any timed comparisons.
                    timed([a.stock,*flags],env);control_hash=sha(archive.read_bytes());archive.unlink()
                    rounds=1 if name=="single" else 3
                    for rnd in range(rounds):
                        arms=["fixed","aligned"] if rnd%2==0 else ["aligned","fixed"]
                        for order,arm in enumerate(arms):
                            encenv=env if arm=="fixed" else {**env,"KOLMO_BENCH_READ_CUTS":"1"}
                            e=timed([a.experiment,*flags],encenv); dec=timed([a.stock,"-q","-T1","-d","-o",back,archive],env)
                            frame=archive.read_bytes();h=sha(frame);rh=sha(back.read_bytes());back.unlink()
                            assert rh==sub["sha256"] and (arm!="fixed" or h==control_hash)
                            ends,codecs=inspect(frame);assert ends[-1]==sub["input_bytes"]
                            aligned_n=sum(x in cutset for x in ends[1:-1])
                            if arm=="aligned" and level=="U":assert aligned_n==len(ends)-2
                            if level=="3":assert h==control_hash
                            check=""
                            if rnd==0:
                                timed([a.reader,"-q","-T1","-d","-o",back,archive],env); check=sha(back.read_bytes());back.unlink();assert check==rh
                            w.writerow([run,name,level,rnd,order,arm,sub["input_bytes"],rh,len(frame),h,e,dec,rh,len(codecs),
                                        "|".join(map(str,codecs)),"|".join(map(str,ends)),boundary_digest(ends),aligned_n,check])
                            out.flush();archive.unlink()
                            print(run,name,level,rnd,arm,len(frame),ends,codecs,e,dec,flush=True)

if __name__=="__main__":main()
