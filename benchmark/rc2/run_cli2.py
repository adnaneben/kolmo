"""Tier C of the rc2 run: whole processes, each tool limited to one thread, on one Mac.

    python3 run_cli2.py --tools tools.json --group NAME --files F1 F2 ... --work DIR --out rows.jsonl

tools.json maps a group name to a list of settings: {"label", "c": [argv], "d": [argv]} where the argv items may use
{in}, {out}, {back}; an argv ending in ">" writes the program's standard output to the target file instead.
Each setting compresses and decompresses each file in fresh processes. Times come from wait4 (elapsed, user+system
CPU, peak RSS). The fastest of up to three rounds counts; a setting stops after a round once 20 s were spent on it.
A row whose CPU time is above its elapsed time (by 3% and 0.05 s) is marked `threads`. The decoded bytes are
compared with the input (SHA-256). Rows already in the output file are kept, so a stopped run resumes.
"""
import argparse
import hashlib
import json
import os
import platform
import subprocess
import sys
import time


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def run(argv):
    """-> (elapsed s, cpu s, peak rss bytes, return code, stderr tail)"""
    out = None
    if argv and argv[-2:-1] == [">"]:
        argv, out = argv[:-2], argv[-1]
    fo = open(out, "wb") if out else subprocess.DEVNULL
    t0 = time.perf_counter()
    p = subprocess.Popen(argv, stdout=fo, stderr=subprocess.PIPE)
    _, status, ru = os.wait4(p.pid, 0)
    t = time.perf_counter() - t0
    err = p.stderr.read().decode(errors="replace")[-300:]
    p.stderr.close()
    if out:
        fo.close()
    rss = ru.ru_maxrss if sys.platform == "darwin" else ru.ru_maxrss * 1024
    return t, ru.ru_utime + ru.ru_stime, rss, os.waitstatus_to_exitcode(status), err


def others():
    out = []
    for line in subprocess.run(["ps", "-Ao", "pcpu=,pid=,comm="], stdout=subprocess.PIPE, text=True).stdout.splitlines():
        a = line.split(None, 2)
        try:
            if float(a[0].replace(",", ".")) >= 50 and int(a[1]) != os.getpid():
                out.append(os.path.basename(a[2].strip()))
        except (ValueError, IndexError):
            continue
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tools", required=True)
    ap.add_argument("--group", required=True)
    ap.add_argument("--files", nargs="+", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--budget", type=float, default=20.0)
    a = ap.parse_args()
    settings = json.load(open(a.tools))[a.group]
    os.makedirs(a.work, exist_ok=True)
    done = set()
    if os.path.exists(a.out):
        for line in open(a.out):
            r = json.loads(line)
            if r.get("type") == "row" and "error" not in r:
                done.add((r["group"], r["file"], r["label"]))
    with open(a.out, "a") as log:
        log.write(json.dumps({"type": "env", "group": a.group, "when": time.strftime("%Y-%m-%d %H:%M:%S"),
                              "machine": platform.machine(), "os": platform.platform(), "load": os.getloadavg()}) + "\n")
        for path in a.files:
            name, n, ref = os.path.basename(path), os.path.getsize(path), sha(path)
            for s in settings:
                if (a.group, name, s["label"]) in done:
                    continue
                z = os.path.join(a.work, "c.bin")
                back = os.path.join(a.work, "back.bin")
                sub = lambda argv: [x.format(**{"in": path, "out": z, "back": back}) for x in argv]
                row = {"type": "row", "group": a.group, "file": name, "label": s["label"], "input": n,
                       "busy_before": others()}
                best_c = best_d = None
                spent, rounds = 0.0, 0
                while rounds < 3 and (rounds == 0 or spent < a.budget):
                    for f in (z, back):
                        if os.path.exists(f):
                            os.remove(f)
                    tc, cc, mc, rc, ec = run(sub(s["c"]))
                    if rc or not os.path.exists(z):
                        row["error"] = f"compress exit {rc}: {ec}"
                        break
                    td, cd, md, rd, ed = run(sub(s["d"]))
                    if rd or not os.path.exists(back) or sha(back) != ref:
                        row["error"] = f"decompress exit {rd} or bytes differ: {ed}"
                        break
                    spent += tc + td
                    rounds += 1
                    size = os.path.getsize(z)
                    if best_c is None or tc < best_c[0]:
                        best_c = (tc, cc, mc)
                    if best_d is None or td < best_d[0]:
                        best_d = (td, cd, md)
                    thr = cc > tc * 1.03 + 0.05 or cd > td * 1.03 + 0.05
                    row["threads"] = row.get("threads", False) or thr
                if "error" not in row:
                    row.update(compressed=size, rounds=rounds, c_s=best_c[0], c_cpu=best_c[1], c_rss=best_c[2],
                               d_s=best_d[0], d_cpu=best_d[1], d_rss=best_d[2],
                               c_mbs=n / best_c[0] / 1e6, d_mbs=n / best_d[0] / 1e6)
                row["busy_after"] = others()
                log.write(json.dumps(row) + "\n")
                log.flush()
                print(f"{name:24s} {s['label']:34s} " + (f"{row['compressed']:>12,} c {row['c_mbs']:8.2f} d {row['d_mbs']:8.2f} MB/s"
                      + (" THREADS" if row.get("threads") else "") if "error" not in row else "ERROR " + row["error"][:80]), flush=True)


if __name__ == "__main__":
    main()
