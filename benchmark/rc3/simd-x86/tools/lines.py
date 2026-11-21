"""lines.py FUNC TOP files...: samples whose innermost function is FUNC, by source line."""
import collections, subprocess, sys
fn, top = sys.argv[1], int(sys.argv[2]); by = collections.Counter()
for f in sys.argv[3:]:
    for l in open(f):
        p, a = l.split()
        if "/build/kolmo" in p: by[(p, int(a, 16))] += 1
paths = {p for p, _ in by}; cnt = collections.Counter(); tot = 0
for path in paths:
    addrs = [a for p, a in by if p == path]
    out = subprocess.run(["addr2line", "-a", "-i", "-f", "-e", path] + ["%x" % a for a in addrs], capture_output=True, text=True).stdout.split("\n")
    i = 0; cur = None
    while i < len(out):
        if out[i].startswith("0x"):
            cur = int(out[i], 16); f, loc = out[i + 1], out[i + 2]
            if f == fn: cnt[loc.split("/")[-1].split(" ")[0]] += by[(path, cur)]
            i += 3
            while i < len(out) and out[i] and not out[i].startswith("0x"): i += 1
        else: i += 1
tot = sum(cnt.values())
for k, v in cnt.most_common(top): print(f"{100*v/tot:5.1f}% {v:6d} {k}")
