"""Line-level profile from proploop's PROPLOOP_PCS samples (macOS atos with inlined frames).

usage: python3 -I benchmark/rc3/simd-arm/pcsym.py BINARY PCS_FILE [TOP]
Prints the share of samples per innermost function (inlined functions count as themselves) and per source line.
BINARY needs debug info next to it (dsymutil BINARY).
"""
import collections
import re
import subprocess
import sys

binary, pcf = sys.argv[1], sys.argv[2]
top = int(sys.argv[3]) if len(sys.argv) > 3 else 25
pcs = [l.strip() for l in open(pcf) if l.strip()]
cnt = collections.Counter(pcs)
addrs = list(cnt)
fn, ln = collections.Counter(), collections.Counter()
for i in range(0, len(addrs), 2000):
    chunk = addrs[i:i + 2000]
    out = subprocess.run(["atos", "-i", "-o", binary, "-l", "0x100000000"] + chunk, capture_output=True, text=True).stdout
    # atos -i prints the inline chain innermost first, one line each, then a blank line per address
    blocks = out.strip("\n").split("\n\n")
    for a, b in zip(chunk, blocks):
        first = b.splitlines()[0] if b.strip() else "?"
        m = re.match(r"(.+?) \(in [^)]+\)(?: \((.+?)\))?", first)
        f = m.group(1) if m else first
        loc = m.group(2) if m and m.group(2) else "?"
        fn[f] += cnt[a]
        ln[f"{f}  {loc}"] += cnt[a]
tot = sum(cnt.values())
print(f"samples {tot}")
for f, c in fn.most_common(top):
    print(f"  {100.0 * c / tot:5.1f}%  {f}")
print("lines:")
for f, c in ln.most_common(top):
    print(f"  {100.0 * c / tot:5.1f}%  {f}")
