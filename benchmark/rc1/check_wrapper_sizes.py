#!/usr/bin/env python3
"""Check that lzbench's kolmo entries write exactly as many bytes as the command-line tool.

usage: check_wrapper_sizes.py <lzbench> <kolmo cli> <new work dir> <out.json> <file> [file ...]

For each file and each lzbench level 0..17 this compares
    lzbench -ekolmo,L        the release library
    lzbench -ekolmo_ctl,L    the control library (same encoder, generic integer inverse in the decoder)
    kolmo -T1 <flags of L>   the command-line tool of the same commit
lzbench also decodes every frame and compares it with the input, so a row here is a verified round trip.
Nothing is timed. Exit status 1 if any size differs or any run fails.
"""
import concurrent.futures
import csv
import io
import json
import os
import subprocess
import sys

# The command-line flags behind each lzbench level (benchmark/lzbench/kolmo_lzbench_dl.cpp holds the same table).
FLAGS = [
    "-m rt", "-m fast -l 1 -q", "-m fast -l 2 -q", "-m fast -q", "-m fast", "-m hc -l 9", "-m hc -l 12", "-m max -l 1",
    "-m eco", "-m hc", "-m max", "-x -l 1", "-x", "-x -l 2", "-m balanced --native-leaf", "-m balanced --typed-huf",
    "-m speed", "-m fast -l 1 -Q",
]


def lzbench_sizes(lzbench, path):
    levels = ",".join(str(i) for i in range(len(FLAGS)))
    cmd = [lzbench, "-o4", "-t0,0", "-i1,1", "-ekolmo,%s/kolmo_ctl,%s" % (levels, levels), path]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    out = {}
    if p.returncode != 0 or "ERROR" in p.stdout or "ERROR" in p.stderr:
        return out, "lzbench exit %d: %s" % (p.returncode, (p.stdout + p.stderr)[-300:])
    for row in csv.reader(io.StringIO(p.stdout)):
        if len(row) >= 7 and row[0].startswith("kolmo"):
            name, level = row[0].split(" ")[0], int(row[0].rsplit("-", 1)[1])
            out[(name, level)] = int(row[4])
    return out, ""


def cli_size(kolmo, flags, path, tmp):
    p = subprocess.run([kolmo, "-T1"] + flags.split() + [path, tmp], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if p.returncode != 0:
        return None
    n = os.path.getsize(tmp)
    os.remove(tmp)
    return n


def one_file(lzbench, kolmo, work, path):
    base = os.path.basename(path)
    sizes, err = lzbench_sizes(lzbench, path)
    rows = []
    for level, flags in enumerate(FLAGS):
        c = cli_size(kolmo, flags, path, os.path.join(work, "%s.%d.kolmo" % (base, level)))
        a, b = sizes.get(("kolmo", level)), sizes.get(("kolmo_ctl", level))
        rows.append({"file": base, "n": os.path.getsize(path), "level": level, "flags": flags, "lzbench": a,
                     "lzbench_ctl": b, "cli": c, "same": a is not None and a == b == c})
    return rows, err


def main():
    if len(sys.argv) < 6:
        sys.exit(__doc__)
    lzbench, kolmo, work, out = sys.argv[1:5]
    os.mkdir(work)   # must be new
    rows, bad = [], 0
    with concurrent.futures.ThreadPoolExecutor(3) as ex:
        for r, err in ex.map(lambda f: one_file(lzbench, kolmo, work, f), sys.argv[5:]):
            rows += r
            if err:
                print(err)
                bad += 1
    for r in rows:
        if not r["same"]:
            bad += 1
            print("DIFFERENT %-26s level %2d (%s): lzbench %s, control %s, cli %s" %
                  (r["file"], r["level"], r["flags"], r["lzbench"], r["lzbench_ctl"], r["cli"]))
    with open(out, "w") as f:
        json.dump(rows, f, indent=1)
    print("%d files x %d levels: %d rows, %d not the same" % (len(sys.argv) - 5, len(FLAGS), len(rows), bad))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
