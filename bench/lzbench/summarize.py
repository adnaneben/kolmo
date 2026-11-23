#!/usr/bin/env python3
"""Summarise repeated lzbench runs (CSV, -o4) and check the market-benchmark claims.

    python3 -I summarize.py <results-dir> [--mac <dir of the Mac CSVs>] [--out summary.md]

<results-dir> holds files named lzb-<tier>-<corpus>.run<k>.csv (one per run) as bench-x86.yml writes them; files
without .run<k> count as a single run (so the Mac directory, evidence/mkt-0.0.2, reads the same way).

For every corpus and codec setting it reports the compressed size (which must be the same in every run: lzbench
checks each round trip, and a size that changes between runs is a bug), and the median, minimum and maximum of the
compression and decompression speeds over the runs. Speeds are lzbench's (MB/s, best iteration of each run).
With --mac it compares every compressed size with the Mac's: the formats are deterministic, so any difference in a
codec's size is flagged (for kolmo it is a bug).
"""
import csv, glob, os, re, statistics, sys

CLAIMS = [  # (label, kolmo level, rival prefix, rival level, what the report says on Silesia)
    ("Level 6 vs xz -9", 6, "xz", 9, "smaller than xz -9, compresses 1.7x faster, decodes 8.6x faster"),
    ("Level 4 vs zstd -19", 4, "zstd", 19, "within 1.6% of zstd -19's ratio, compresses 13x faster"),
    ("Level 9 vs zpaq -5", 9, "zpaq", 5, "matches zpaq -5's ratio, compresses 14x and decodes 120x faster"),
    ("Level 3 vs zstd -12", 3, "zstd", 12, "zstd -12's ratio at 5x its compress speed"),
]


def load(d):
    """{corpus: {setting: {"size": set, "orig": int, "c": [..], "d": [..]}}}"""
    out = {}
    for path in sorted(glob.glob(os.path.join(d, "lzb-*.csv"))):
        m = re.match(r"lzb-(fast|slow)-(.+?)(?:\.run\d+)?\.csv$", os.path.basename(path))
        if not m:
            continue
        corpus = m.group(2)
        with open(path, newline="") as f:
            for row in csv.DictReader(f):
                name = row["Compressor name"].strip()
                if name.startswith("memcpy"):
                    continue
                e = out.setdefault(corpus, {}).setdefault(name, {"size": set(), "orig": 0, "c": [], "d": []})
                e["size"].add(int(row["Compressed size"]))
                e["orig"] = int(row["Original size"])
                e["c"].append(float(row["Compression speed"]))
                e["d"].append(float(row["Decompression speed"]))
    return out


def find(settings, prefix, level):
    for name, e in settings.items():
        p = name.split()
        if p[0] == prefix and p[-1] == f"-{level}":
            return e
    return None


def med(x):
    return statistics.median(x) if x else float("nan")


def spread(x):
    m = med(x)
    return (max(x) - min(x)) / m * 100 if len(x) > 1 and m else 0.0


def fmt_speed(x):
    s = f"{med(x):.3g}"
    return s + (f" ({min(x):.3g}-{max(x):.3g})" if len(x) > 1 else "")


def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    res = load(a[0])
    mac = load(a[a.index("--mac") + 1]) if "--mac" in a else None
    out = a[a.index("--out") + 1] if "--out" in a else None
    L, bugs = [], []
    runs = max((len(e["c"]) for s in res.values() for e in s.values()), default=0)
    L.append(f"# lzbench summary ({runs} run(s) per setting)\n")
    L.append("Speeds: lzbench MB/s, median over the runs (min-max in brackets). Shared CI runners are noisy: read "
             "speed differences under the spread shown as ties. Ratios are exact and identical in every run.\n")
    for corpus, settings in res.items():
        L.append(f"\n## {corpus}\n")
        L.append("| setting | compressed B | ratio | compress | spread | decompress | spread |" +
                 (" Mac size |" if mac else ""))
        L.append("|---|---:|---:|---|---:|---|---:|" + ("---|" if mac else ""))
        for name, e in settings.items():
            if len(e["size"]) != 1:
                bugs.append(f"{corpus}: {name}: compressed size differs between runs: {sorted(e['size'])}")
            size = min(e["size"])
            cell = ""
            if mac:
                me = mac.get(corpus, {}).get(name)
                if me is None:
                    cell = " — |"
                elif min(me["size"]) == size:
                    cell = " same |"
                else:
                    cell = f" **{min(me['size'])}** |"
                    bugs.append(f"{corpus}: {name}: {size} B here, {min(me['size'])} B on the Mac")
            L.append(f"| {name} | {size} | {e['orig'] / size:.3f} | {fmt_speed(e['c'])} | {spread(e['c']):.0f}% | "
                     f"{fmt_speed(e['d'])} | {spread(e['d']):.0f}% |" + cell)

    L.append("\n## Claims of the market report, per corpus\n")
    L.append("Ratio gap = Kolmo's ratio / rival's ratio - 1. Speed factors = Kolmo's median speed / rival's.\n")
    for label, kl, rp, rl, said in CLAIMS:
        L.append(f"\n### {label} (Mac report on Silesia: {said})\n")
        L.append("| corpus | kolmo ratio | rival ratio | ratio gap | compress x | decompress x |" +
                 (" Mac: ratio gap, compress x, decompress x |" if mac else ""))
        L.append("|---|---:|---:|---:|---:|---:|" + ("---|" if mac else ""))
        for corpus, settings in res.items():
            k, r = find(settings, "kolmo", kl), find(settings, rp, rl)
            if not k or not r:
                continue

            def row(k, r):
                kr, rr = k["orig"] / min(k["size"]), r["orig"] / min(r["size"])
                return kr, rr, (kr / rr - 1) * 100, med(k["c"]) / med(r["c"]), med(k["d"]) / med(r["d"])
            kr, rr, gap, cx, dx = row(k, r)
            cell = ""
            if mac and corpus in mac:
                mk, mr = find(mac[corpus], "kolmo", kl), find(mac[corpus], rp, rl)
                if mk and mr:
                    _, _, mg, mcx, mdx = row(mk, mr)
                    cell = f" {mg:+.2f}%, {mcx:.2f}x, {mdx:.2f}x |"
            L.append(f"| {corpus} | {kr:.3f} | {rr:.3f} | {gap:+.2f}% | {cx:.2f}x | {dx:.2f}x |" + cell)
    L.append("\n## Problems\n")
    L.extend(["- " + b for b in bugs] or ["None: every size was the same in every run" +
                                         (" and the same as on the Mac." if mac else ".")])
    text = "\n".join(L) + "\n"
    if out:
        with open(out, "w") as f:
            f.write(text)
    print(text)
    sys.exit(1 if any("kolmo" in b for b in bugs) else 0)


if __name__ == "__main__":
    main()
