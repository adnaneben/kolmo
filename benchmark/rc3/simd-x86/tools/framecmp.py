"""framecmp.py OUT WORK DATA name=binary[@ENV=VAL] ...: frames of every (input, setting) from every build/variant,
compared by SHA-256; the first build's frame decoded by every build. One line per case, JSON summary at the end."""
import hashlib, json, os, subprocess, sys
out, W, D = sys.argv[1], sys.argv[2], sys.argv[3]
bins = []
for a in sys.argv[4:]:
    n, rest = a.split("=", 1); env = {}
    if "@" in rest: rest, e = rest.split("@"); k, v = e.split("="); env[k] = v
    bins.append((n, rest, env))
os.makedirs(W, exist_ok=True)
FULL = [["-m", "speed"], ["-m", "balanced"], ["-m", "balanced", "--native-leaf"], ["-m", "fast", "--typed-huf"]]
MID = [["-m", "eco"], ["-m", "hc"], ["-m", "ratio"]]                  # 4 MiB prefixes (one block)
SMALL = [["-x"], ["-x", "-l", "1"], ["-x", "-l", "2"]]               # 512 KiB prefixes
def run(b, args):
    r = subprocess.run([b[1], *args], env={**os.environ, **b[2]}, capture_output=True)
    if r.returncode: raise SystemExit(f"{b[0]} {args}: {r.stderr[:200]}")
def sha(p): return hashlib.sha256(open(p, "rb").read()).hexdigest()[:16]
rows = []; bad = 0
files = sorted(os.listdir(D))
for f in files:
    src = os.path.join(D, f)
    small = os.path.join(W, "x_" + f); open(small, "wb").write(open(src, "rb").read()[:512 * 1024])
    mid = os.path.join(W, "m_" + f); open(mid, "wb").write(open(src, "rb").read()[:4 << 20])
    for st in FULL + MID + SMALL:
        inp = small if st in SMALL else mid if st in MID else src
        T = ["-T1"] if "--typed-huf" in st else ["-T4"]
        h = {}
        for b in bins:
            o = os.path.join(W, b[0] + ".k"); run(b, [*st, *T, inp, o]); h[b[0]] = sha(o)
        ref = os.path.join(W, bins[0][0] + ".k"); dec = {}
        for b in bins:
            u = os.path.join(W, "u"); run(b, ["-d", "-T4", ref, u]); dec[b[0]] = open(u, "rb").read() == open(inp, "rb").read()
        same = len(set(h.values())) == 1; ok = all(dec.values()); bad += (not same) + (not ok)
        rows.append({"file": f, "setting": " ".join(st), "bytes": os.path.getsize(inp), "frame": h[bins[0][0]], "size": os.path.getsize(ref), "same": same, "decoded": ok, **({} if same else {"frames": h})})
        print(f"{f:20s} {' '.join(st):28s} {'same' if same else 'DIFFERENT':9s} {'decoded' if ok else 'DECODE FAIL'} {h[bins[0][0]]}", flush=True)
json.dump({"builds": [[n, p, e] for n, p, e in bins], "cases": rows, "bad": bad}, open(out, "w"), indent=1)
print("ALL SAME AND DECODED" if not bad else f"{bad} PROBLEMS")
