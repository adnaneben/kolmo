"""Table of native coder against library from natbench lines (native lane, rc3 wave 2).

usage: python3 benchmark/rc3w2/native/proto_table.py components-pass2.txt components-pass1.txt
Pairs (role: library -> native coders) are listed below; a role's rows come from the first file that has them."""
import sys, re
PAIRS = [("speed", "lz4", ["klz1", "klz2"]), ("balanced", "zstd1", ["klzh1"]), ("balanced", "zstd3", ["klzh2", "klzh3", "klzh5"]),
         ("huf", "huf", ["khuf"]), ("leafchunks", "huf", ["khuf"]), ("eco", "brotli8", ["klzh7", "klzma3"]),
         ("hc", "zstd19", ["klzh8", "klzma6"]), ("ratio", "lzma9e", ["klzma8"])]
rows = {}
for path in reversed(sys.argv[1:]):   # later files first, so the first file wins
    for l in open(path):
        m = re.match(r"(\S+) (\S+) (\S+)\s+T(\d)\s+in\s+(\d+)\s+out\s+(\d+)\s+ratio\s+\S+\s+c\s+([\d.]+) MB/s\s+d\s+([\d.]+) MB/s", l)
        if m:
            role, st, codec, th, i, o, c, d = m.groups()
            rows[(role, st, codec, th)] = (int(i), int(o), float(c), float(d))
def f(x): return f"{x:.1f}" if x < 100 else f"{x:.0f}"
print("| role | set | library → native | threads | library size | native size | size | compress | decompress |")
print("|---|---|---|---|---|---|---|---|---|")
for role, lib, nats in PAIRS:
    for st in ("silesia", "struct", "kolmo"):
        for nat in nats:
            for th in ("1", "4"):
                a, b = rows.get((role, st, lib, th)), rows.get((role, st, nat, th))
                if not a or not b: continue
                print(f"| {role} | {st} | {lib} → {nat} | {th} | {a[1]:,} | {b[1]:,} | {100 * (b[1] / a[1] - 1):+.2f}% | {b[2] / a[2]:.2f}× ({f(b[2])} / {f(a[2])} MB/s) | {b[3] / a[3]:.2f}× ({f(b[3])} / {f(a[3])} MB/s) |")
