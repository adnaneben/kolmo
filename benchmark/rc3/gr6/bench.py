#!/usr/bin/env python3
"""Interleaved single-thread benchmark: base vs new kolmo binaries.
usage: bench.py ROUNDS OUTJSON MODEARGS -- file1 file2 ...
Each round runs, per file, base then new (alternating order each round): compress, decompress, verify.
Records size and min wall time over rounds."""
import json, os, subprocess, sys, time, filecmp

SP = "/private/tmp/claude-501/-Users-adnanbenali-Projects-kolmo/150de77a-2a2f-5cf3-b144-4e505fb7f30d/scratchpad/gr6"
BINS = {"base": SP + "/bin/kolmo-base", "new": SP + "/bin/kolmo-new"}
W = SP + "/w"

def run(cmd):
    t = time.perf_counter()
    r = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    dt = time.perf_counter() - t
    if r.returncode:
        raise SystemExit("failed: %s: %s" % (cmd, r.stderr.decode()))
    return dt

def main():
    rounds = int(sys.argv[1]); out = sys.argv[2]
    i = sys.argv.index("--"); modeargs = sys.argv[3:i]; files = sys.argv[i + 1:]
    os.makedirs(W, exist_ok=True)
    res = {}
    for rd in range(rounds):
        for f in files:
            name = os.path.basename(f)
            order = ["base", "new"] if rd % 2 == 0 else ["new", "base"]
            for b in order:
                c = "%s/%s.%s.k" % (W, name, b); d = "%s/%s.%s.out" % (W, name, b)
                tc = run([BINS[b]] + modeargs + ["-T", "1", f, c])
                td = run([BINS[b], "-d", "-T", "1", c, d])
                if not filecmp.cmp(f, d, shallow=False):
                    raise SystemExit("ROUNDTRIP MISMATCH %s %s" % (b, f))
                with open(c, "rb") as fh: hdr = fh.read(6)
                e = res.setdefault(name, {}).setdefault(b, {"size": os.path.getsize(c), "ver": hdr[4], "tc": [], "td": []})
                e["tc"].append(tc); e["td"].append(td)
                os.remove(d)
                print("%d %s %s size=%d ver=%d c=%.2fs d=%.2fs" % (rd, name, b, e["size"], hdr[4], tc, td), flush=True)
    for name, v in res.items():
        v["orig"] = os.path.getsize([f for f in files if os.path.basename(f) == name][0])
    with open(out, "w") as fh: json.dump(res, fh, indent=1)

main()
