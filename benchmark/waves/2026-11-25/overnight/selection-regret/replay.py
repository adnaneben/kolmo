"""Measure the native size selector, every eligible fixed setting and all public presets."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import random
import re
import subprocess
import sys
import tempfile
import time

HERE = Path(__file__).resolve().parent
sha = lambda data: hashlib.sha256(data).hexdigest()


def require(ok, message):
    if not ok:
        raise ValueError(message)


def slices(data):
    """Reconstruct pinned target.c source coordinates for the frozen single-chunk files."""
    n = len(data)
    if n <= 262144:
        return [(0, n)]
    head = data[:4096]
    text = not data.startswith(b"\x93NUMPY") and sum(32 <= c < 127 or c in (9,10,13) or c >= 194 for c in head)*100 >= len(head)*95
    size = 65536
    out = []
    for k, start in enumerate([0, (n-size)//2, n-size]):
        end = start+size
        if text and k:
            nl = data.find(b"\n", start, start+size//2)
            if nl >= 0:
                start = nl+1
        if text and k < 2:
            e = end
            while e > start+size//2 and data[e-1] != 10:
                e -= 1
            if data[e-1] == 10:
                end = e
        out.append((start, end))
    return out


def process(command, env):
    with tempfile.TemporaryFile() as errors:
        start = time.monotonic_ns()
        with subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=errors, env=env) as child:
            _, status, usage = os.wait4(child.pid, 0)
            wall = time.monotonic_ns()-start
            child.returncode = os.waitstatus_to_exitcode(status)
        errors.seek(0)
        log = errors.read().decode(errors="replace")
        require(child.returncode == 0, log)
    return wall, usage.ru_maxrss*(1 if sys.platform == "darwin" else 1024), log


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ("encoder","reader","out"):
        p.add_argument("--"+key, type=Path, required=True)
    p.add_argument("--input-dir", type=Path, help="directory containing files named by their freeze.json keys")
    args = p.parse_args()
    m = json.loads((HERE/"freeze.json").read_text())
    require(sha(args.encoder.read_bytes()) == m["encoder_sha256"] and sha(args.reader.read_bytes()) == m["reader_sha256"],
            "codec digest mismatch")
    args.out.mkdir(parents=True, exist_ok=True)
    env = {k:v for k,v in os.environ.items() if not k.startswith(("CZN_", "KOLMO_"))}
    require(not Path("~/.config/kolmo/calibration").expanduser().is_file(), "run in an environment without custom calibration")
    rng = random.Random(m["seed"])
    variants = {"target": m["selector"], **m["eligible"], **m["other_public_presets"]}
    def write(writer, fields, result):
        if writer is None:
            writer = csv.DictWriter(fields, fieldnames=result)
            writer.writeheader()
        writer.writerow(result)
        return writer
    with tempfile.TemporaryDirectory() as temp, (args.out/"whole.csv").open("x",newline="",buffering=1) as whole, (args.out/"probes.csv").open("x",newline="",buffering=1) as probes:
        t = Path(temp)
        packed, recovered = t/"archive", t/"output"
        fw, pw, hashes = None, None, {}
        def run(path, flags, verbose=False):
            en, er, log = process([args.encoder, *flags, "-v" if verbose else "-q", "-T","1","-o", packed,"--",path],env)
            de, dr, _ = process([args.reader,"-d","-q","-T","1","-o",recovered,"--",packed],env)
            encoded = packed.read_bytes()
            require(recovered.read_bytes() == path.read_bytes(), "stock reader bytes mismatch")
            require(encoded.startswith(b"CZN1\x0c"), "unexpected frame format")
            packed.unlink(); recovered.unlink()
            return dict(stored_bytes=len(encoded),frame_sha256=sha(encoded),encode_ns=en,decode_ns=de,encode_rss=er,decode_rss=dr),log
        for name,info in m["producers"].items():
            src = args.input_dir/name if args.input_dir else Path(info["source"])
            data = src.read_bytes()
            require(len(data)==info["bytes"] and sha(data)==info["sha256"], "input digest mismatch")
            for rep in range(m["warmups"]+m["scored_rounds"]):
                for order,arm in enumerate(rng.sample(list(variants),len(variants))):
                    metrics,log = run(src,variants[arm],verbose=arm=="target")
                    chosen, pred = "", ""
                    if arm == "target":
                        hit = re.search(r"1 chunk: ([\w-]+) x1; predicted (\d+) bytes",log)
                        require(hit is not None and hit[1] in m["eligible"], "missing selected route report: "+log)
                        chosen,pred=hit.groups()
                    digest=metrics["frame_sha256"]
                    require(hashes.setdefault((name,arm),digest)==digest, "unstable frame")
                    fw=write(fw,whole,dict(file=name,repeat=rep,order=order,arm=arm,actual_chosen=chosen,
                                           predicted_bytes=pred,**metrics))
            for start,end in slices(data):
                if (start,end)==(0,len(data)):
                    continue  # reuse whole.csv for the identical whole-file sample
                segment=t/"segment";segment.write_bytes(data[start:end])
                for arm,flags in m["eligible"].items():
                    metrics,_=run(segment,flags)
                    pw=write(pw,probes,dict(file=name,offset=start,length=end-start,
                                            input_sha256=sha(data[start:end]),arm=arm,**metrics))
            print(name,"complete",flush=True)


if __name__ == "__main__":
    main()
