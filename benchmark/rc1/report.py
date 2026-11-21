#!/usr/bin/env python3
"""Turn the result files of benchmark/rc1 into the tables of the report (Markdown).

usage: report.py --dir <dir with A1.jsonl A2.jsonl A3.jsonl> [--inprocess <jsonl>] [--cli <jsonl>] --out <tables.md>

Nothing is measured here. Every number in the tables is read from the result files or computed from them:
    size difference   kolmo's size / the rival's size - 1
    speed ratio       kolmo's speed / the rival's speed, from the fastest iteration of each
    a group of files  sizes are added; speed is the total of the bytes divided by the total of the fastest times
Words: sizes within 1% and speeds within 10% count as the same. "Ahead" is smaller and not slower; "behind" is larger,
or the same size and slower. A result that is smaller but slower is called a trade, with its numbers.
"""
import argparse
import collections
import json
import os

FLAGS = ["-m rt", "-m fast -l 1 -q", "-m fast -l 2 -q", "-m fast -q", "-m balanced", "-m hc -l 9", "-m hc -l 12", "-m max -l 1",
         "-m eco", "-m hc", "-m ratio", "-x -l 1", "-x", "-x -l 2", "-m balanced --native-leaf", "-m balanced --typed-huf",
         "-m speed", "-m fast -l 1 -Q"]
A1_ORDER = ["memcpy", "lz4", "kolmo,16", "kolmo,0", "lzav,1", "zxc,1", "zxc,3", "snappy", "lzfse", "zstd,1", "kolmo,17", "kolmo,1",
            "kolmo,3", "zstd,3", "kolmo,4", "kolmo_ctl,4", "libdeflate,6", "zlib-ng,6", "brotli,5", "kolmo,8", "zstd,9", "kolmo,5",
            "zstd,19", "zstd,22", "kolmo,9", "brotli,11", "xz,9", "lzma,9", "kolmo,10", "kolmo,7", "bsc0", "bsc1", "bzip3,5",
            "bzip3,10", "ppmd8,4", "kanzi,7", "kanzi,9", "zpaq,5", "kolmo,11", "kolmo,12", "kolmo,13"]
AREAS = [  # number, name, [(kolmo entry, rival entry)]; the first pair of each area was fixed before measuring
    (1, "Lowest latency", [("kolmo,16", "lz4"), ("kolmo,16", "lzav,1"), ("kolmo,16", "zxc,1"), ("kolmo,16", "zxc,3"),
                           ("kolmo,16", "snappy"), ("kolmo,16", "lzfse"), ("kolmo,0", "lz4")]),
    (2, "Fast general purpose", [("kolmo,17", "zstd,1"), ("kolmo,1", "zstd,1")]),
    (3, "Balanced default", [("kolmo,4", "zstd,3"), ("kolmo,3", "zstd,3")]),
    (4, "DEFLATE", [("kolmo,4", "libdeflate,6"), ("kolmo,4", "zlib-ng,6")]),
    (5, "Brotli", [("kolmo,4", "brotli,5"), ("kolmo,9", "brotli,11"), ("kolmo,8", "brotli,5")]),
    (6, "High compression, quick decompression", [("kolmo,9", "zstd,19"), ("kolmo,9", "zstd,22"), ("kolmo,5", "zstd,9")]),
    (7, "LZMA2", [("kolmo,10", "xz,9"), ("kolmo,10", "lzma,9"), ("kolmo,9", "xz,9")]),
    (8, "Strong practical compressors", [("kolmo,10", "bsc0"), ("kolmo,10", "bsc1"), ("kolmo,10", "bzip3,5"), ("kolmo,10", "bzip3,10"),
                                         ("kolmo,10", "kanzi,9"), ("kolmo,10", "kanzi,7"), ("kolmo,10", "ppmd8,4"), ("kolmo,7", "bsc0")]),
    (9, "Extreme ratio", [("kolmo,12", "zpaq,5"), ("kolmo,13", "zpaq,5"), ("kolmo,11", "zpaq,5"), ("kolmo,12", "kanzi,9")]),
]
GROUPS = [
    ("Plain text", ["dickens", "reymont", "webster", "ld30.txt"]),
    ("Executables and mixed", ["mozilla", "ooffice", "samba"]),
    ("Database and markup", ["nci", "osdb", "xml"]),
    ("JSON", ["api.json", "owid_arr.json"]),
    ("Log", ["loghub_all.log"]),
    ("DNA text", ["chr22.fa"]),
    ("16-bit medical images", ["mr", "x-ray"]),
    ("Star catalogue", ["sao"]),
    ("Tables (CSV)", ["owid_test.csv", "energy_test.csv", "owid-co2-data.csv", "wide_test.csv"]),
    ("Raw 8-bit images", ["img_astronaut.u8", "img_camera.u8"]),
    ("Integer arrays", ["year.i32", "population.i64", "log_timestamps_ms.i64"]),
    ("NumPy arrays", ["lfw_subset.npy", "owid_energy_f64.npy"]),
    ("Pickle", ["owid_energy.pkl"]),
    ("Model weights", ["magika.onnx", "magika.gguf", "magika.safetensors", "llm_bf16_10mb.safetensors",
                       "llm_bf16_47mb.safetensors", "llm_nvfp4_17mb.safetensors"]),
]
STRONG = ["zstd,19", "brotli,11", "xz,9", "bsc0", "bzip3,5"]   # the general rivals measured on every file of A2
CONTROL = ["lz4", "kolmo,16", "zstd,3", "kolmo,4"]
SILESIA = ["dickens", "mozilla", "mr", "nci", "ooffice", "osdb", "reymont", "samba", "sao", "webster", "x-ray", "xml"]


def load(path):
    """-> {(round, file, setting): measurement}: the last clean row, or else the last marked row, or else the failure."""
    rows = {}
    if not path or not os.path.exists(path):
        return rows
    for line in open(path):
        r = json.loads(line)
        if r.get("type") != "row":
            continue
        k = (r.get("round", "main"), r["file"], r.get("entry") or r.get("set"))
        r["marked"] = "failed" if "error" in r else "threads" if r.get("threads") else "busy" if r.get("busy") else ""
        if "error" not in r:
            n = r["n"]
            r["ct"] = r["c_s"] if "c_s" in r else n / 1e6 / r["c_mbps"]
            r["dt"] = r["d_s"] if "d_s" in r else n / 1e6 / r["d_mbps"]
        old = rows.get(k)
        rank = {"": 0, "threads": 1, "busy": 1, "failed": 2}
        if old is None or rank[r["marked"]] <= rank[old["marked"]]:
            rows[k] = r
    return rows


def agg(ms):
    ms = list(ms)
    if not ms or any(m is None or m["marked"] == "failed" for m in ms):
        return None
    return {"size": sum(m["size"] for m in ms), "n": sum(m["n"] for m in ms), "ct": sum(m["ct"] for m in ms),
            "dt": sum(m["dt"] for m in ms), "marked": ",".join(sorted({m["marked"] for m in ms if m["marked"]}))}


def show(entry):
    name, _, level = entry.partition(",")
    if name == "kolmo":
        return "kolmo " + FLAGS[int(level)]
    if name == "kolmo_ctl":
        return "kolmo control build " + FLAGS[int(level)]
    if name in ("bsc0", "bsc1"):
        return "bsc -m0 -e%d" % (2 - int(name[3]))
    return name + (" -" + level if level else "")


def mbps(m, which):
    return m["n"] / 1e6 / m[which]


def f_speed(x):
    return ("%.0f" if x >= 100 else "%.1f" if x >= 10 else "%.2f") % x


def f_ratio(x):
    return ("%.0f×" if x >= 100 else "%.1f×" if x >= 10 else "%.2f×" if x >= 0.1 else "%.3f×" if x >= 0.01 else "%.4f×") % x


def f_size(ds):
    return "same size" if abs(ds) < 0.0005 else "%.1f%% %s" % (abs(ds) * 100, "smaller" if ds < 0 else "larger")


def versus(k, r):
    """-> (size difference, compression speed ratio, decompression speed ratio) of k against r"""
    return k["size"] / r["size"] - 1, r["ct"] / k["ct"], r["dt"] / k["dt"]


def verdict(ds, c, d):
    slow = [w for w, x in (("compress", c), ("decompress", d)) if x < 0.9]
    fast = [w for w, x in (("compress", c), ("decompress", d)) if x > 1.1]
    if ds > 0.01:
        return "Behind: larger" + (", though faster to " + " and ".join(fast) if fast else "")
    if ds < -0.01:
        return "Ahead" if not slow else "Trade: smaller, slower to " + " and ".join(slow)
    if slow and fast:
        return "Same size: faster to %s, slower to %s" % (fast[0], slow[0])
    if slow:
        return "Behind: same size, slower to " + " and ".join(slow)
    return "Ahead: same size, faster to " + " and ".join(fast) if fast else "Level"


def table(out, head, lines):
    out.append("| " + " | ".join(head) + " |")
    out.append("|" + "|".join("---" for _ in head) + "|")
    out.extend("| " + " | ".join(str(x) for x in l) + " |" for l in lines)
    out.append("")


def vs_cells(k, r):
    if k is None or r is None:
        return ["not measured", "", "", ""]
    ds, c, d = versus(k, r)
    mark = " (marked: %s)" % (k["marked"] or r["marked"]) if k["marked"] or r["marked"] else ""
    return [f_size(ds), f_ratio(c), f_ratio(d), verdict(ds, c, d) + mark]


def tier_a(out, a1, a2, a3):
    get1 = lambda e: a1.get(("main", "silesia.tar", e))   # noqa: E731
    if a1:
        out.append("## A1. `silesia.tar` (211,948,032 bytes), every setting, in memory, one thread\n")
        base = get1("memcpy")
        lines = []
        for e in A1_ORDER:
            m = get1(e)
            if m is None or m["marked"] == "failed":
                lines.append([show(e), "not measured" if m is None else "failed", "", "", "", "", ""])
                continue
            mem = "%.0f" % ((m["peak_rss"] - base["peak_rss"]) / 1e6) if base and e != "memcpy" else ""
            lines.append([show(e) if e.startswith("kolmo") else m["name"], "{:,}".format(m["size"]), "%.3f" % (m["n"] / m["size"]),
                          f_speed(m["c_mbps"]), f_speed(m["d_mbps"]), mem, m["marked"]])
        table(out, ["Setting", "Compressed bytes", "Ratio", "Compress MB/s", "Decompress MB/s", "Peak memory above the memcpy row, MB", "Mark"], lines)
    files2 = sorted({k[1] for k in a2 if k[0] == "main"})
    get2 = lambda e: agg(a2.get(("main", f, e)) for f in files2)   # noqa: E731
    if a1 or a2:
        out.append("## Areas 1 to 9: Kolmo against the leader of each area\n")
        out.append("The first line of each area is the pair fixed before measuring; the lines under it are shown for context.\n")
        lines = []
        for num, name, pairs in AREAS:
            for i, (ke, re_) in enumerate(pairs):
                for label, get in (("silesia.tar", get1), ("%d files" % len(files2), get2)):
                    k, r = get(ke), get(re_)
                    if k is None or r is None:
                        continue
                    wins = ""
                    if get is get2:
                        d = [a2[("main", f, ke)]["size"] / a2[("main", f, re_)]["size"] - 1 for f in files2]
                        wins = "%d / %d / %d" % (sum(x < -0.01 for x in d), sum(abs(x) <= 0.01 for x in d), sum(x > 0.01 for x in d))
                    lines.append([str(num) if i == 0 else "", name if i == 0 else "", show(ke), show(re_), label] + vs_cells(k, r) + [wins])
        table(out, ["#", "Area", "Kolmo setting", "Rival", "Input", "Size", "Compress speed", "Decompress speed", "Standing",
                    "Files where Kolmo is smaller / within 1% / larger"], lines)
    if a2:
        out.append("## A2 by data type\n")
        out.append("`-m balanced` is set against zstd -3. `-m ratio` and `-x` are set against the general rival that is smallest on "
                   "the group (of zstd -19, brotli -11, xz -9, bsc -m0 -e2 and bzip3 with 16 MiB blocks).\n")
        lines, detail = [], []
        for gname, files in GROUPS:
            g = lambda e: agg(a2.get(("main", f, e)) for f in files)   # noqa: E731
            cands = [(g(e)["size"], e) for e in STRONG if g(e)]
            if not cands or not g("kolmo,4"):
                continue
            best = min(cands)[1]
            n = g("kolmo,4")["n"]
            row = ["%s (%d, %.1f MB)" % (gname, len(files), n / 1e6)]
            for ke, re_ in (("kolmo,4", "zstd,3"), ("kolmo,10", best), ("kolmo,12", best)):
                c = vs_cells(g(ke), g(re_))
                row.append("%s; %s / %s" % (c[0], c[1], c[2]) if c[1] else c[0])
            lines.append(row[:2] + [show(best)] + row[2:])
            for e in ["lz4", "kolmo,16", "zstd,1", "kolmo,17", "zstd,3", "kolmo,4", "kolmo,14", "libdeflate,6", "brotli,5", "kolmo,8",
                      "zstd,9", "kolmo,5", "zstd,19", "kolmo,9", "brotli,11", "xz,9", "bsc0", "bzip3,5", "kolmo,10", "kolmo,12"]:
                m = g(e)
                if m:
                    detail.append([gname, show(e), "{:,}".format(m["size"]), "%.3f" % (m["n"] / m["size"]), f_speed(mbps(m, "ct")),
                                   f_speed(mbps(m, "dt")), m["marked"]])
        table(out, ["Data type (files, size)", "`-m balanced` against zstd -3: size; compress / decompress speed", "Smallest general rival",
                    "`-m ratio` against it", "`-x` against it"], lines)
        out.append("### A2, every setting by data type\n")
        table(out, ["Data type", "Setting", "Compressed bytes", "Ratio", "Compress MB/s", "Decompress MB/s", "Mark"], detail)
        cols = ["zstd,3", "kolmo,4", "zstd,19", "kolmo,9", "brotli,11", "xz,9", "bsc0", "bzip3,5", "kolmo,10", "kolmo,12"]
        if all(agg(a2.get(("main", f, e)) for f in SILESIA) for e in cols):
            out.append("### A2, the 12 Silesia files compressed one by one (the form the public Silesia table uses)\n")
            lines = []
            for e in cols:
                m = agg(a2.get(("main", f, e)) for f in SILESIA)
                lines.append([show(e), "{:,}".format(m["size"]), "%.3f" % (m["n"] / m["size"]), f_speed(mbps(m, "ct")), f_speed(mbps(m, "dt")), m["marked"]])
            table(out, ["Setting", "Sum of the 12 compressed sizes", "Ratio", "Compress MB/s", "Decompress MB/s", "Mark"], lines)
        out.append("### A2, file by file: compressed bytes\n")
        lines = []
        for f in sorted(files2, key=lambda f: a2[("main", f, "memcpy")]["n"] if ("main", f, "memcpy") in a2 else 0):
            ms = [a2.get(("main", f, e)) for e in cols]
            n = next((m["n"] for m in ms if m and m["marked"] != "failed"), 0)
            best = min((m["size"] for m in ms if m and m["marked"] != "failed"), default=None)
            lines.append([f, "{:,}".format(n)] + ["failed" if not m or m["marked"] == "failed" else
                                                  ("**{:,}**" if m["size"] == best else "{:,}").format(m["size"]) for m in ms])
        table(out, ["File", "Bytes"] + [show(e) for e in cols], lines)
        small = [f for f in files2 if ("main", f, "lz4") in a2 and a2[("main", f, "lz4")].get("n", 1 << 40) < 1000000]
        if small:
            out.append("### A2, inputs under 1 MB: compress / decompress MB/s of the fast settings\n")
            pairs = ["lz4", "kolmo,16", "zstd,1", "kolmo,17", "zstd,3", "kolmo,4"]
            lines = []
            for f in sorted(small, key=lambda f: a2[("main", f, "lz4")]["n"]):
                ms = [a2.get(("main", f, e)) for e in pairs]
                lines.append([f, "{:,}".format(a2[("main", f, "lz4")]["n"])] +
                             ["failed" if not m or m["marked"] == "failed" else "%s / %s" % (f_speed(m["c_mbps"]), f_speed(m["d_mbps"])) for m in ms])
            table(out, ["File", "Bytes"] + [show(e) for e in pairs], lines)
    if a3:
        out.append("## A3. Typed arrays (lzbench, in memory)\n")
        files3 = sorted({k[1] for k in a3 if k[0] == "main"}, key=lambda f: (os.path.splitext(f)[1], f))
        cols = ["zstd,3", "kolmo,4", "kolmo,14", "kolmo,15", "zstd,19", "xz,9", "bsc0", "bzip3,5", "kolmo,10", "kolmo,12", "kolmo,13"]
        lines = []
        for f in files3:
            oz = [m for k, m in a3.items() if k[0] == "main" and k[1] == f and k[2].startswith("openzl") and m["marked"] != "failed"]
            ms = [a3.get(("main", f, e)) for e in cols]
            n = next(m["n"] for m in ms if m and m["marked"] != "failed")
            lines.append([f, "{:,}".format(n)] + ["{:,}".format(m["size"]) if m and m["marked"] != "failed" else "failed" for m in ms] +
                         ["{:,}".format(oz[0]["size"]) if oz else ""])
        table(out, ["File", "Bytes"] + [show(e) for e in cols] + ["OpenZL 0.2.3 numeric profile"], lines)
        lines = []
        for e in cols + ["kolmo_ctl,4", "kolmo_ctl,15"]:
            m = agg(a3.get(("main", f, e)) for f in files3)
            if m:
                lines.append([show(e), "{:,}".format(m["size"]), "%.3f" % (m["n"] / m["size"]), f_speed(mbps(m, "ct")), f_speed(mbps(m, "dt")), m["marked"]])
        raw = [f for f in files3 if any(k[1] == f and k[2].startswith("openzl") for k in a3)]
        table(out, ["Setting, all %d arrays" % len(files3), "Compressed bytes", "Ratio", "Compress MB/s", "Decompress MB/s", "Mark"], lines)
        if raw:
            lines = []
            ozm = agg(next(m for k, m in a3.items() if k[0] == "main" and k[1] == f and k[2].startswith("openzl")) for f in raw)
            for e in ["kolmo,4", "kolmo,14", "kolmo,10", "kolmo,12", "zstd,19", "bsc0"]:
                lines.append([show(e)] + vs_cells(agg(a3.get(("main", f, e)) for f in raw), ozm))
            out.append("Against OpenZL's numeric profile (lzbench's OpenZL 0.2.3) on the %d raw integer arrays:\n" % len(raw))
            table(out, ["Setting", "Size", "Compress speed", "Decompress speed", "Standing"], lines)


def tier_s1(out, s):
    if not s:
        return
    out.append("## S1. Specialists called in one Python process beside Kolmo's binding (one thread)\n")
    by = collections.defaultdict(dict)
    group_of = {}
    for (_, f, st), m in s.items():
        by[f][st] = m
        group_of[f] = m["group"]
    kinds = [("Arrays of numbers", lambda g: g.startswith("numbers"), ["pcodec level 8", "pcodec level 12"]),
             ("Raw 8-bit images", lambda g: g.startswith("raw 8-bit"), ["JPEG XL lossless, effort 7", "JPEG XL lossless, effort 9"]),
             ("Audio, 16-bit PCM", lambda g: g.startswith("audio"), ["FLAC level 5", "FLAC level 8", "FLAC level 5, through a file",
                                                                    "FLAC level 8, through a file", "pcodec level 8", "pcodec level 12"])]
    ksets = ["kolmo -m balanced", "kolmo -m balanced --native-leaf", "kolmo -m balanced --typed-huf", "kolmo -m hc", "kolmo -m ratio",
             "kolmo -x", "kolmo -x -l 2"]
    for title, pred, rivals in kinds:
        files = sorted(f for f in by if pred(group_of[f]))
        if not files:
            continue
        ref = rivals[1] if title != "Arrays of numbers" else rivals[0]
        refm = agg(by[f].get(ref) for f in files)
        out.append("### %s: %d file%s, %s bytes\n" % (title, len(files), "" if len(files) == 1 else "s", "{:,}".format(sum(by[f][ksets[0]]["n"] for f in files))))
        lines = []
        for st in rivals + ksets:
            m = agg(by[f].get(st) for f in files)
            if m is None:
                lines.append([st, "not on every file", "", "", "", "", "", ""])
                continue
            wins = sum(1 for f in files if by[f][st]["size"] < by[f][ref]["size"]) if refm and st != ref else ""
            cells = vs_cells(m, refm) if refm and st != ref else ["", "", "", ""]
            lines.append([st, "{:,}".format(m["size"]), f_speed(mbps(m, "ct")), f_speed(mbps(m, "dt")), cells[0], "%s / %s" % (cells[1], cells[2]) if cells[1] else "",
                          "%s of %d" % (wins, len(files)) if wins != "" else "", m["marked"]])
        table(out, ["Setting", "Bytes", "Compress MB/s", "Decompress MB/s", "Size against " + ref, "Speed against it (compress / decompress)",
                    "Files where it is smaller", "Mark"], lines)
        lib = [(st, sum(by[f][st]["lib_c_s"] for f in files), sum(by[f][st]["lib_d_s"] for f in files), agg(by[f].get(st) for f in files))
               for st in ksets if all(st in by[f] and "lib_c_s" in by[f][st] for f in files)]
        if lib:
            out.append("Kolmo's bare library call (buffers allocated beforehand) against its Python binding, speed ratio compress / decompress: " +
                       "; ".join("%s %.2f× / %.2f×" % (st[6:], m["ct"] / c, m["dt"] / d) for st, c, d, m in lib) + ".\n")
        if title != "Audio, 16-bit PCM":
            lines = []
            for f in files:
                for st in rivals + ksets:
                    m = by[f].get(st)
                    if m and m["marked"] != "failed":
                        lines.append([f, st, "{:,}".format(m["size"]), "%.3f" % (m["n"] / m["size"]), f_speed(mbps(m, "ct")), f_speed(mbps(m, "dt")), m["marked"]])
                    else:
                        lines.append([f, st, "failed" if m else "not run", "", "", "", ""])
            table(out, ["File", "Setting", "Bytes", "Ratio", "Compress MB/s", "Decompress MB/s", "Mark"], lines)


def tier_s2(out, s):
    if not s:
        return
    out.append("## S2. Command-line tools, whole process, one thread (OpenZL `zli`, 7-Zip, Kolmo, zstd)\n")
    by = collections.defaultdict(dict)
    group_of, order = {}, []
    for (_, f, st), m in s.items():
        by[f][st] = m
        group_of[f] = m["group"]
        if m["group"] not in order:
            order.append(m["group"])
    for f in by:   # the best OpenZL profile of each file, per level
        for level in ("6", "9"):
            c = [m for st, m in by[f].items() if st.startswith("zli -p") and st.endswith("-l " + level) and m["marked"] != "failed"]
            if c:
                by[f]["zli, its best profile for the file, -l " + level] = min(c, key=lambda m: m["size"])
    summary, detail = [], []
    for gname in order:
        files = sorted(f for f in by if group_of[f] == gname)
        sets = []
        for f in files:
            sets += [st for st in by[f] if st not in sets]
        sets = [st for st in sets if not st.startswith("zli -p") or len(files) == 1]
        g = lambda st: agg(by[f].get(st) for f in files)   # noqa: E731
        rivals = [st for st in sets if st.startswith(("zli", "7zz")) and g(st)]
        for st in sets:
            m = g(st)
            if m:
                detail.append([gname, st, "{:,}".format(m["size"]), "%.3f" % (m["n"] / m["size"]), f_speed(mbps(m, "ct")), f_speed(mbps(m, "dt")),
                               "%.0f / %.0f" % (max(by[f][st]["c_rss"] for f in files) / 1e6, max(by[f][st]["d_rss"] for f in files) / 1e6), m["marked"]])
        for rv in rivals:
            for ks in ("kolmo -m balanced", "kolmo -m hc", "kolmo -m ratio", "kolmo -x"):
                if g(ks):
                    summary.append(["%s (%d, %.1f MB)" % (gname, len(files), g(ks)["n"] / 1e6), rv, ks] + vs_cells(g(ks), g(rv)))
    table(out, ["Data type (files, size)", "Rival", "Kolmo setting", "Size", "Compress speed", "Decompress speed", "Standing"], summary)
    out.append("### S2, every setting by data type\n")
    table(out, ["Data type", "Setting", "Bytes", "Ratio", "Compress MB/s", "Decompress MB/s", "Peak memory MB (compress / decompress)", "Mark"], detail)
    lines = []
    for f in sorted(by):
        for st, m in by[f].items():
            if st.startswith("zli -p") and m["marked"] != "failed":
                lines.append([f, st, "{:,}".format(m["size"]), f_speed(mbps(m, "ct")), f_speed(mbps(m, "dt")), m["marked"]])
    table(out, ["File", "OpenZL profile", "Bytes", "Compress MB/s", "Decompress MB/s", "Mark"], lines)


def further(out, a1, a2, a3, s1):
    """One comparison per line with its standing, and the pairs that were chosen after the results were seen."""
    out.append("## Further comparisons\n")
    out.append("Made for the report from the same rows. The data-type tables repeat the pairs above with one comparison per "
               "line and its standing. The table \"Pairs not fixed before measuring\" holds pairs chosen after the results "
               "were seen.\n")
    head = ["Kolmo setting", "Rival", "Size", "Compress speed", "Decompress speed", "Standing"]
    spread = "Files where Kolmo is smaller / within 1% / larger"

    def split(sizes):   # sizes: (kolmo bytes, rival bytes) per file
        d = [k / r - 1 for k, r in sizes]
        return "%d / %d / %d" % (sum(x < -0.01 for x in d), sum(abs(x) <= 0.01 for x in d), sum(x > 0.01 for x in d))

    def pair(rows, files, ke, re_):
        k, r = (agg(rows.get(("main", f, e)) for f in files) for e in (ke, re_))
        if not k or not r:
            return None
        return k, [show(ke), show(re_)] + vs_cells(k, r) + [split([(rows[("main", f, ke)]["size"], rows[("main", f, re_)]["size"]) for f in files])
                                                           if len(files) > 1 else ""]

    files2 = sorted({k[1] for k in a2 if k[0] == "main"})
    if a2:
        lines = []
        for gname, files in GROUPS:
            sizes = [(m["size"], e) for e in STRONG for m in [agg(a2.get(("main", f, e)) for f in files)] if m]
            if not sizes:
                continue
            best = min(sizes)[1]
            for ke, re_ in (("kolmo,4", "zstd,3"), ("kolmo,10", best), ("kolmo,12", best)):
                p = pair(a2, files, ke, re_)
                if p:
                    lines.append(["%s (%d, %.1f MB)" % (gname, len(files), p[0]["n"] / 1e6)] + p[1])
        out.append("### A2 by data type, one comparison per line\n")
        table(out, ["Data type (files, size)"] + head + [spread], lines)
        lines = []
        for e in ["lz4", "kolmo,16", "zstd,1", "kolmo,17", "zstd,3", "kolmo,4", "kolmo,14", "libdeflate,6", "brotli,5", "kolmo,8",
                  "zstd,9", "kolmo,5", "zstd,19", "kolmo,9", "brotli,11", "xz,9", "bsc0", "bzip3,5", "kolmo,10", "kolmo,12"]:
            m = agg(a2.get(("main", f, e)) for f in files2)
            if m:
                lines.append([show(e), "{:,}".format(m["size"]), "%.3f" % (m["n"] / m["size"]), f_speed(mbps(m, "ct")), f_speed(mbps(m, "dt")),
                              "{:,}".format(m["n"]), m["marked"]])
        out.append("### A2, all %d files together: the settings measured on every file\n" % len(files2))
        table(out, ["Setting", "Sum of the compressed sizes", "Ratio", "Compress MB/s", "Decompress MB/s", "Input bytes", "Mark"], lines)
    lines = []
    for ke, re_ in (("kolmo,3", "zstd,9"), ("kolmo,4", "zstd,9"), ("kolmo,5", "zstd,19"), ("kolmo,7", "bzip3,5"), ("kolmo,7", "kanzi,7"),
                    ("kolmo,7", "xz,9"), ("kolmo,10", "zpaq,5")):
        for label, rows, files in (("silesia.tar", a1, ["silesia.tar"]), ("%d files" % len(files2), a2, files2)):
            p = pair(rows, files, ke, re_) if rows and files else None
            if p:
                lines.append(p[1][:2] + [label] + p[1][2:])
    if lines:
        out.append("### Pairs not fixed before measuring\n")
        table(out, head[:2] + ["Input"] + head[2:] + [spread], lines)
    if a3:
        files3 = sorted({k[1] for k in a3 if k[0] == "main"})
        sizes = [(m["size"], e) for e in ("zstd,19", "xz,9", "bsc0", "bzip3,5") for m in [agg(a3.get(("main", f, e)) for f in files3)] if m]
        lines = []
        for ke, re_ in (("kolmo,4", "zstd,3"), ("kolmo,14", "zstd,3"), ("kolmo,15", "zstd,3")) + \
                tuple((ke, min(sizes)[1]) for ke in ("kolmo,10", "kolmo,12", "kolmo,13") if sizes):
            p = pair(a3, files3, ke, re_)
            if p:
                lines.append(p[1])
        out.append("### A3, all %d typed arrays, one comparison per line\n" % len(files3))
        table(out, head + [spread], lines)
    if s1:
        by = collections.defaultdict(dict)
        group_of = {}
        for (_, f, st), m in s1.items():
            by[f][st] = m
            group_of[f] = m["group"]
        kb, kn, kt, kh, kr, kx, kx2 = ("kolmo -m balanced", "kolmo -m balanced --native-leaf", "kolmo -m balanced --typed-huf", "kolmo -m hc",
                                       "kolmo -m ratio", "kolmo -x", "kolmo -x -l 2")
        subsets = [("Arrays of numbers", lambda g: g.startswith("numbers"), ["pcodec level 8", "pcodec level 12"], [kb, kn, kh, kr, kx, kx2]),
                   ("Arrays of numbers: 64-bit floats in NumPy files", lambda g: g == "numbers (NumPy float64)", ["pcodec level 8", "pcodec level 12"], [kb, kh, kr, kx]),
                   ("Arrays of numbers: 16-bit images", lambda g: g == "numbers (<u2)", ["pcodec level 8", "pcodec level 12"], [kb, kh, kr, kx]),
                   ("Arrays of numbers: raw signed integers", lambda g: g in ("numbers (<i2)", "numbers (<i4)", "numbers (<i8)"), ["pcodec level 8", "pcodec level 12"],
                    [kb, kn, kh, kr, kx, kx2]),
                   ("Raw 8-bit images", lambda g: g.startswith("raw 8-bit"), ["JPEG XL lossless, effort 7", "JPEG XL lossless, effort 9"], [kb, kh, kr, kx]),
                   ("Audio, 16-bit PCM", lambda g: g.startswith("audio"), ["FLAC level 5", "FLAC level 8"], [kb, kt, kh, kr, kx, kx2])]
        lines = []
        for title, pred, rivals, ks in subsets:
            files = sorted(f for f in by if pred(group_of[f]))
            for rv in rivals:
                r = agg(by[f].get(rv) for f in files)
                for st in ks:
                    k = agg(by[f].get(st) for f in files)
                    if k and r:
                        lines.append(["%s (%d, %.1f MB)" % (title, len(files), k["n"] / 1e6), st, rv] + vs_cells(k, r) +
                                     [split([(by[f][st]["size"], by[f][rv]["size"]) for f in files])])
        out.append("### S1, one comparison per line\n")
        table(out, ["Data (files, size)"] + head + [spread], lines)
    small = [f for f in files2 if ("main", f, "lz4") in a2 and a2[("main", f, "lz4")].get("n", 1 << 40) < 1000000]
    if small:
        cols = ["lz4", "kolmo,16", "zstd,1", "kolmo,17", "zstd,3", "kolmo,4"]
        lines = []
        for f in sorted(small, key=lambda f: a2[("main", f, "lz4")]["n"]):
            ms = [a2.get(("main", f, e)) for e in cols]
            lines.append([f, "{:,}".format(a2[("main", f, "lz4")]["n"])] +
                         ["failed" if not m or m["marked"] == "failed" else "%.3f / %.3f" % (m["ct"] * 1e3, m["dt"] * 1e3) for m in ms])
        out.append("### A2, inputs under 1 MB: time of one call in milliseconds, compress / decompress\n")
        table(out, ["File", "Bytes"] + [show(e) for e in cols], lines)


def checks(out, tiers, extra):
    out.append("## Checks\n")
    lines = []
    for tname, rows in tiers:
        if not rows:
            continue
        for (rnd, f, e), end in sorted(rows.items()):
            if rnd != "end" or end["marked"] == "failed":
                continue
            first = rows.get(("main", f, e))
            if first and first["marked"] != "failed":
                lines.append([tname, f, show(e), f_speed(first["c_mbps"]), f_speed(end["c_mbps"]), "%+.1f%%" % ((end["c_mbps"] / first["c_mbps"] - 1) * 100),
                              f_speed(first["d_mbps"]), f_speed(end["d_mbps"]), "%+.1f%%" % ((end["d_mbps"] / first["d_mbps"] - 1) * 100)])
    if lines:
        out.append("Drift: the control settings at the start and again at the end of each tier.\n")
        table(out, ["Tier", "File", "Setting", "Compress MB/s, first", "end", "change", "Decompress MB/s, first", "end", "change"], lines)
    lines = []
    for tname, rows in list(tiers) + list(extra):
        if not rows:
            continue
        marks = collections.Counter(m["marked"] for m in rows.values())
        seen = collections.Counter(o[0] for m in rows.values() for o in m.get("others", []))
        with_others = sum(1 for m in rows.values() if m.get("others"))
        lines.append([tname, len(rows), marks[""], marks["threads"], marks["busy"], marks["failed"], with_others,
                      ", ".join("%s (%d)" % x for x in seen.most_common(4))])
    table(out, ["Tier", "Rows", "Clean", "Marked `threads`", "Marked `busy`", "Failed", "Rows with another busy process", "Which (rows)"], lines)
    for tname, rows in list(tiers) + list(extra):
        for k, m in sorted(rows.items()):
            if m["marked"]:
                out.append("- %s, %s, %s: %s%s" % (tname, k[1], show(k[2]) if tname.startswith("A") else k[2], m["marked"],
                                                   " (" + m["error"][:160].replace("\n", " ") + ")" if "error" in m else ""))
    out.append("")
    lines = []
    for tname, rows in tiers:
        for level in ("4", "15"):
            files = sorted({k[1] for k in rows if k[0] == "main" and k[2] == "kolmo_ctl," + level})
            rel, ctl = agg(rows.get(("main", f, "kolmo," + level)) for f in files), agg(rows.get(("main", f, "kolmo_ctl," + level)) for f in files)
            if rel and ctl:
                lines.append([tname, len(files), FLAGS[int(level)], "yes" if all(rows[("main", f, "kolmo," + level)]["size"] ==
                              rows[("main", f, "kolmo_ctl," + level)]["size"] for f in files) else "NO", f_ratio(ctl["ct"] / rel["ct"]), f_ratio(ctl["dt"] / rel["dt"])])
    if lines:
        out.append("Release build against the control build (generic integer inverse in the decoder), same frames:\n")
        table(out, ["Tier", "Files", "Setting", "Same sizes", "Compress speed, release / control", "Decompress speed, release / control"], lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", required=True)
    ap.add_argument("--inprocess")
    ap.add_argument("--cli")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    a1, a2, a3 = (load(os.path.join(a.dir, t + ".jsonl")) for t in ("A1", "A2", "A3"))
    s1, s2 = load(a.inprocess), load(a.cli)
    out = ["# Kolmo 1.2.0-rc1: tables made by benchmark/rc1/report.py from the result files", ""]
    tier_a(out, a1, a2, a3)
    tier_s1(out, s1)
    tier_s2(out, s2)
    further(out, a1, a2, a3, s1)
    checks(out, [("A1", a1), ("A2", a2), ("A3", a3)], [("S1", s1), ("S2", s2)])
    with open(a.out, "w") as f:
        f.write("\n".join(out) + "\n")
    print("wrote", a.out, len(out), "lines")


if __name__ == "__main__":
    main()
