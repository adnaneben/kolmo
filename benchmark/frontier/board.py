"""Measure the board-linked whole Silesia tar across both pinned lzbench builds."""
import argparse
import csv
import fcntl
import io
import json
import os
from pathlib import Path
import random
import subprocess
import time
from panel import OPTIONS, MARKET, sha, write_json

BOARD_SHA = "a667bac80089b3d04d5496772149e632f0758974b0579d743cd8d6e5e0d63eb2"
# Expensive full-corpus rows run in a separate queue, after shorter comparisons.
SLOW = {"brotli11", "zpaq", "kolmo_ultra", "kanzi9", "zstd22", "lzma9", "brieflz5",
        "fastlzma2", "lizard39", "kolmo_max", "kbc", "pulsar", "ppmd", "kolmo_hc"}


def one(exe, flags, src, lock):
    with lock.open("a") as handle:
        fcntl.flock(handle, fcntl.LOCK_EX)
        start = time.time()
        proc = subprocess.run([exe,*flags,src],cwd=exe.parent,capture_output=True,text=True)
        elapsed=time.time()-start
    rows=list(csv.DictReader(io.StringIO(proc.stdout)))
    valid=(proc.returncode==0 and len(rows)==1 and int(rows[0]["Original size"])==src.stat().st_size
           and int(rows[0]["Compressed size"])>0
           and all(float(rows[0][k])>0 for k in ("Compression speed","Decompression speed")))
    receipt=dict(flags=flags,valid=valid,started=start,elapsed_s=elapsed,
                 code=proc.returncode,stderr=proc.stderr,raw=proc.stdout.replace(str(src),src.name))
    if valid:
        receipt.update(bytes=int(rows[0]["Compressed size"]),encode=float(rows[0]["Compression speed"]),
                       decode=float(rows[0]["Decompression speed"]))
    return receipt


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("work",type=Path)
    p.add_argument("source",type=Path)
    p.add_argument("queue",choices=["short","slow"])
    a=p.parse_args()
    work=a.work.resolve(); src=a.source.resolve()
    assert sha(src)==BOARD_SHA
    exe1=work/"frozen/lzbench"; exe2=work/"market-src/lzbench"
    base=json.loads((work/"manifest.json").read_text())
    extra=json.loads((work/"market-manifest.json").read_text())
    assert sha(exe1)==base["lzbench"] and sha(exe2)==extra["lzbench"]
    assert sha(exe1.parent/"build/libkolmo.dylib")==base["libkolmo"]
    assert sha(exe2.parent/extra["rust_path"])==extra["rust_sha256"]
    codecs=list(OPTIONS.items())+list(MARKET.items())
    random.Random(625194).shuffle(codecs)
    out=work/"board"; out.mkdir(exist_ok=True)
    for codec,setting in codecs:
        if (codec in SLOW)!=(a.queue=="slow"): continue
        dest=out/(codec+".json")
        if dest.exists(): continue
        exe=exe2 if codec in MARKET else exe1
        flags=["-e"+setting,"-I1","-T1","-t0,0","-i3,5","-p3","-o4","-q"]
        rec=one(exe,flags,src,work/"timing.lock")
        rec.update(codec=codec,source_sha256=BOARD_SHA)
        write_json(dest,rec)
        print(codec,rec["valid"],round(rec["elapsed_s"],2),flush=True)


if __name__=="__main__":
    main()
