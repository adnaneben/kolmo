"""Queryable archives: index size and the cost of a query against a full decode (features-b report).

usage: python3 -I bench_query.py KOLMO OUT.json NAME=FILE ...
For each file: sizes of the plain frame (balanced), the indexed frame (balanced, filter on and off) and zstd -19; then
each query of QUERIES[NAME] with the index and with --no-index (every block decoded, the same answer), at -T1 and -T4,
best of 3 wall times, with the bytes decoded reported by --stats. Every indexed answer is compared with --no-index. """
import json, os, re, subprocess, sys, tempfile, time

KOLMO, OUT = sys.argv[1], sys.argv[2]
FILES = dict(a.split("=", 1) for a in sys.argv[3:])
QUERIES = {
    "owid": [("cat --col co2", "column co2, all rows"), ("cat --col co2 --rows 50000-51000", "column co2, rows 50000-51000"),
             ("cat --rows 30000-30100", "rows 30000-30100"), ("grep -F -w Morocco", "grep -F -w Morocco"),
             ("grep -c -E '^Mor[a-z]+,20[12][0-9],'", "grep -c -E '^Mor[a-z]+,20[12][0-9],'")],
    "sensor": [("cat --col co2", "column co2, all rows"), ("cat --col co2 --rows 1000000-1010000", "column co2, 10k rows"),
               ("cat --rows 2500000-2500100", "rows 2500000-2500100"), ("grep -c -F -w fault", "grep -c -F -w fault"),
               ("grep -c -F 1700012345", "grep -c -F 1700012345 (a timestamp)")],
    "hdfs": [("cat --rows 1000000-1000100", "rows 1000000-1000100"), ("cat --col '#4' --rows 1000000-1100000", "field 4, 100k rows"),
             ("grep -F -w 1660256537217386052", "grep -F -w (a block id)"), ("grep -c -F WARN", "grep -c -F WARN"),
             ("grep -c -E 'blk_-?16089[0-9]+'", "grep -c -E 'blk_-?16089[0-9]+'")],
    "json": [("cat --col co2", "key co2, all rows"), ("cat --col co2 --rows 400000-410000", "key co2, 10k rows"),
             ("grep -c -F -w maint", "grep -c -F -w maint")],
}

def run(cmd):
    t = time.perf_counter(); p = subprocess.run(cmd, shell=True, capture_output=True); dt = time.perf_counter() - t
    return dt, p.stdout, p.stderr.decode(), p.returncode

def best(cmd, n=3):
    r = None
    for _ in range(n):
        x = run(cmd)
        if r is None or x[0] < r[0]: r = x
    return r

res = json.load(open(OUT)) if os.path.exists(OUT) else {}
tmp = tempfile.mkdtemp(dir=os.environ.get("TMPDIR"))
for name, path in FILES.items():
    raw = os.path.getsize(path)
    R = res.setdefault(name, {"file": os.path.basename(path), "bytes": raw, "sizes": {}, "queries": {}})
    plain, idx, nof, zst = (os.path.join(tmp, name + s) for s in (".plain.kol", ".kol", ".nofilter.kol", ".zst"))
    for label, cmd, out in (("plain balanced", f"{KOLMO} -T4 {path} {plain}", plain),
                            ("indexed balanced", f"{KOLMO} --index -T4 {path} {idx}", idx),
                            ("indexed balanced, no filter", f"{KOLMO} --index --no-filter -T4 {path} {nof}", nof),
                            ("zstd -19", f"zstd -q -f -19 -T4 {path} -o {zst}", zst)):
        dt, _, err, rc = run(cmd); assert rc == 0, err
        R["sizes"][label] = {"size": os.path.getsize(out), "c_s_T4": dt}
    for T in (1, 4):
        dt, _, _, rc = best(f"{KOLMO} -d -T{T} {plain} {tmp}/out"); R["sizes"]["plain balanced"][f"d_s_T{T}"] = dt
        dt, _, _, rc = best(f"{KOLMO} -d -T{T} {idx} {tmp}/out"); R["sizes"]["indexed balanced"][f"d_s_T{T}"] = dt
    os.remove(f"{tmp}/out")
    R["sizes"]["zstd -19"]["d_s_T1"] = best(f"zstd -q -d -c {zst} > /dev/null")[0]
    print(name, json.dumps(R["sizes"]), flush=True)
    for q, label in QUERIES[name]:
        Q = R["queries"].setdefault(label, {"cmd": q})
        for T in (1, 4):
            for mode, extra in (("index", ""), ("scan", " --no-index")):
                dt, out, err, rc = best(f"{KOLMO} {q.split(' ', 1)[0]} -T {T} --stats{extra} {q.split(' ', 1)[1]} {idx}")
                m = re.search(r"decoded (\d+) of (\d+) bytes .*read (\d+) stored bytes.*?(\d+) rows", err)
                b = re.search(r"(\d+) blocks, (\d+) decoded whole, (\d+) read by column, (\d+) ruled out", err)
                Q[f"{mode}_T{T}"] = {"s": dt, "decoded": int(m.group(1)) if m else None, "stored_read": int(m.group(3)) if m else None,
                                     "blocks": [int(x) for x in b.groups()] if b else None, "rc": rc, "out_bytes": len(out)}
                Q.setdefault("outs", {})[f"{mode}_T{T}"] = hash(out)
        Q["same_answer"] = len(set(Q.pop("outs").values())) == 1
        if q.startswith("grep"):                                          # zstd -19 and grep, the usual way
            g = q.split(" ", 1)[1]
            Q["zstdcat_grep_T1"] = best(f"zstd -q -d -c {zst} | grep {g}")[0]
        print(f"{name:7s} {label:40s} idx T1 {Q['index_T1']['s']:.3f}s ({Q['index_T1']['decoded']} B)  scan T1 {Q['scan_T1']['s']:.3f}s  "
              f"idx T4 {Q['index_T4']['s']:.3f}s  scan T4 {Q['scan_T4']['s']:.3f}s  same={Q['same_answer']}", flush=True)
    json.dump(res, open(OUT, "w"), indent=1)
    for f in (plain, idx, nof, zst): os.remove(f)
os.rmdir(tmp)
