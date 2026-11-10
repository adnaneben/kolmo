"""Measure all other product modes on the fresh specialist inputs using exact file I/O."""
import argparse
import json
from pathlib import Path
from file_trials import run
from panel import sha
HERE=Path(__file__).resolve().parent

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("work",type=Path);a=p.parse_args()
    w=a.work.resolve();src=w/"holdouts";exe=w/"m1/kolmo"
    assert sha(exe)==json.loads((HERE/"m1-freeze.json").read_text())["binaries"]["candidate"]
    pins=json.loads((HERE/"holdout-inputs.json").read_text())
    pins.update(json.loads((HERE/"additional-inputs.json").read_text()))
    paths=dict(cellar=src/"cellar.pcm",bsub=src/"bsub.fa",akiyo=src/"akiyo.yuv",yeast=src/"yeast.fa",
               electricity=src/"cossmic/test.csv",reads=src/"reads.fastq",chr22=src/"chr22.fa")
    out=w/"mode-trials";out.mkdir(exist_ok=True)
    for k,file in paths.items():
        assert sha(file)==pins[k].get("target_sha256",pins[k].get("test_sha256"))
        commands={}
        for m in ("rt","fast","eco","hc","max","ultra"):
            commands["kolmo_"+m]=dict(encode=[str(exe),"-T","1","-m",m,"{src}","{frame}"],
                                      decode=[str(exe),"-T","1","-d","{frame}","{out}"])
        run(file,commands,w,out/(k+".json"),3)

if __name__=="__main__":
    main()
