"""Recheck earlier controls and released frames before promoting a candidate."""
import argparse
import fcntl
import json
from pathlib import Path
import subprocess
import tempfile
from file_trials import run
from panel import sha,write_json

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]


def commands(exe):
    return {"encode":[str(exe),"-T","1","-m","fast","{src}","{frame}"],
            "decode":[str(exe),"-T","1","-d","{frame}","{out}"]}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("work",type=Path)
    p.add_argument("reference",type=Path)
    a=p.parse_args()
    work=a.work.resolve();old=a.reference.resolve()
    data=old/"build/comparison/data";control=old/"build/holdout"
    earlier=HERE.parent/"holdout"
    previous=json.loads((earlier/"release-runs.json").read_text())
    inputs=json.loads((earlier/"inputs.json").read_text())
    held=json.loads((earlier/"holdout-runs.json").read_text())
    sources=dict(archive=old/"benchmark/ratio/data/silesia.tar",tables=data/"table.test.csv",numbers=data/"numbers.test",
                 genome=data/"ecoli.fa",image=control/"astronaut.rgb",audio=data/"digits.pcm",video=data/"foreman.yuv")
    expected={k:(previous[k+"/fast"]["input_sha256"],previous[k+"/fast"]["frames"]["kolmo"]["sha256"]) for k in sources}
    for k,v in inputs.items():
        sources[k]=control/(k+".test")
        row=next(t for t in held[k]["trials"] if t["codec"]=="kolmo")
        expected[k]=(v["test_sha256"],row["encoded_sha256"])
    base=ROOT/"build/wide/candidate";candidate=work/"m1/kolmo";normal=ROOT/"build/kolmo"
    release=control/"frozen/release"
    hashes=json.loads((HERE/"m1-freeze.json").read_text())["binaries"]
    for k,exe in (("candidate",candidate),("baseline",base),("release",release)):
        assert sha(exe)==hashes[k]
    dest=work/"controls";dest.mkdir(exist_ok=True)
    cmds=dict(candidate=commands(candidate),baseline=commands(base))
    compat_path=dest/"compatibility.json"
    compat=json.loads(compat_path.read_text()) if compat_path.exists() else {}
    for k,source in sources.items():
        target,oldframe=expected[k]
        assert sha(source)==target
        if k not in compat:
            with (work/"timing.lock").open("a") as lock,tempfile.TemporaryDirectory(dir=work) as tmp:
                fcntl.flock(lock,fcntl.LOCK_EX)
                frame=Path(tmp)/"frame";out=Path(tmp)/"out"
                subprocess.run([normal,"-T","1","-m","fast",source,frame],check=True,capture_output=True)
                normal_digest=sha(frame)
                assert normal_digest==oldframe,(k,"production frame changed")
                subprocess.run([candidate,"-d","-T","1",frame,out],check=True,capture_output=True)
                assert sha(out)==target
                frame.unlink();out.unlink()
                subprocess.run([candidate,"-T","1","-m","fast",source,frame],check=True,capture_output=True)
                with frame.open("rb") as f: ver=f.read(5)[4]
                chunk=min(65536,source.stat().st_size//3)
                offset=max(0,source.stat().st_size//2-7)
                subprocess.run([candidate,"-r",str(offset)+":"+str(chunk),frame,out],check=True,capture_output=True)
                with source.open("rb") as f:
                    f.seek(offset);assert out.read_bytes()==f.read(chunk)
                out.unlink()
                legacy=subprocess.run([release,"-T","1","-d",frame,out],capture_output=True)
                if ver<6: assert not legacy.returncode and sha(out)==target
                else: assert legacy.returncode
                compat[k]=dict(source_sha256=target, production_frame_sha256=normal_digest,
                               candidate_frame_sha256=sha(frame), candidate_frame_version=ver,
                               production_binary_sha256=sha(normal), legacy_decoder_exit=legacy.returncode,
                               range_offset=offset,range_length=chunk)
                write_json(compat_path,compat)
        run(source,cmds,work,dest/(k+".json"))

if __name__=="__main__":
    main()
