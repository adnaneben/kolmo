#!/usr/bin/env python3
"""Size ledger and level-order gate: every change to compressed sizes shows up per file, level and data kind.

usage: python3 benchmark/ledger/ledger.py KOLMO [--write] [--base OLD_SIZES_TSV] [--jobs N]

Corpus: the originals already committed under tests/v12 (tests/v12/r0.0.2/in/* and the 0.0.1 originals listed in
tests/v12/MANIFEST), about 12.9 MB and hash-pinned by tests/v12.sha256, so the ledger needs no new data. They cover every
route the planner takes today: KLZH2, graph, audio, video, kimg, KLZMA, block sorting, context mixing (ultra) and
stored blocks, on text, CSV, JSON, logs, FASTQ/FASTA, images, audio, video, numeric arrays, executables, a tar,
mixed binaries and edge cases. One generated input, concat.bin (the r0.0.2 originals concatenated in name order,
12.4 MB), adds multi-block frames: levels 1-9, plus levels 8 and 9 with 4 MiB blocks; at three settings it is also
compressed with 1 and 4 threads, which must give the same frame.

Every corpus file is compressed at levels 1-9 and ultra with one thread. The results are compared with
benchmark/ledger/sizes.tsv (kind, entry, setting, bytes, sha256):
  - FAIL when an entry grows by more than TOL (0.1%, and at least 16 bytes) or shrinks by more than the same amount
    (a stale ledger hides the next regression), when rows are missing or new, or when 1 and 4 threads differ.
    A deliberate change refreshes the ledger in the same PR (--write) and the diff of sizes.tsv is the record.
  - smaller changes and same-size byte changes pass and are listed.
  - the diff is printed per data kind and level (and per entry), and appended to $GITHUB_STEP_SUMMARY when set.
--base OLD: also print the per-kind diff of this build against OLD (CI passes the base branch's sizes.tsv, so a PR
that refreshes the ledger still shows its impact).

Level-order gate (on this build's sizes): for every file, level N+1 (and ultra after 9) is never larger than level N
by more than 0.5% and 16 bytes. Known inversions are listed in benchmark/ledger/level_order_exceptions.txt with a
reason; a listed inversion that no longer occurs is reported as a warning so the list can be trimmed.
"""
import collections
import concurrent.futures
import hashlib
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
V12 = os.path.join(ROOT, "tests", "v12")
LEDGER = os.path.join(HERE, "sizes.tsv")
EXCEPTIONS = os.path.join(HERE, "level_order_exceptions.txt")

TOL_FRAC, TOL_BYTES = 0.001, 16          # ledger: growth or shrink beyond both fails
ORDER_FRAC, ORDER_BYTES = 0.005, 16      # level order: N+1 larger than N beyond both fails

LEVELS = ["-1", "-2", "-3", "-4", "-5", "-6", "-7", "-8", "-9", "-U"]
CONCAT_SETTINGS = LEVELS[:-1] + ["-8 --block-size 4", "-9 --block-size 4"]
THREAD_CHECKS = ["-3", "-7", "-8 --block-size 4"]   # concat.bin: -T1 must equal -T4

# Data kind of each corpus file (the per-kind diff groups by it). A new corpus file needs a line here.
KIND = {
    "text.txt": "text", "tiny.txt": "edge", "empty": "edge", "random.bin": "edge", "zeros.bin": "edge",
    "log.log": "log", "service.log": "log",
    "doc.json": "json", "minified.json": "json", "events.jsonl": "json",
    "table.csv": "csv", "derived.csv": "csv", "growth.csv": "csv", "longfield.csv": "csv", "names.csv": "csv",
    "pairs.csv": "csv", "quoted.csv": "csv", "ragged.csv": "csv", "stations.csv": "csv",
    "reads.fastq": "genomics", "genome.fa": "genomics",
    "img.ppm": "image", "pic.ppm": "image", "rgb.ppm": "image", "rgb-corr.ppm": "image", "gray.pgm": "image",
    "gray-big.pgm": "image", "gray12.pgm": "image", "gray16.pgm": "image", "gray16-big.pgm": "image",
    "photo.pgm": "image", "xray.bin": "image",
    "tone.wav": "audio", "tone.aiff": "audio", "clip.y4m": "video",
    "bf16.st": "numeric", "f64.npy": "numeric", "i64.bin": "numeric", "values.f64": "numeric",
    "values.npy": "numeric", "series.i32": "numeric", "stamps.i64": "numeric",
    "exe.bin": "executable", "code.x86": "executable", "prog-arm64": "executable", "prog-x86_64": "executable",
    "bigprog-arm64": "executable", "bigprog-x86_64": "executable",
    "bundle.tar": "archive", "mixed.bin": "binary", "records.bin": "binary",
    "concat.bin": "multiblock",
}


def corpus():
    """(entry name, path) for every committed original, in a fixed order."""
    files = []
    rin = os.path.join(V12, "r0.0.2", "in")
    for f in sorted(os.listdir(rin)):
        files.append(("r0.0.2/" + f, os.path.join(rin, f)))
    seen = set()
    for line in open(os.path.join(V12, "MANIFEST")):
        parts = line.split()
        if len(parts) >= 2 and "/" not in parts[1]:
            seen.add(parts[1])
    for f in sorted(seen):
        files.append(("v12/" + f, os.path.join(V12, f)))
    return files


def compress(kolmo, path, setting, threads):
    r = subprocess.run([kolmo, *setting.split(), "-T%d" % threads, "-q", "-c", path], capture_output=True)
    if r.returncode:
        raise SystemExit("ledger: %s %s -T%d failed: %s" % (path, setting, threads, r.stderr.decode()[:200]))
    return len(r.stdout), hashlib.sha256(r.stdout).hexdigest()


def kind_of(entry):
    return KIND.get(entry.split("/")[-1], "other")


def read_tsv(path):
    rows = {}
    for line in open(path):
        if line.startswith("#") or not line.strip():
            continue
        kind, entry, setting, size, sha = line.rstrip("\n").split("\t")
        rows[(entry, setting)] = (kind, int(size), sha)
    return rows


def pct(new, old):
    return (new - old) * 100.0 / old if old else (0.0 if new == old else float("inf"))


def kind_table(cur, old, title):
    """Markdown table: bytes per kind and level, old -> new, only where something changed."""
    agg = collections.defaultdict(lambda: [0, 0, 0])      # (kind, level) -> old, new, changed entries
    for key, (kind, size, sha) in cur.items():
        if key not in old:
            continue
        o = old[key]
        level = key[1]
        a = agg[(kind, level)]
        a[0] += o[1]; a[1] += size; a[2] += (o[1] != size)
    lines = []
    for (kind, level), (o, n, c) in sorted(agg.items()):
        if c:
            lines.append("| %s | %s | %d | %d | %+.3f%% | %d |" % (kind, level, o, n, pct(n, o), c))
    if not lines:
        return "%s: no size changes.\n" % title
    return ("%s\n\n| kind | setting | before | after | change | entries changed |\n|---|---|---|---|---|---|\n"
            % title) + "\n".join(lines) + "\n"


def main():
    args = sys.argv[1:]
    if not args or args[0].startswith("-"):
        sys.exit(__doc__)
    kolmo = args[0]
    write = "--write" in args
    base = args[args.index("--base") + 1] if "--base" in args else None
    jobs = int(args[args.index("--jobs") + 1]) if "--jobs" in args else (os.cpu_count() or 2)

    files = corpus()
    with tempfile.TemporaryDirectory() as tmp:
        concat = os.path.join(tmp, "concat.bin")
        with open(concat, "wb") as out:
            for entry, path in files:
                if entry.startswith("r0.0.2/"):
                    out.write(open(path, "rb").read())
        tasks = [(entry, path, s, 1) for entry, path in files for s in LEVELS]
        tasks += [("gen/concat.bin", concat, s, 4) for s in CONCAT_SETTINGS]
        tasks += [("gen/concat.bin", concat, s, 1) for s in THREAD_CHECKS]
        # the slow ones first, so the pool's tail is short
        tasks.sort(key=lambda t: (-(t[0] == "gen/concat.bin"), -("-U" in t[2]), -os.path.getsize(t[1])))
        with concurrent.futures.ThreadPoolExecutor(jobs) as pool:
            res = dict(zip(tasks, pool.map(lambda t: compress(kolmo, t[1], t[2], t[3]), tasks)))

    problems, notes = [], []
    cur = {}
    for (entry, path, s, th), (size, sha) in res.items():
        if entry == "gen/concat.bin" and th == 1:
            continue
        cur[(entry, s)] = (kind_of(entry), size, sha)
    for s in THREAD_CHECKS:
        a, b = res[("gen/concat.bin", concat, s, 1)], res[("gen/concat.bin", concat, s, 4)]
        if a != b:
            problems.append("gen/concat.bin %s: 1 and 4 threads give different frames (%d vs %d bytes)" % (s, a[0], b[0]))
    unknown = sorted({e for (e, _), v in cur.items() if v[0] == "other"})
    if unknown:
        problems.append("corpus files without a kind in ledger.py KIND: %s" % ", ".join(unknown))

    report = []
    if write:
        with open(LEDGER, "w") as f:
            f.write("# kind\tentry\tsetting\tbytes\tsha256  (benchmark/ledger/ledger.py --write; one thread, "
                    "gen/concat.bin with 4)\n")
            for (entry, s), (kind, size, sha) in sorted(cur.items(), key=lambda kv: (kv[1][0], kv[0][0], LEVELS_KEY(kv[0][1]))):
                f.write("%s\t%s\t%s\t%d\t%s\n" % (kind, entry, s, size, sha))
        print("ledger written: %s (%d entries)" % (os.path.relpath(LEDGER, ROOT), len(cur)))
    else:
        old = read_tsv(LEDGER)
        for key in sorted(set(old) - set(cur)):
            problems.append("%s %s: in the ledger but not measured (refresh with --write)" % key)
        for key in sorted(set(cur) - set(old)):
            problems.append("%s %s: measured but not in the ledger (refresh with --write)" % key)
        changed = []
        for key in sorted(set(cur) & set(old), key=lambda k: (k[0], LEVELS_KEY(k[1]))):
            kind, size, sha = cur[key]
            osize, osha = old[key][1], old[key][2]
            if size == osize and sha == osha:
                continue
            d = size - osize
            line = "%-10s %-26s %-18s %9d -> %9d  %+8d B  %+.3f%%" % (kind, key[0], key[1], osize, size, d, pct(size, osize))
            changed.append(line)
            if abs(d) > TOL_BYTES and abs(d) > TOL_FRAC * osize:
                if d > 0:
                    problems.append("GREW beyond tolerance: " + line)
                else:
                    problems.append("shrank beyond tolerance (the ledger is stale; refresh it with --write in this PR): " + line)
            elif d == 0:
                notes.append("same size, different bytes: " + line)
        report.append(kind_table(cur, old, "### Size ledger: this build against benchmark/ledger/sizes.tsv"))
        if changed:
            print("entries that differ from the ledger:")
            for line in changed:
                print("  " + line)
    if base:
        report.append(kind_table(cur, read_tsv(base), "### Size ledger: this build against the base branch's ledger"))

    # level-order gate
    exc = {}
    for line in open(EXCEPTIONS):
        line = line.split("#", 1)[0].strip()
        if line:
            entry, pair = line.split()[:2]
            exc[(entry, pair)] = False
    inversions = []
    entries = sorted({e for e, _ in cur})
    for entry in entries:
        chain = LEVELS if entry != "gen/concat.bin" else LEVELS[:-1]
        for lo, hi in zip(chain, chain[1:]):
            a, b = cur[(entry, lo)][1], cur[(entry, hi)][1]
            if b > a * (1 + ORDER_FRAC) and b - a > ORDER_BYTES:
                pair = "%s>%s" % (hi, lo)
                line = "%-26s %-7s %9d > %9d  %+.2f%%" % (entry, pair, b, a, pct(b, a))
                if (entry, pair) in exc:
                    exc[(entry, pair)] = True
                    inversions.append("  known: " + line)
                else:
                    problems.append("LEVEL ORDER: " + line)
    if inversions:
        print("known level-order inversions (benchmark/ledger/level_order_exceptions.txt):")
        print("\n".join(inversions))
    for (entry, pair), hit in sorted(exc.items()):
        if not hit:
            msg = "level-order exception %s %s no longer occurs: remove it from level_order_exceptions.txt" % (entry, pair)
            print(("::warning::" if os.environ.get("GITHUB_ACTIONS") else "warning: ") + msg)

    text = "\n".join(report)
    print(text)
    for n in notes:
        print(n)
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a") as f:
            f.write(text + "\n")
            if problems:
                f.write("\n**Ledger problems:**\n\n```\n" + "\n".join(problems) + "\n```\n")
    for p in problems:
        print(p)
    if problems:
        sys.exit("SIZE LEDGER FAILED (%d problems over %d entries)" % (len(problems), len(cur)))
    print("SIZE LEDGER PASSED (%d entries, %d level-order inversions known)" % (len(cur), len(inversions)))


def LEVELS_KEY(s):
    return (CONCAT_SETTINGS + LEVELS).index(s) if s in CONCAT_SETTINGS + LEVELS else 99


if __name__ == "__main__":
    main()
