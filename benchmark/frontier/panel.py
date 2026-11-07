"""Prepare deterministic development slices and run a resumable, one-worker lzbench panel."""
import argparse
import csv
import fcntl
import hashlib
import io
import json
import os
from pathlib import Path
import random
import subprocess
import tarfile
import time

LIMIT = 4 << 20
HERE = Path(__file__).resolve().parent
MODES = ["rt", "fast", "eco", "hc", "max", "ultra"]
OPTIONS = {f"kolmo_{m}": f"kolmo,{i}" for i, m in enumerate(MODES)}
OPTIONS.update(kbc="kbc", krc="krc", lz4="lz4", lz4fast="lz4fast,17",
               zstd1="zstd,1", zstd3="zstd,3", zstd19="zstd,19",
               brotli8="brotli,8", brotli11="brotli,11", lzma9="lzma,9",
               bsc="bsc1", ppmd="ppmd8,8", zpaq="zpaq,5", openzl="openzl_generic")
MARKET = {
    "aceapex-fast": "aceapex_fast,8", "aceapex3": "aceapex,3",
    "brieflz5": "brieflz,5", "bzip2": "bzip2,9", "bzip3": "bzip3,5",
    "density1": "density,1", "density3": "density,3", "fastlz": "fastlz,1",
    "fastlzma2": "fastlzma2,9", "kanzi1": "kanzi,1", "kanzi6": "kanzi,6",
    "kanzi9": "kanzi,9", "libdeflate1": "libdeflate,1", "libdeflate12": "libdeflate,12",
    "lizard10": "lizard,10", "lizard39": "lizard,39", "lzav1": "lzav,1",
    "lzav2": "lzav,2", "lzf1": "lzf,1", "lzfse": "lzfse", "lzvn": "lzvn",
    "lzo1x": "lzo1x,1", "memlz": "memlz", "misa77": "misa77,0", "pulsar": "pulsar",
    "quicklz": "quicklz,1", "snappy": "snappy", "wflz": "wflz", "wzip": "wzip,9",
    "zlibng": "zlib-ng,6", "zstd22": "zstd,22", "zstd-fast5": "zstd_fast,-5",
}
MEMBERS = "dickens mozilla mr nci ooffice osdb reymont samba sao webster xml x-ray".split()


def sha(path):
    with path.open("rb") as f:
        h = hashlib.sha256()
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def write_json(path, data):
    tmp = path.with_suffix(".tmp")
    tmp.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
    os.replace(tmp, path)


def prepare(args):
    out = args.work / "samples"
    out.mkdir(exist_ok=True, parents=True)
    old = args.previous_workspace
    sources = old / "build/comparison/data"
    files = dict(rgb=old / "build/holdout/astronaut.rgb", pcm=sources / "digits.pcm",
                 yuv=sources / "foreman.yuv", fasta=sources / "ecoli.fa",
                 integers=sources / "numbers.test", co2=sources / "table.test.csv",
                 grid=args.grid, png=sources / "astronaut.png")
    provenance = {}
    with tarfile.open(old / "benchmark/ratio/data/silesia.tar", "r:") as t:
        for name in MEMBERS:
            member = t.getmember(name)
            with t.extractfile(member) as f:
                (out / name).write_bytes(f.read(LIMIT))
            provenance[name] = f"Silesia/{name}"
    for name, path in files.items():
        with path.open("rb") as f:
            (out / name).write_bytes(f.read(LIMIT))
        provenance[name] = path.name
    (out / "entropy").write_bytes(hashlib.shake_256(b"kolmo entropy control v1").digest(LIMIT))
    provenance["entropy"] = "SHAKE256 of the UTF-8 bytes kolmo entropy control v1"
    return {name: dict(source=source, size=(out/name).stat().st_size, sha256=sha(out/name))
            for name, source in provenance.items()}


def run(args):
    manifest = json.loads((args.work / "manifest.json").read_text())
    inputs = manifest["samples"]
    binary = args.work / "frozen/lzbench"
    lib = args.work / "frozen/build/libkolmo.dylib"
    assert sha(binary) == manifest["lzbench"] and sha(lib) == manifest["libkolmo"]
    for name, entry in inputs.items():
        assert sha(args.work / "samples" / name) == entry["sha256"]
    options = OPTIONS
    if args.stage == "market":
        extra = json.loads((args.work / "market-manifest.json").read_text())
        binary = args.work / "market-src/lzbench"
        assert sha(binary) == extra["lzbench"]
        assert sha(binary.parent / extra["rust_path"]) == extra["rust_sha256"]
        assert extra["options"] == MARKET
        options = MARKET
        selected = list(MARKET)
    elif args.stage == "overview":
        selected = ["kolmo_rt", "kolmo_fast", "lz4", "zstd3", "openzl"]
    else:
        selected = list(OPTIONS)
    work = [(name, codec) for name in sorted(inputs) for codec in selected]
    random.Random(20261107).shuffle(work)
    dest = args.work / "observations"
    dest.mkdir(exist_ok=True)
    for name, codec in work:
        path = dest / f"{name}.{codec}.json"
        if path.exists():
            continue
        flags = ["-e" + options[codec], "-I1", "-T1", "-t0,0", "-i3,5", "-p3", "-o4", "-q"]
        # Shared with development timing, even when using different runners.
        with (args.work / "timing.lock").open("a") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            if path.exists():
                continue
            start = time.time()
            proc = subprocess.run([binary, *flags, args.work / "samples" / name],
                                  cwd=binary.parent, text=True, capture_output=True)
        rows = list(csv.DictReader(io.StringIO(proc.stdout)))
        valid = proc.returncode == 0 and len(rows) == 1
        if valid:
            row = rows[0]
            valid = (int(row["Original size"]) == inputs[name]["size"]
                     and int(row["Compressed size"]) > 0
                     and all(float(row[k]) > 0 for k in ("Compression speed", "Decompression speed")))
        record = dict(input=name, codec=codec, flags=flags, valid=valid,
                      start_epoch=start, duration=time.time()-start, code=proc.returncode,
                      stderr=proc.stderr, raw=proc.stdout.replace(str(args.work / "samples")+"/", ""))
        if valid:
            record.update(bytes=int(row["Compressed size"]), encode=float(row["Compression speed"]),
                          decode=float(row["Decompression speed"]))
        write_json(path, record)
        print(name, codec, valid, flush=True)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("stage", choices=["prepare", "overview", "full", "market"])
    p.add_argument("work", type=Path)
    p.add_argument("--previous-workspace", type=Path)
    p.add_argument("--grid", type=Path)
    a = p.parse_args()
    a.work = a.work.resolve()
    if a.stage == "prepare":
        samples = prepare(a)
        manifest = dict(samples=samples, options=OPTIONS, lzbench=sha(a.work/"frozen/lzbench"),
                        libkolmo=sha(a.work/"frozen/build/libkolmo.dylib"),
                        upstream_lzbench="d68cdbc2309408e82da881c61d23cf70cca8db09",
                        baseline="8ada3f54dba9155c0abdfa5e88d140433571b1b5")
        write_json(a.work / "manifest.json", manifest)
    else:
        run(a)


if __name__ == "__main__":
    main()
