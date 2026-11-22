"""callers.py FUNC TOP files...: for samples whose innermost function is FUNC, the call site one inline level up."""
import collections, subprocess, sys
fn, top = sys.argv[1], int(sys.argv[2]); by = collections.Counter()
for f in sys.argv[3:]:
    for l in open(f):
        p, a = l.split()
        if "/build/kolmo" in p: by[(p, int(a, 16))] += 1
cnt = collections.Counter()
for path in {p for p, _ in by}:
    addrs = [a for p, a in by if p == path]
    out = subprocess.run(["addr2line", "-a", "-i", "-f", "-e", path] + ["%x" % a for a in addrs], capture_output=True, text=True).stdout.split("\n")
    i = 0
    while i < len(out):
        if out[i].startswith("0x"):
            cur = int(out[i], 16); fr = []; i += 1
            while i + 1 < len(out) and out[i] and not out[i].startswith("0x"): fr.append((out[i], out[i + 1])); i += 2
            if fr and fr[0][0] == fn and len(fr) > 1: cnt[fr[1][0] + " @ " + fr[0][1].split("/")[-1].split(" ")[0]] += by[(path, cur)]
            # fr[0][1] is the line in FUNC; the call site line is in fr[1]? addr2line -i: each frame's location is where the next-inner was inlined
            if fr and fr[0][0] == fn and len(fr) > 1: cnt["callsite " + fr[1][1].split("/")[-1].split(" ")[0]] += by[(path, cur)]
        else: i += 1
for k, v in cnt.most_common(top): print(v, k)
