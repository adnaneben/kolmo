#!/usr/bin/env python3
"""Tiers S and B of benchmark/rc1/PROTOCOL.md, whole-process part: command-line tools, one thread each.

usage: run_cli.py --kolmo <kolmo cli> --zli <OpenZL zli> --data <dir> --fresh <dir> --work <dir> --out <out.jsonl>
                  [--only <regex on "file setting">] [--list]

OpenZL's format profiles (CSV, NumPy, pickle, model files, typed integers, the SAO catalogue) and its trained
compressors exist only in its command-line tool `zli`, and 7-Zip's executable filters only in `7zz`. Neither can be
timed inside lzbench, so here every tool, kolmo included, is run as a user runs it: one process per compression and
one per decompression, file to file. A time therefore contains process start, reading and writing as well as the
codec. `zstd -3` is run the same way so that these rows can be set beside the in-memory rows of tier A.

Each (file, setting) is run in up to three rounds, the settings of a file one after another in each round; a setting
that has already used 20 s is not run again. The fastest round is kept, as in the other tiers. The process's own CPU
time and peak memory are read when it ends (wait4): CPU time above elapsed time means that more than one thread
worked, and the row is marked `threads`. Every output is decompressed and compared with the input.
Rows already in the output file are skipped on a restart; marked and failed rows are run again.
"""
import argparse
import filecmp
import hashlib
import json
import os
import re
import subprocess
import sys
import time

ROUNDS, BUDGET_S, ULTRA_MAX = 3, 20.0, 16 << 20
KOLMO = ["-m balanced", "-m balanced --native-leaf", "-m hc", "-m ratio", "-x"]   # -x only on files up to 16 MiB

CSV = [["-p", "csv"]]
FILES = [  # file, directory key, group, zli profiles
    ("owid_test.csv", "data", "tables (CSV)", CSV), ("energy_test.csv", "data", "tables (CSV)", CSV),
    ("owid-co2-data.csv", "data", "tables (CSV)", CSV), ("wide_test.csv", "data", "tables (CSV)", [["-p", "csv", "--profile-arg", ";"]]),
    ("appl.test", "fresh", "tables (CSV), OpenZL trained on other rows", CSV),
    ("quakes2.test", "fresh", "tables (CSV), OpenZL trained on other rows", CSV),
    ("rides2.test", "fresh", "tables (CSV), OpenZL trained on other rows", CSV),
    ("mr", "data", "16-bit medical images", [["-p", "le-u16"], ["-p", "be-u16"]]),
    ("x-ray", "data", "16-bit medical images", [["-p", "le-u16"], ["-p", "be-u16"]]),
    ("sao", "data", "star catalogue", [["-p", "sao"], ["-p", "sddl2", "--profile-arg", "@SDDL@"]]),
    ("img_astronaut.u8", "data", "raw 8-bit images", [["-p", "u8"]]), ("img_camera.u8", "data", "raw 8-bit images", [["-p", "u8"]]),
    ("year.i32", "data", "integer arrays", [["-p", "le-i32"]]), ("population.i64", "data", "integer arrays", [["-p", "le-i64"]]),
    ("log_timestamps_ms.i64", "data", "integer arrays", [["-p", "le-i64"]]),
    ("lfw_subset.npy", "data", "NumPy arrays", [["-p", "numpy"]]), ("owid_energy_f64.npy", "data", "NumPy arrays", [["-p", "numpy"]]),
    ("owid_energy.pkl", "data", "pickle", [["-p", "pickle"]]),
    ("magika.onnx", "data", "model weights", [["-p", "onnx"]]), ("magika.gguf", "data", "model weights", [["-p", "gguf"]]),
    ("magika.safetensors", "data", "model weights", [["-p", "safetensors"]]),
    ("llm_bf16_10mb.safetensors", "data", "model weights", [["-p", "safetensors"]]),
    ("llm_nvfp4_17mb.safetensors", "data", "model weights", [["-p", "safetensors"]]),
    ("llm_bf16_47mb.safetensors", "data", "model weights", [["-p", "safetensors"]]),
    # 7-Zip with its executable filters: no OpenZL profile
    ("ooffice", "data", "executables and mixed", []), ("mozilla", "data", "executables and mixed", []),
    ("samba", "data", "executables and mixed", []),
]


def settings(a, name, key, group, profiles, n):
    """-> [(label, compress command, decompress command, decompress writes to stdout)]; @IN@ @Z@ @BACK@ are filled in later"""
    out = [("zstd -3", ["zstd", "-q", "-f", "--single-thread", "-3", "@IN@", "-o", "@Z@"],
            ["zstd", "-q", "-f", "-d", "--single-thread", "@Z@", "-o", "@BACK@"], False)]
    for flags in KOLMO:
        if flags == "-x" and n > ULTRA_MAX:
            continue
        out.append(("kolmo " + flags, [a.kolmo, "-T1"] + flags.split() + ["@IN@", "@Z@"], [a.kolmo, "-d", "-T1", "@Z@", "@BACK@"], False))
    sddl = os.path.join(a.fresh, "openzl", "examples", "sddl2", "sao_silesia.sddl")
    unzl = [a.zli, "decompress", "-f", "@Z@", "-o", "@BACK@"]
    for prof in profiles:
        prof = [sddl if x == "@SDDL@" else x for x in prof]
        shown = " ".join("<sao_silesia.sddl>" if x == sddl else x for x in prof)
        for level in (6, 9):
            out.append(("zli %s -l %d" % (shown, level), [a.zli, "compress"] + prof + ["-l", str(level), "-f", "@IN@", "-o", "@Z@"], unzl, False))
    if key == "fresh":
        zlc = os.path.join(a.fresh, "profiles", name.split(".")[0] + ".zlc")
        out.append(("zli trained compressor", [a.zli, "compress", "--compressor", zlc, "-f", "@IN@", "-o", "@Z@"], unzl, False))
    if group.startswith("executables"):
        # -myx9 makes 7-Zip look inside every file to pick its executable filter (the inputs have no file extension).
        # With BCJ2, 7-Zip runs the filter's coders beside the one LZMA2 thread; -mmtf=off keeps everything on one thread.
        # 7-Zip's xz writer is left out: it uses two threads whatever -mmt says.
        for label, extra in (("7zz -t7z -mx9 -myx9 -mmt1 (LZMA2, filter picked by 7-Zip)", ["-myx9"]),
                             ("7zz -t7z -mx9 -myx9 -mmt1 -mmtf=off (the same, filter on the one thread)", ["-myx9", "-mmtf=off"]),
                             ("7zz -t7z -mx9 -mf=off -mmt1 (LZMA2, no filter)", ["-mf=off"])):
            out.append((label, ["7zz", "a", "-t7z", "-mx9"] + extra + ["-mmt1", "-bso0", "-bsp0", "@Z@.7z", "@IN@"],
                        ["7zz", "x", "-so", "-mmt1"] + [x for x in extra if x == "-mmtf=off"] + ["-bso0", "-bsp0", "@Z@.7z"], True))
    return out


def run(cmd, stdout_path=None):
    """-> (exit code, elapsed s, CPU s, peak memory in bytes, last bytes of stderr)"""
    so = open(stdout_path, "wb") if stdout_path else subprocess.DEVNULL
    t0 = time.perf_counter()
    p = subprocess.Popen(cmd, stdin=subprocess.DEVNULL, stdout=so, stderr=subprocess.PIPE)
    err = p.stderr.read()
    _, status, ru = os.wait4(p.pid, 0)
    t = time.perf_counter() - t0
    p.returncode = os.waitstatus_to_exitcode(status)
    if stdout_path:
        so.close()
    rss = ru.ru_maxrss if sys.platform == "darwin" else ru.ru_maxrss * 1024
    return p.returncode, t, ru.ru_utime + ru.ru_stime, rss, err[-200:].decode("utf-8", "replace")


def watcher_running():
    return subprocess.run(["pgrep", "-f", "bench_mac3"], stdout=subprocess.DEVNULL).returncode == 0


def others():
    """Other processes that used at least half a core just now: [name, percent]. Recorded, not acted on."""
    out = []
    for line in subprocess.run(["ps", "-Ao", "pcpu=,pid=,comm="], stdout=subprocess.PIPE, text=True).stdout.splitlines():
        x = line.split(None, 2)
        try:
            pc, pid = float(x[0].replace(",", ".")), int(x[1])
        except (ValueError, IndexError):
            continue
        if pc >= 50 and pid != os.getpid() and len(x) == 3:
            out.append([os.path.basename(x[2].strip()), pc])
    return out


def version(cmd):
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL, text=True)
        return " ".join(p.stdout.split())[:120]
    except OSError as e:
        return "not found: %s" % e


def main():
    ap = argparse.ArgumentParser()
    for k in ("kolmo", "zli", "data", "fresh", "work", "out"):
        ap.add_argument("--" + k, required=True)
    ap.add_argument("--only")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    dirs = {"data": a.data, "fresh": os.path.join(a.fresh, "data")}
    plan = []
    for name, key, group, profiles in FILES:
        src = os.path.join(dirs[key], name)
        sets = settings(a, name, key, group, profiles, os.path.getsize(src))
        if a.only:
            sets = [s for s in sets if re.search(a.only, name + " " + s[0])]
        plan.append((name, group, src, sets))
    if a.list:
        for name, group, src, sets in plan:
            for s in sets:
                print(name, "|", s[0])
        print(sum(len(p[3]) for p in plan), "rows")
        return
    os.makedirs(a.work, exist_ok=True)
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    done = set()
    if os.path.exists(a.out):
        for line in open(a.out):
            r = json.loads(line)
            if r.get("type") == "row" and "error" not in r and not r.get("threads") and not r.get("busy"):
                done.add((r["file"], r["set"]))
    log = open(a.out, "a")

    def write(obj):
        log.write(json.dumps(obj) + "\n")
        log.flush()
        os.fsync(log.fileno())

    write({"type": "env", "when": time.strftime("%Y-%m-%d %H:%M:%S"), "kolmo": a.kolmo,
           "kolmo_sha256": hashlib.sha256(open(a.kolmo, "rb").read()).hexdigest(),
           "zli": version([a.zli, "--version"]), "zstd": version(["zstd", "--version"]), "7zz": version(["7zz", "i"])[:60],
           "load": os.getloadavg(), "watcher_running": watcher_running(), "rows_done_before": len(done)})
    z, back = os.path.join(a.work, "z"), os.path.join(a.work, "back")
    for name, group, src, sets in plan:
        n = os.path.getsize(src)
        sets = [s for s in sets if (name, s[0]) not in done]
        rows = {s[0]: {"type": "row", "group": group, "file": name, "n": n, "set": s[0], "c": [], "d": [], "c_cpu": [], "d_cpu": [],
                       "c_rss": 0, "d_rss": 0, "threads": False, "busy": False, "others": []} for s in sets}
        for rnd in range(ROUNDS):
            for label, ccmd, dcmd, to_stdout in sets:
                r = rows[label]
                if "error" in r or (rnd and sum(r["c"]) + sum(r["d"]) >= BUDGET_S):
                    continue
                fill = lambda cmd: [x.replace("@IN@", src).replace("@Z@", z).replace("@BACK@", back) for x in cmd]   # noqa: E731
                zpath = next((x for x in fill(ccmd) if x.startswith(z)), z)
                for path in (zpath, back):
                    if os.path.exists(path):
                        os.remove(path)
                r["busy"] = r["busy"] or watcher_running()
                for o in others():
                    if o[0] not in [x[0] for x in r["others"]]:
                        r["others"].append(o)
                rc, t, cpu, rss, err = run(fill(ccmd))
                if rc != 0 or not os.path.exists(zpath):
                    r["error"] = "compression failed (exit %d): %s" % (rc, err)
                    continue
                size = os.path.getsize(zpath)
                if r.setdefault("size", size) != size:
                    r["error"] = "the size changed between rounds: %d and %d" % (r["size"], size)
                    continue
                rc2, t2, cpu2, rss2, err2 = run(fill(dcmd), back if to_stdout else None)
                if rc2 != 0 or not os.path.exists(back) or not filecmp.cmp(src, back, shallow=False):
                    r["error"] = "round trip failed (exit %d): %s" % (rc2, err2)
                    continue
                r["c"].append(round(t, 4)); r["d"].append(round(t2, 4)); r["c_cpu"].append(round(cpu, 4)); r["d_cpu"].append(round(cpu2, 4))
                r["c_rss"], r["d_rss"] = max(r["c_rss"], rss), max(r["d_rss"], rss2)
                r["threads"] = r["threads"] or cpu > t * 1.03 + 0.05 or cpu2 > t2 * 1.03 + 0.05
                r["busy"] = r["busy"] or watcher_running()
        for label, _, _, _ in sets:
            r = rows[label]
            r["when"] = time.strftime("%H:%M:%S")
            if "error" not in r:
                r["c_s"], r["d_s"] = min(r["c"]), min(r["d"])
            write(r)
            if "error" in r:
                print("%-26s %-52s FAILED %s" % (name, label, r["error"].replace("\n", " ")), flush=True)
            else:
                print("%-26s %-52s %9d  c %8.2f  d %8.2f MB/s  mem %4.0f / %4.0f MB%s%s" % (
                    name, label, r["size"], n / 1e6 / r["c_s"], n / 1e6 / r["d_s"], r["c_rss"] / 1e6, r["d_rss"] / 1e6,
                    " THREADS" if r["threads"] else "", " BUSY" if r["busy"] else ""), flush=True)
    write({"type": "end", "when": time.strftime("%Y-%m-%d %H:%M:%S"), "watcher_running": watcher_running(), "load": os.getloadavg()})
    print("finished", time.strftime("%H:%M:%S"), flush=True)


if __name__ == "__main__":
    main()
