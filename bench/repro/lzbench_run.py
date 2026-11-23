#!/usr/bin/env python3
"""Single-thread lzbench comparison on the standard corpora, as in the 0.0.2 market report, plus the parse of
lzbench's CSV into JSON for the page.

usage: lzbench_run.py TOOLS.json CORPUS_DIR OUT_DIR [--quick]
       lzbench_run.py --parse CSV_DIR OUT.json            (only convert existing lzb-*.csv files)

Runs (each holds the timing lock if the caller wraps this script in it):
  fast list: best of 3 runs of 3 s per codec (-t3,3); slow list: one run (-i1,1 -t0,0); all one thread.
  corpora: silesia.tar, enwik8, canterbury.tar, and Silesia file by file (-r).
Kolmo levels in lzbench are `kolmo -1`..`-9`, and 10 = `kolmo -U` (bench/lzbench/README.md)."""
import csv, glob, json, os, re, subprocess, sys

FAST = "memcpy/lz4/lz4hc,9,12/snappy/libdeflate,6,12/zstd,1,3,6,9,12/brotli,1,5/lzav,1,2/zxc,3,7/kanzi,1,2,3,4/openzl_generic/kolmo,1,2,3,4"
SLOW = "zstd,15,19/zstd22,22/brotli,9,11/brotli24,11/xz,6,9/bzip2,9/bzip3,5,10/bsc0/bsc1/ppmd8,6,9/zpaq,4,5/kanzi,5,7,9/kolmo,5,6,7,8,9,10"
PERFILE = "zstd,3,19/xz,9/bzip3,5/brotli,11/openzl_generic/kolmo,3,7,9"
QUICK_FAST, QUICK_SLOW = "zstd,1,3,12/lz4/kolmo,1,2,3,4", "zstd,19/xz,9/bzip3,5/kolmo,5,6,9"


def family(name):
    n = name.split()[0].lower()
    return re.sub(r"\d+$", "", n) if n.startswith(("zstd", "brotli", "bsc")) else n


def parse(csv_dir, out):
    res = {}
    for p in sorted(glob.glob(os.path.join(csv_dir, "lzb-*.csv"))):
        tag = os.path.basename(p)[4:-4]  # fast-silesia.tar, slow-enwik8, silesia-files
        with open(p) as f:
            for r in csv.DictReader(f):
                if not r.get("Compressor name") or r["Compressor name"].startswith("memcpy"):
                    continue
                corpus = os.path.basename(r["Filename"]) if tag != "silesia-files" else "silesia/" + os.path.basename(r["Filename"])
                res.setdefault(corpus, []).append(dict(
                    codec=r["Compressor name"], family=family(r["Compressor name"]), list=tag.split("-")[0],
                    enc_mbs=float(r["Compression speed"]), dec_mbs=float(r["Decompression speed"]),
                    bytes=int(r["Original size"]), size=int(r["Compressed size"]),
                    ratio=int(r["Original size"]) / int(r["Compressed size"])))
    json.dump(dict(unit="MB/s as lzbench reports it (10^6 bytes per second)", corpora=res), open(out, "w"), indent=1)
    print(f"wrote {out}: {sum(len(v) for v in res.values())} rows over {len(res)} inputs")


def main():
    if sys.argv[1] == "--parse":
        return parse(sys.argv[2], sys.argv[3])
    tools, corpus, out = sys.argv[1:4]; quick = "--quick" in sys.argv
    LZ = json.load(open(tools))["lzbench"]["path"]; os.makedirs(out, exist_ok=True)
    fast, slow = (QUICK_FAST, QUICK_SLOW) if quick else (FAST, SLOW)
    jobs = []
    for f in (["canterbury.tar"] if quick else ["silesia.tar", "enwik8", "canterbury.tar"]):
        jobs.append((f"lzb-fast-{f}.csv", ["-t3,3", "-o4", "-e" + fast, os.path.join(corpus, f)]))
        jobs.append((f"lzb-slow-{f}.csv", ["-i1,1", "-t0,0", "-o4", "-e" + slow, os.path.join(corpus, f)]))
    if not quick:
        jobs.append(("lzb-silesia-files.csv", ["-i1,1", "-t0,0", "-o4", "-r", "-e" + PERFILE, os.path.join(corpus, "silesia")]))
    for name, args in jobs:
        dst = os.path.join(out, name)
        if os.path.exists(dst) and os.path.getsize(dst) > 0:
            continue
        print("lzbench", name, flush=True)
        with open(dst + ".part", "w") as o, open(dst[:-4] + ".err", "w") as e:
            subprocess.run([LZ] + args, stdout=o, stderr=e, check=True)
        os.replace(dst + ".part", dst)
    parse(out, os.path.join(out, "lzbench.json"))


if __name__ == "__main__":
    main()
