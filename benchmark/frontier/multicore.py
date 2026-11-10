"""Compare fixed external chunk pools and a separate single-stream internal-worker regime."""
import argparse
import json
from pathlib import Path
import random
from board import one,BOARD_SHA
from panel import OPTIONS,MARKET,sha,write_json
HERE=Path(__file__).resolve().parent
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("work",type=Path);p.add_argument("reference",type=Path)
    a=p.parse_args();w=a.work.resolve();old=a.reference.resolve();data=old/"build/comparison/data"
    paths={"archive":old/"benchmark/ratio/data/silesia.tar","grid":w.parent/"fast-memory/test.csv",
           "video":data/"foreman.yuv","reads":w/"holdouts/reads.fastq","cossmic":w/"holdouts/cossmic/test.csv",
           "integers":data/"numbers.test","bsub":w/"holdouts/bsub.fa"}
    hashes={k:sha(f) for k,f in paths.items()};assert hashes["archive"]==BOARD_SHA
    dest=w/"multicore";dest.mkdir(exist_ok=True)
    pin=json.loads((HERE/"m1-freeze.json").read_text())["binaries"]
    exe=w/"m1/lzbench";assert sha(exe)==pin["harness"] and sha(exe.parent/"build/libkolmo.dylib")==pin["candidate_library"]
    market=json.loads((HERE/"market-manifest.json").read_text())
    exe2=w/"market-src/lzbench";assert sha(exe2)==market["lzbench"]
    assert sha(exe2.parent/market["rust_path"])==market["rust_sha256"]
    jobs=[]
    for name in paths:
        for mode in ("pool10","internal10"):
            settings=["kolmo_rt","kolmo_fast","zstd1","zstd3","zstd19"]
            if mode=="pool10":settings+=["lz4","openzl","snappy","lzav2","memlz","misa77","krc"]
            for k in settings:jobs.append((name,mode,k))
    random.Random(353211).shuffle(jobs)
    for name,mode,k in jobs:
        out=dest/(name+"."+mode+"."+k+".json")
        if out.exists():continue
        flags=(["-b1024","-T10","-I1"] if mode=="pool10" else ["-b0","-T1","-I10"])
        src=paths[name];binary=exe2 if k in MARKET else exe
        opts=MARKET if k in MARKET else OPTIONS
        args=["-e"+opts[k],*flags,"-t0,0","-i3,5","-p3","-o4","-q"]
        result=one(binary,args,src,w/"timing.lock")
        result.update(input=name,codec=k,regime=mode,original_bytes=src.stat().st_size,source_sha256=hashes[name])
        write_json(out,result)
        print(name,mode,k,result["valid"],flush=True)
if __name__=="__main__":main()
