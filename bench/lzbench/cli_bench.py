#!/usr/bin/env python3
"""Command-line comparison on one file, one thread and all cores: kolmo, zstd, xz and bzip3, as in the market report.

    python3 -I cli_bench.py <kolmo binary> <input file> <work dir> <runs> <out.json>

Every tool is started the same way (fork/exec, wait4), file to file, one at a time. Each setting runs <runs> times;
the JSON keeps every time, and the table gives the best and the spread ((max - min) / best). Every decode is
compared with the input byte for byte. Peak RSS comes from wait4 (KiB on Linux, bytes on macOS).
"""
import filecmp, json, os, sys, time


def tools(kolmo, cores):
    sh = lambda s: ["sh", "-c", s]
    t = []
    for lv in (3, 7, 9):
        for th in (1, 0):
            t.append((f"kolmo -{lv} -T{th}", [kolmo, f"-{lv}", "-T", str(th), "-q", "-f", "{i}", "-o", "{o}"],
                      [kolmo, "-d", "-T", str(th), "-q", "-f", "{i}", "-o", "{o}"]))
    for lv in (3, 19):
        for th in (1, 0):
            t.append((f"zstd -{lv} -T{th}", ["zstd", f"-{lv}", f"-T{th}", "-q", "-f", "{i}", "-o", "{o}"],
                      ["zstd", "-d", "-q", "-f", "{i}", "-o", "{o}"]))
    for th in (1, 0):
        t.append((f"xz -9 -T{th}", sh(f"xz -9 -T{th} -c '{{i}}' > '{{o}}'"), sh(f"xz -d -T{th} -c '{{i}}' > '{{o}}'")))
    for j in (1, cores):
        t.append((f"bzip3 -j{j}", sh(f"bzip3 -j {j} -c '{{i}}' > '{{o}}'"), sh(f"bzip3 -d -j {j} -c '{{i}}' > '{{o}}'")))
    return t


def run(argv):
    t = time.perf_counter()
    pid = os.fork()
    if pid == 0:
        fd = os.open("/dev/null", os.O_WRONLY)
        os.dup2(fd, 1)
        os.dup2(fd, 2)
        os.execvp(argv[0], argv)
    _, status, ru = os.wait4(pid, 0)
    if status != 0:
        sys.exit(f"failed ({status}): {argv}")
    rss = ru.ru_maxrss * (1 if sys.platform == "darwin" else 1024)
    return time.perf_counter() - t, rss


def main():
    if len(sys.argv) != 6:
        sys.exit(__doc__)
    kolmo, src, work, n, out = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]), sys.argv[5]
    os.makedirs(work, exist_ok=True)
    size = os.path.getsize(src)
    cores = os.cpu_count()
    res = {"file": os.path.basename(src), "size": size, "runs": n, "cores": cores, "results": {}}
    c, d = os.path.join(work, "c.bin"), os.path.join(work, "d.bin")
    for name, ca, da in tools(kolmo, cores):
        enc, dec, rss_e, rss_d = [], [], 0, 0
        for _ in range(n):
            for p in (c, d):
                if os.path.exists(p):
                    os.remove(p)
            te, re_ = run([a.replace("{i}", src).replace("{o}", c) for a in ca])
            td, rd = run([a.replace("{i}", c).replace("{o}", d) for a in da])
            if not filecmp.cmp(src, d, shallow=False):
                sys.exit(f"MISMATCH {name}")
            enc.append(te), dec.append(td)
            rss_e, rss_d = max(rss_e, re_), max(rss_d, rd)
        r = dict(comp_bytes=os.path.getsize(c), ratio=size / os.path.getsize(c), enc_s=enc, dec_s=dec,
                 enc_mibs=size / min(enc) / 2**20, dec_mibs=size / min(dec) / 2**20,
                 enc_spread=(max(enc) - min(enc)) / min(enc), dec_spread=(max(dec) - min(dec)) / min(dec),
                 enc_rss_mb=rss_e / 2**20, dec_rss_mb=rss_d / 2**20)
        res["results"][name] = r
        print(f"| {name} | {r['comp_bytes']} | {r['ratio']:.3f} | {r['enc_mibs']:.1f} | {r['enc_spread']*100:.0f}% | "
              f"{r['dec_mibs']:.1f} | {r['dec_spread']*100:.0f}% | {r['enc_rss_mb']:.0f} / {r['dec_rss_mb']:.0f} |",
              flush=True)
        with open(out, "w") as f:
            json.dump(res, f, indent=1)


if __name__ == "__main__":
    main()
