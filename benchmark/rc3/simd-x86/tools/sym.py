"""sym.py TOP file... : pooled flat profile (innermost inlined function at each sampled address, addr2line -i)."""
import collections, subprocess, sys
top = int(sys.argv[1]); cnt = collections.Counter(); by = collections.defaultdict(collections.Counter)
for fn in sys.argv[2:]:
    for line in open(fn):
        p, a = line.split(); by[p][int(a, 16)] += 1
total = sum(sum(c.values()) for c in by.values())
cache = {}
for path, c in by.items():
    if not path.startswith("/") or "/build/kolmo" not in path:
        cnt["[" + path.rsplit("/", 1)[-1] + "]"] += sum(c.values()); continue
    addrs = list(c)
    out = subprocess.run(["addr2line", "-i", "-f", "-C", "-e", path] + ["%x" % a for a in addrs] + ["0"], capture_output=True, text=True).stdout.split("\n")
    # with -i, each address yields 2+ lines per frame; split on the sentinel by re-running per-address is slow, so use -a
    out = subprocess.run(["addr2line", "-a", "-i", "-f", "-e", path] + ["%x" % a for a in addrs], capture_output=True, text=True).stdout.split("\n")
    cur = None; first = {}
    i = 0
    while i < len(out):
        l = out[i]
        if l.startswith("0x"):
            cur = int(l, 16); i += 1
            f = out[i]; loc = out[i + 1] if i + 1 < len(out) else ""
            # skip intrinsic headers: walk frames until a non-include frame
            j = i
            while j + 1 < len(out) and not out[j].startswith("0x") and ("include/" in out[j + 1] and "intrin" in out[j + 1]):
                j += 2
            if j < len(out) and not out[j].startswith("0x"):
                f, loc = out[j], out[j + 1]
            src = loc.split(":")[0]
            own = "vendor/" + src.split("vendor/")[1].split("/")[0] if "vendor/" in src else ("kolmo" if "/src/" in src else "?")
            first[cur] = f"{f} ({own})"
            i += 2
            while i < len(out) and out[i] and not out[i].startswith("0x"): i += 1
        else: i += 1
    for a, k in c.items(): cnt[first.get(a, "?")] += k
print(f"total {total}")
for name, k in cnt.most_common(top): print(f"{100*k/total:6.2f}%  {k:6d}  {name}")
own = collections.Counter()
for name, k in cnt.items(): own["kolmo" if "(kolmo)" in name else "vendor" if "(vendor" in name else "sys/other"] += k
print("  ".join(f"{o} {100*k/total:.1f}%" for o, k in own.most_common()))
