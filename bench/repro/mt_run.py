#!/usr/bin/env python3
"""Command-line comparison on one thread and on all cores (Silesia as one tar), as in the 0.0.2 market report.

usage: mt_run.py TOOLS.json CORPUS_DIR OUT.json [--n 3] [--files silesia.tar]

Every tool is started the same way (fork/exec, wait4), file to file, best of N per direction, each decode compared
byte for byte, peak RSS from wait4. Resumable: tools already in OUT.json are skipped.
Output schema = the report's mt-result.json: {files, n, total_in, results: {name: {ratio, comp_bytes, enc_mibs,
dec_mibs, enc_rss_mb, dec_rss_mb, per_file}}}"""
import argparse, filecmp, json, os, sys, time


def run(argv):
    t = time.perf_counter()
    pid = os.fork()
    if pid == 0:
        fd = os.open("/dev/null", os.O_WRONLY); os.dup2(fd, 1); os.dup2(fd, 2)
        os.execvp(argv[0], argv)
    _, status, ru = os.wait4(pid, 0)
    if status != 0:
        raise SystemExit(f"failed ({status}): {argv}")
    rss = ru.ru_maxrss if sys.platform == "darwin" else ru.ru_maxrss * 1024  # bytes on macOS, KiB on Linux
    return time.perf_counter() - t, rss


def tools(T):
    K = T["kolmo"]["path"]; zs = T["cli"]["zstd"]["path"] or "zstd"; xz = T["cli"]["xz"]["path"] or "xz"
    b3 = T["cli"]["bzip3"]["path"] or "bzip3"; nc = str(os.cpu_count())
    out = []
    for th in ("1", "0"):
        for lv in ("3", "7", "9"):
            out.append((f"kolmo -{lv} -T{th}", [K, "-" + lv, "-T", th, "-q", "-f", "{i}", "-o", "{o}"], [K, "-d", "-T", th, "-q", "-f", "{i}", "-o", "{o}"]))
    for th in ("1", "0"):
        for lv in ("3", "19"):
            out.append((f"zstd -{lv} -T{th}", [zs, "-" + lv, "-T" + th, "-q", "-f", "{i}", "-o", "{o}"], [zs, "-d", "-q", "-f", "{i}", "-o", "{o}"]))
    for th in ("1", "0"):
        out.append((f"xz -9 -T{th}", ["sh", "-c", f"{xz} -9 -T{th} -c '{{i}}' > '{{o}}'"], ["sh", "-c", f"{xz} -d -T{th} -c '{{i}}' > '{{o}}'"]))
    for j in ("1", nc):
        out.append((f"bzip3 -j{j}", ["sh", "-c", f"{b3} -j {j} -c '{{i}}' > '{{o}}'"], ["sh", "-c", f"{b3} -d -j {j} -c '{{i}}' > '{{o}}'"]))
    return out


def main():
    ap = argparse.ArgumentParser(); ap.add_argument("tools"); ap.add_argument("corpus"); ap.add_argument("out")
    ap.add_argument("--n", type=int, default=3); ap.add_argument("--files", default="silesia.tar")
    a = ap.parse_args()
    T = json.load(open(a.tools)); FILES = a.files.split(",")
    work = os.path.join(os.path.dirname(os.path.abspath(a.out)), "work-mt"); os.makedirs(work, exist_ok=True)
    res = json.load(open(a.out)) if os.path.exists(a.out) else {"files": FILES, "n": a.n, "results": {}}
    total_in = sum(os.path.getsize(os.path.join(a.corpus, f)) for f in FILES); res["total_in"] = total_in
    res["cores"] = os.cpu_count()
    for name, ca, da in tools(T):
        if name in res["results"]:
            continue
        per = {}; csize = enc = dec = 0.0; rss_e = rss_d = 0
        for f in FILES:
            src = os.path.join(a.corpus, f); c = os.path.join(work, "c.bin"); d = os.path.join(work, "d.bin")
            be = bd = 1e18
            for _ in range(a.n):
                for p in (c, d):
                    if os.path.exists(p): os.chmod(p, 0o644); os.remove(p)
                te, re_ = run([x.format(i=src, o=c) for x in ca]); be = min(be, te); rss_e = max(rss_e, re_)
                td, rd = run([x.format(i=c, o=d) for x in da]); bd = min(bd, td); rss_d = max(rss_d, rd)
                if not filecmp.cmp(src, d, shallow=False):
                    raise SystemExit(f"MISMATCH {name} {f}")
            sz = os.path.getsize(c); per[f] = dict(size=sz, enc_s=be, dec_s=bd); csize += sz; enc += be; dec += bd
        r = dict(ratio=total_in / csize, comp_bytes=int(csize), enc_mibs=total_in / enc / 2**20, dec_mibs=total_in / dec / 2**20,
                 enc_rss_mb=rss_e / 2**20, dec_rss_mb=rss_d / 2**20, per_file=per)
        res["results"][name] = r
        json.dump(res, open(a.out + ".tmp", "w"), indent=1); os.replace(a.out + ".tmp", a.out)
        print(f"{name:22s} ratio {r['ratio']:.3f}  enc {r['enc_mibs']:8.1f}  dec {r['dec_mibs']:8.1f} MiB/s  "
              f"rss {r['enc_rss_mb']:.0f}/{r['dec_rss_mb']:.0f} MB", flush=True)


if __name__ == "__main__":
    main()
