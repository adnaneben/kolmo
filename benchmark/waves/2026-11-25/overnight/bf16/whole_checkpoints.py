"""Run the stock CLI and real ZipNN on existing full models, serially. Requires macOS for RSS units."""
import argparse
import collections
import csv
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def file_hash(path):
    dig = hashlib.sha256()
    with path.open("rb") as f:
        while chunk := f.read(4 << 20):
            dig.update(chunk)
    return dig.hexdigest()


def zip_phase(mode, source, target):
    from zipnn import ZipNN
    start = time.perf_counter_ns()
    z = ZipNN(input_format="byte", bytearray_dtype="bfloat16", threads=1) if mode == "c" else ZipNN(threads=1)
    data = Path(source).read_bytes()
    output = z.compress(data) if mode == "c" else z.decompress(data)
    with Path(target).open("wb") as f:
        f.write(memoryview(output))
    print(time.perf_counter_ns() - start)
    # data stays private to this short-lived phase; decompressed files and original hashes are checked separately.


if sys.argv[1:2] == ["--zip-phase"]:
    zip_phase(*sys.argv[2:])
    sys.exit(0)


def timed(cmd, env, metrics):
    with metrics.open("wb") as out:
        start = time.perf_counter_ns()
        proc = subprocess.Popen(cmd, env=env, stdout=out)
        _, status, usage = os.wait4(proc.pid, 0)
        elapsed = time.perf_counter_ns() - start
        proc.returncode = os.waitstatus_to_exitcode(status)
        if proc.returncode:
            raise subprocess.CalledProcessError(proc.returncode, cmd)
    text = metrics.read_text().strip()
    return elapsed, usage.ru_maxrss, int(text) if text.isdecimal() else None


def layout(path):
    with path.open("rb") as f:
        header_len = int.from_bytes(f.read(8), "little")
        header = f.read(header_len)
    json_header = json.loads(header)
    tensors = [v for k,v in json_header.items() if k != "__metadata__"]
    intervals = sorted(tuple(v["data_offsets"]) for v in tensors)
    assert intervals[0][0] == 0 and all(a[1] == b[0] for a,b in zip(intervals, intervals[1:]))
    assert 8 + header_len + intervals[-1][1] == path.stat().st_size
    return dict(header_bytes=header_len,header_sha256=hashlib.sha256(header).hexdigest(),
                tensor_count=len(tensors), dtypes=dict(collections.Counter(v["dtype"] for v in tensors)),
                metadata=json_header.get("__metadata__"), payload_bytes=intervals[-1][1])


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--review-root", required=True, type=Path)
    ap.add_argument("--kolmo", required=True, type=Path)
    ap.add_argument("--tag-reader", required=True, type=Path)
    ap.add_argument("--python", required=True, type=Path)
    ap.add_argument("--results", required=True, type=Path)
    a=ap.parse_args()
    assert sys.platform=="darwin", "ru_maxrss in this harness is expressed in Darwin bytes"
    a.results.mkdir(parents=True,exist_ok=True)
    panel=a.review_root/"bench-page"; pins=json.loads((panel/"data/FETCHED.json").read_text())
    env={**os.environ, "OMP_NUM_THREADS":"1","MKL_NUM_THREADS":"1",
         "OPENBLAS_NUM_THREADS":"1","VECLIB_MAXIMUM_THREADS":"1", "PYTHONWARNINGS":"ignore"}
    env.pop("KOLMO_MEM_FORCE_BUDGET",None)
    runinfo=dict(physical_bytes=os.sysconf("SC_PHYS_PAGES")*os.sysconf("SC_PAGESIZE"),
                 platform=os.uname().sysname+" "+os.uname().release+" "+os.uname().machine,
                 methods={"kolmo_L3":str(a.kolmo),"zipnn_0.5.4":str(a.python)},
                 kolmo_sha256=file_hash(a.kolmo),tag_reader_sha256=file_hash(a.tag_reader),inputs={})
    manifest=a.results/"whole-inputs.json"
    fieldnames="model trial order method input_bytes input_sha256 packed_bytes packed_sha256 restored_sha256 encode_wall_ns decode_wall_ns encode_rss_bytes decode_rss_bytes zip_encode_ready_ns zip_decode_ready_ns tag_restored_sha256".split()
    with (a.results/"whole-raw.csv").open("w",newline="") as rf, tempfile.TemporaryDirectory(prefix="kolmo-weights-") as td:
        tmp=Path(td); writer=csv.DictWriter(rf,fieldnames); writer.writeheader()
        for model,file_name in [("smollm2-135m","smollm2-135m-bf16.safetensors"),("qwen2.5-0.5b","qwen2.5-0.5b-bf16.safetensors")]:
            source=panel/"data/weights"/file_name
            meta=layout(source); size=source.stat().st_size; expected=file_hash(source)
            assert size==pins[model]["bytes"] and expected==pins[model]["sha256"]
            runinfo["inputs"][model]={"path":str(source),"sha256":expected,"bytes":size,**meta,"pinned_source":pins[model]["url"]}
            manifest.write_text(json.dumps(runinfo,indent=2)+"\n")
            for trial in range(4):   # trial 0 primes the filesystem and is excluded from the medians
                methods=("kolmo_L3","zipnn_0.5.4")
                if trial % 2: methods=methods[::-1]
                for position,method in enumerate(methods):
                    pack=tmp/"packed"; restore=tmp/"restored"; statfile=tmp/"phase-time"
                    if method=="kolmo_L3":
                        enc=[a.kolmo,"-q","-3","-T1","-o",pack,source]
                        dec=[a.kolmo,"-q","-d","-T1","-o",restore,pack]
                    else:
                        enc=[a.python,__file__,"--zip-phase","c",source,pack]
                        dec=[a.python,__file__,"--zip-phase","d",pack,restore]
                    e_ns,e_rss,zin=timed(enc,env,statfile)
                    d_ns,d_rss,zout=timed(dec,env,statfile)
                    got=file_hash(restore)
                    assert got==expected and restore.stat().st_size==size
                    archive_bytes=pack.stat().st_size; archive_hash=file_hash(pack);tag_hash=""
                    if trial==0 and method=="kolmo_L3":
                        restore.unlink()
                        subprocess.run([a.tag_reader,"-q","-d","-T1","-o",restore,pack],env=env,check=True)
                        tag_hash=file_hash(restore)
                        assert tag_hash==expected
                    writer.writerow(dict(zip(fieldnames,[model,trial,position,method,size,expected,archive_bytes,archive_hash,got,e_ns,d_ns,e_rss,d_rss,zin,zout,tag_hash])));rf.flush()
                    print(model,trial,method,archive_bytes,e_ns,d_ns,e_rss,d_rss,flush=True)
                    pack.unlink(); restore.unlink(); statfile.unlink()


if __name__=="__main__":
    main()
