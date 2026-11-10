"""Replay the pinned fast candidate and slice leaders on complete inputs."""
import argparse
import json
from pathlib import Path
import random
import tarfile
import shutil

from board import one, BOARD_SHA
from panel import OPTIONS, MARKET, MEMBERS, sha, write_json


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("stage",choices=["candidate","full"])
    p.add_argument("work",type=Path)
    p.add_argument("reference",type=Path)
    a=p.parse_args()
    work=a.work.resolve()
    old=a.reference.resolve()
    if a.stage=="full":
        expected=set(MARKET)|set(OPTIONS)
        observed={}
        for row in (work/"observations").glob("*.json"):
            name,codec=row.stem.split(".",1)
            observed.setdefault(name,set()).add(codec)
        ready=[name for name,done in observed.items() if done>=expected]
        completed=work/"full-inputs"
        if not any(not (completed/(name+".finished")).exists() for name in ready):
            return
    h=Path(__file__).resolve().parent
    exe1=work/"frozen/lzbench";exe2=work/"market-src/lzbench";cand=work/"m1/lzbench"
    manifests=[json.loads((h/name).read_text()) for name in ("panel-manifest.json","market-manifest.json","m1-freeze.json")]
    panel,market,freeze=manifests
    assert sha(exe1)==panel["lzbench"] and sha(exe2)==market["lzbench"]
    assert sha(exe1.parent/"build/libkolmo.dylib")==panel["libkolmo"]
    assert sha(exe2.parent/market["rust_path"])==market["rust_sha256"]
    assert sha(cand)==freeze["binaries"]["harness"]
    assert sha(cand.parent/"build/libkolmo.dylib")==freeze["binaries"]["candidate_library"]
    tar=old/"benchmark/ratio/data/silesia.tar"
    assert sha(tar)==BOARD_SHA
    jobs=[]
    if a.stage=="candidate":
        jobs=[(n,work/"samples"/n,"m1_fast") for n in panel["samples"]]
        jobs.append(("whole-silesia",tar,"m1_fast"))
        dest=work/"m1-panel"
    else:
        dest=work/"full-inputs"
        folder=work/"members";folder.mkdir(exist_ok=True)
        with tarfile.open(tar,"r:") as ar:
            for name in MEMBERS:
                target=folder/name
                member=ar.getmember(name)
                if target.exists() and target.stat().st_size==member.size:
                    continue
                with ar.extractfile(member) as src,target.open("wb") as out:
                    shutil.copyfileobj(src,out)
        data=old/"build/comparison/data"
        paths={name:folder/name for name in MEMBERS}
        paths.update(fasta=data/"ecoli.fa",pcm=data/"digits.pcm",rgb=old/"build/holdout/astronaut.rgb",
                     yuv=data/"foreman.yuv",integers=data/"numbers.test",co2=data/"table.test.csv",
                     grid=work.parent/"fast-memory/test.csv",png=data/"astronaut.png",
                     entropy=work/"samples/entropy")
        rivals=(set(OPTIONS)|set(MARKET))-{k for k in OPTIONS if k.startswith("kolmo_") or k in ("kbc","krc")}
        for n,path in paths.items():
            if n not in ready or (dest/(n+".finished")).exists():
                continue
            files={k:work/"observations"/f"{n}.{k}.json" for k in rivals}
            if not all(f.exists() for f in files.values()):
                print("Awaiting remaining slice measurements for",n,flush=True)
                continue
            receipts={k:json.loads(f.read_text()) for k,f in files.items()}
            valid={k:v for k,v in receipts.items() if v["valid"]}
            selections={"m1_fast","kolmo_fast","zstd3"}
            for key,op in (("bytes",min),("encode",max),("decode",max)):
                best=op(v[key] for v in valid.values())
                selections.update(k for k,v in valid.items() if v[key]==best)
            jobs.extend((n,path,k) for k in sorted(selections))
    dest.mkdir(exist_ok=True)
    random.Random(318041).shuffle(jobs)
    hashes={}
    for name,path,codec in jobs:
        out=dest/f"{name}.{codec}.json"
        if out.exists(): continue
        if name not in hashes: hashes[name]=sha(path)
        if a.stage=="candidate":
            assert hashes[name]==(BOARD_SHA if name=="whole-silesia" else panel["samples"][name]["sha256"])
        exe=cand if codec=="m1_fast" else exe2 if codec in MARKET else exe1
        setting="kolmo,1" if codec=="m1_fast" else (MARKET if codec in MARKET else OPTIONS)[codec]
        flags=["-e"+setting,"-I1","-T1","-t0,0","-i3,5","-p3","-o4","-q"]
        r=one(exe,flags,path,work/"timing.lock")
        r.update(input=name,codec=codec,original_bytes=path.stat().st_size,source_sha256=hashes[name])
        write_json(out,r)
        print(name,codec,r["valid"],flush=True)
    if a.stage=="full":
        for name in ready:
            if all((dest/f"{n}.{k}.json").exists() for n,path,k in jobs if n==name):
                (dest/(name+".finished")).touch()

if __name__=="__main__":
    main()
