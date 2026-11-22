"""Fold sampler.c output into a flat profile: python3 -I symbolize.py SAMPLES.txt [TOP]
Function = innermost source function at the sampled address that is not a compiler intrinsic header
(addr2line -i; inlined helpers count as themselves, as perf does with inline unwinding). Owner = Kolmo (src/),
vendored (vendor/<lib>) or a system library (resolved by its dynamic symbol table)."""
import bisect, collections, subprocess, sys
fn = sys.argv[1]; top = int(sys.argv[2]) if len(sys.argv) > 2 else 15
by_file = collections.defaultdict(list)
for line in open(fn):
    p, a = line.split(); by_file[p].append(int(a, 16))
total = sum(len(v) for v in by_file.values()); cnt = collections.Counter(); owner = {}

def dynsyms(path):
    out = subprocess.run(["nm", "-D", "--defined-only", path], capture_output=True, text=True).stdout.split("\n")
    s = sorted((int(x.split()[0], 16), x.split()[2]) for x in out if len(x.split()) == 3 and x.split()[1] in "Tti")
    return [a for a, _ in s], [n for _, n in s]

for path, addrs in by_file.items():
    if not path.startswith("/") or path.startswith("/usr/lib") or path.startswith("/lib") or "sampler" in path:
        lib = path.rsplit("/", 1)[-1]
        A, N = dynsyms(path) if path.startswith("/") else ([], [])
        data = open(path, "rb").read() if path.startswith("/") else b""
        def label(a):
            i = bisect.bisect_right(A, a) - 1; name = N[i].split("@")[0] if i >= 0 else "?"
            if data[a - 2:a] == b"\x0f\x05":            # returning from a syscall: kernel time of that call
                return f"syscall {name.lstrip('_').replace('libc_', '')} ({lib}, kernel time)"
            d = subprocess.run(["objdump", "-d", path, f"--start-address={a:#x}", f"--stop-address={a + 8:#x}"], capture_output=True, text=True).stdout
            ins = [l for l in d.split("\n") if l.strip()[:1].isalnum() and ":\t" in l]
            if ins and any(x in ins[0] for x in ("movs", "vmovdq", "vmovup", "stos", "prefetch")):
                return f"memcpy/memmove/memset ({lib}, stripped)"
            return f"{name} ({lib})"
        lab = {a: label(a) for a in set(addrs)}
        for a in addrs:
            k = lab[a]
            cnt[k] += 1; owner[k] = "system"
        continue
    res = {}
    for a in sorted(set(addrs)):
        o = subprocess.run(["addr2line", "-f", "-i", "-e", path, hex(a)], capture_output=True, text=True).stdout.strip().split("\n")
        frames = [(o[j], o[j + 1].split(":")[0]) for j in range(0, len(o) - 1, 2)]
        keep = [f for f in frames if "/usr/lib/gcc" not in f[1] and "/usr/include" not in f[1]] or frames
        res[a] = keep[0]
    for a in addrs:
        f, src = res[a]; cnt[f] += 1
        owner[f] = "Kolmo" if "src/" in src and "vendor/" not in src else ("vendored: " + src.split("vendor/")[1].split("/")[0] if "vendor/" in src else src)
print(f"{total} samples")
print("| # | function | share | owner |\n|---:|---|---:|---|")
acc = 0
for i, (k, c) in enumerate(cnt.most_common(top), 1):
    acc += c; print(f"| {i} | `{k}` | {100*c/total:.1f}% | {owner[k]} |")
kol = sum(c for k, c in cnt.items() if owner[k] == "Kolmo"); ven = sum(c for k, c in cnt.items() if owner[k].startswith("vendored"))
print(f"\ntop {top}: {100*acc/total:.1f}% of samples; all Kolmo-owned code {100*kol/total:.1f}%, vendored {100*ven/total:.1f}%, system libraries {100*(total-kol-ven)/total:.1f}%")
