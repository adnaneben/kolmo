"""Run the paired CLI experiment on macOS. Pass --help for paths and scopes."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import shutil
import subprocess
import tempfile
import time

SOURCES = {
    "energy": "corpus/energy_test.csv",
    "ghcn": "review40b/work/R21/data/ghcn_train.csv",
    "co2": "corpus-holdout/owid-co2-data.csv",
    "backblaze": "outreach-data/backblaze/2025-12-31.csv",
    "gaia": "outreach-data/gaia/GaiaSource_000000-003111.csv",
    "ghcn-tsv": "review40b/work/R34/sqs/ghcn.rows.tsv",
    "prose": "corpus/dickens",
    "audio": "corpus/esc50-1-100032-A-0.wav",
}

def sha(path):
    with path.open("rb") as f:
        digest = hashlib.sha256()
        for chunk in iter(lambda: f.read(1 << 20), b""):
            digest.update(chunk)
        return digest.hexdigest()

def run(argv):
    start = time.perf_counter_ns()
    proc = subprocess.Popen(list(map(str, argv)), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    _, status, ru = os.wait4(proc.pid, 0)
    elapsed = (time.perf_counter_ns() - start) / 1e9
    proc.returncode = os.waitstatus_to_exitcode(status)
    if proc.returncode:
        raise RuntimeError(f"exit {proc.returncode}: {argv}")
    return [elapsed, ru.ru_utime + ru.ru_stime, ru.ru_maxrss]

def main():
    p = argparse.ArgumentParser()
    p.add_argument("corpus", type=Path)
    p.add_argument("base", type=Path)
    p.add_argument("candidate", type=Path)
    p.add_argument("output", type=Path)
    p.add_argument("--files", nargs="+", choices=SOURCES, default=list(SOURCES))
    p.add_argument("--sizes", nargs="+", default=["256", "2048", "whole"], help="KiB or whole")
    p.add_argument("--threads", type=int, nargs="+", default=[1,4])
    p.add_argument("--levels", type=int, nargs="+", default=[1,2])
    p.add_argument("--rounds", type=int, default=7)
    p.add_argument("--rivals", action="store_true")
    a = p.parse_args()
    rng = random.Random(71521)
    with tempfile.TemporaryDirectory() as tmp, a.output.open("x") as out:
        tmp = Path(tmp)
        inp, frame, restored = (tmp / k for k in ("in","frame","readback"))
        for key in a.files:
            source = a.corpus / SOURCES[key]
            source_hash = sha(source)
            seen = set()
            for size in a.sizes:
                n = source.stat().st_size if size == "whole" else min(int(size) << 10,source.stat().st_size)
                if n in seen:
                    continue
                seen.add(n)
                with source.open("rb") as inf:
                    inp.write_bytes(inf.read(n))
                expected = sha(inp)
                for level in a.levels:
                    for threads in a.threads:
                        for rep in range(a.rounds+1):
                            labels = ["A","A2","B"]
                            # Competitors are unrelated to the named Kolmo level. Time them once per file size.
                            if a.rivals and level == a.levels[0] and threads == 1:
                                labels.extend(["lz4","zstd"])
                            rng.shuffle(labels)
                            for label in labels:
                                if label in ("A","A2","B"):
                                    exe = a.candidate if label == "B" else a.base
                                    enc = [exe,f"-{level}","-T",threads,"-f","-o",frame,inp]
                                    dec = [a.base,"-d","-T",threads,"-f","-o",restored,frame]
                                elif label == "lz4":
                                    exe = shutil.which("lz4")
                                    enc = [exe,"-1","-q","-T1","-f",inp,frame]
                                    dec = [exe,"-q","-d","-f",frame,restored]
                                else:
                                    exe = shutil.which("zstd")
                                    enc = [exe,"-1","-q","--single-thread","-f",inp,"-o",frame]
                                    dec = [exe,"-d","-q","--single-thread","-f",frame,"-o",restored]
                                e, ecpu, erss = run(enc)
                                d, dcpu, drss = run(dec)
                                rec = dict(file=key, source=SOURCES[key], file_sha256=source_hash,
                                    input_sha256=expected,n=n,whole=n==source.stat().st_size,
                                    level=level,threads=threads,rep=rep,arm=label,
                                    bytes=frame.stat().st_size,archive_sha256=sha(frame),
                                    restore_sha256=sha(restored),enc=e,dec=d,encode_cpu=ecpu,
                                    decode_cpu=dcpu,encode_rss=erss,decode_rss=drss,
                                    load1=os.getloadavg()[0],exe=str(exe))
                                out.write(json.dumps(rec)+"\n")
                                out.flush()
                                if rec["restore_sha256"] != expected:
                                    raise RuntimeError("readback mismatch")
if __name__ == "__main__":
    main()
