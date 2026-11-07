"""Run the earlier KRC CPU/Metal implementation on the current benchmark inputs."""
import argparse
import csv
import fcntl
import io
import json
from pathlib import Path
import subprocess
from panel import sha, write_json


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ("work","exe","shader","archive","grid","video","integers"):
        p.add_argument(name,type=Path)
    a=p.parse_args()
    paths={k:getattr(a,k).resolve() for k in ("archive","grid","video","integers")}
    result=dict(binary_sha256=sha(a.exe),shader_sha256=sha(a.shader),
                threads=10,repeats=7,cpu_share_percent=50,samples={})
    for name,source in paths.items():
        flags=["-T","10","-r","7","-p","50","-G","-s",str(a.shader.resolve())]
        with (a.work/"timing.lock").open("a") as lock:
            fcntl.flock(lock,fcntl.LOCK_EX)
            proc=subprocess.run([a.exe.resolve(),*flags,source],text=True,capture_output=True)
        if proc.returncode:
            raise RuntimeError(proc.stderr)
        rows=list(csv.DictReader(io.StringIO(proc.stdout)))
        assert {r["codec"] for r in rows} >= {"krc-metal","krc-hybrid-50","krc-cpu"}
        assert all(int(r["verified"])==1 and int(r["input_bytes"])==source.stat().st_size for r in rows)
        krc=[r for r in rows if r["codec"].startswith("krc-")]
        assert len({r["compressed_bytes"] for r in krc})==1
        for row in rows: row["file"]=source.name
        result["samples"][name]=dict(sha256=sha(source),rows=rows,diagnostics=proc.stderr)
        write_json(a.work/"gpu.json",result)
        print(name,[(r["codec"],r["cpu_threads"],r["compress_mbps"]) for r in krc],flush=True)


if __name__=="__main__":
    main()
