#!/usr/bin/env python3
"""R11 experiment 1: what would a prior pack be worth, measured with the unmodified rc3 binary.
For a set of files of one type in arrival order: train = first half, test = NT files evenly spaced in the second half.
  untrained      : sum of size(kolmo MODE f) over the test files
  profile        : kolmo train on the train files, then kolmo -P p.kpf MODE f (rc3's trained profiles, zstd dictionary)
  primed (sim)   : sum of size(MODE P||f) - size(MODE P), P = the last W bytes of the concatenated train files.
                   This is the marginal cost of f for a coder whose state was warmed on P: the figure a primed codec
                   would reach, less a few bytes of stream end/header per file. Not a product feature of rc3.
Every compressed file is decoded and compared (cmp) before its size is used.
usage: prior_sim.py SET NT W_KIB [curve]   -> JSON lines on stdout"""
import json, os, subprocess, sys, tempfile, filecmp
K = "/home/user/kolmo-review40-base/build/kolmo"
SETS = "/home/user/kolmo-review40-data/R11/sets"
setn, NT, W = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]) << 10
curve = len(sys.argv) > 4
tmp = tempfile.mkdtemp(prefix="r11_")
def comp(args, data=None, path=None):
    """size of kolmo args on data (round trip checked)"""
    if path is None:
        path = os.path.join(tmp, "in"); open(path, "wb").write(data)
    o, d = os.path.join(tmp, "o.k"), os.path.join(tmp, "o.d")
    for p in (o, d):
        if os.path.exists(p): os.remove(p)
    subprocess.run(["nice", K] + args + ["-T", "1", path, o], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    dargs = ["-d"] + [a for i, a in enumerate(args) if a == "-P" or (i and args[i-1] == "-P")]
    subprocess.run(["nice", K] + dargs + ["-T", "1", o, d], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    assert filecmp.cmp(path, d, shallow=False), "round trip failed"
    return os.path.getsize(o)
files = sorted(os.listdir(os.path.join(SETS, setn)))
half = len(files) // 2
train, rest = files[:half], files[half:]
step = max(1, len(rest) // NT); test = rest[::step][:NT]
rd = lambda f: open(os.path.join(SETS, setn, f), "rb").read()
tr = b"".join(rd(f) for f in train)
res = {"set": setn, "train_files": len(train), "train_bytes": len(tr), "test_files": len(test), "W": W}
tb = [rd(f) for f in test]; res["test_bytes"] = sum(len(b) for b in tb)
MODES = {"balanced": ["-m", "balanced"], "hc": ["-m", "hc"], "ratio": ["-m", "ratio"], "x": ["-x"], "x-n": ["-x", "-n"], "ratio-n": ["-m", "ratio", "-n"]}
if not curve:
    for name, a in MODES.items(): res["untrained_" + name] = sum(comp(a, b) for b in tb)
    for mode in ("balanced", "hc"):
        kpf = os.path.join(tmp, mode + ".kpf")
        subprocess.run([K, "train", "-m", mode, "-o", kpf] + [os.path.join(SETS, setn, f) for f in train], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        res["profile_bytes_" + mode] = os.path.getsize(kpf)
        res["profile_" + mode] = sum(comp(["-P", kpf, "-m", mode], b) for b in tb)
        if mode == "hc":
            res["profile_hc_on_x"] = sum(comp(["-P", kpf, "-x"], b) for b in tb)
            res["profile_hc_on_ratio"] = sum(comp(["-P", kpf, "-m", "ratio"], b) for b in tb)
    P = tr[-W:]
    for name in ("x-n", "ratio-n", "hc"):
        base = comp(MODES[name], P)
        res["primed_" + name] = sum(comp(MODES[name], P + b) - base for b in tb)
        res["primed_base_" + name] = base
    print(json.dumps(res), flush=True)
else:
    for k in (0, 1, 2, 4, 8, 16, 32, 64, len(train)):
        if k > len(train): continue
        P = b"".join(rd(f) for f in train[:k])[-W:]
        r = dict(res); r["k"] = k; r["prior_bytes"] = len(P)
        if k == 0: r["x-n"] = sum(comp(MODES["x-n"], b) for b in tb)
        else:
            base = comp(MODES["x-n"], P); r["x-n"] = sum(comp(MODES["x-n"], P + b) - base for b in tb)
        print(json.dumps(r), flush=True)
