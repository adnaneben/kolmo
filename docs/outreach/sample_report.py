#!/usr/bin/env python3
"""Design-partner sample report: Kolmo against zstd and xz on a directory of someone's sample files.

    python3 docs/outreach/sample_report.py SAMPLE_DIR --volume 40TB --price 0.023 --out report.md

For every regular file under SAMPLE_DIR (recursively) and every tool setting below, the script:

  1. compresses the file with the tool's command line on ONE thread, file to file, timing the process;
  2. decompresses the result, timing it, and compares the output with the original byte for byte
     (a mismatch or a tool error stops the run: nothing unverified is reported);
  3. keeps the best (fastest) of --repeat timed runs; sizes do not vary between runs.

Settings: kolmo -3 / -6 / -9 (-T 1), zstd -3 / -19 (-T1), xz -9 (-T1); --gzip adds gzip -6, --bzip2 adds
bzip2 -9, and --current names the setting the data is stored with today (gzip, bzip2, zstd-3, ... or none =
uncompressed), which the savings are computed against.

Ratio = original bytes / compressed bytes. Speeds are MiB/s of original data over wall time, process start-up and
file I/O included (CLI-process timing, as in docs/outreach/CLAIMS.md), so small files understate every tool's speed.

Projection: --volume is the stated total volume of this kind of data, UNCOMPRESSED (or, with --volume-is current, as
stored today with the --current setting). The yearly cost of each setting is volume / total sample ratio x $/GB-month
x 12. It assumes the sample is representative of the whole volume; say so in the report you send.

--from-json re-renders a report from an earlier --json file without measuring again.

Outputs a Markdown report (sections match docs/outreach/sample-report-template.md) and, with --json, the raw numbers.
Needs Python 3.8+ and the kolmo, zstd and xz command lines (gzip with --gzip). Standard library only.
"""
import argparse
import datetime
import filecmp
import fnmatch
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time

MiB = 1024 * 1024
UNITS = {"B": 1, "KB": 1e3, "MB": 1e6, "GB": 1e9, "TB": 1e12, "PB": 1e15,
         "KIB": 2**10, "MIB": 2**20, "GIB": 2**30, "TIB": 2**40, "PIB": 2**50}


def settings(kolmo, levels, gzip, bzip2=False):
    """(name, compress argv, decompress argv) for each setting; argv read stdin and write stdout."""
    s = [(f"kolmo -{lv}", [kolmo, f"-{lv}", "-T", "1", "-c", "-q"], [kolmo, "-d", "-T", "1", "-c", "-q"])
         for lv in levels]
    s += [("zstd -3", ["zstd", "-3", "-T1", "-c", "-q"], ["zstd", "-d", "-T1", "-c", "-q"]),
          ("zstd -19", ["zstd", "-19", "-T1", "-c", "-q"], ["zstd", "-d", "-T1", "-c", "-q"]),
          ("xz -9", ["xz", "-9", "-T1", "-c", "-q"], ["xz", "-d", "-T1", "-c", "-q"])]
    if gzip:
        s.append(("gzip -6", ["gzip", "-6", "-c", "-n"], ["gzip", "-d", "-c"]))
    if bzip2:
        s.append(("bzip2 -9", ["bzip2", "-9", "-c"], ["bzip2", "-d", "-c"]))
    return s


def parse_size(text):
    m = re.fullmatch(r"\s*([0-9]*\.?[0-9]+)\s*([A-Za-z]*)\s*", text)
    if not m or m.group(2).upper() not in UNITS:
        raise argparse.ArgumentTypeError(f"size like 40TB, 500GB or 2.5PiB, got {text!r}")
    return float(m.group(1)) * UNITS[m.group(2).upper()]


def human(n):
    for unit, f in (("PB", 1e15), ("TB", 1e12), ("GB", 1e9), ("MB", 1e6), ("KB", 1e3)):
        if n >= f:
            return f"{n / f:.3g} {unit}"
    return f"{n:.0f} B"


def tool_version(argv0):
    for flag in ("--version", "-V"):
        try:
            out = subprocess.run([argv0, flag], stdin=subprocess.DEVNULL, capture_output=True, timeout=10,
                                 text=True, errors="replace")
        except (OSError, subprocess.TimeoutExpired):
            continue
        # bzip2 prints its version on stderr (and compresses empty input to stdout): take the first readable line
        lines = [ln.strip("* ") for ln in (out.stdout + "\n" + out.stderr).splitlines()]
        text = [ln for ln in lines if ln.isprintable() and re.search(r"[0-9]+\.[0-9]+", ln)]
        if out.returncode == 0 and text:
            return text[0]
    return "unknown"


def timed(argv, src, dst):
    with open(src, "rb") as fi, open(dst, "wb") as fo:
        t = time.perf_counter()
        p = subprocess.run(argv, stdin=fi, stdout=fo, stderr=subprocess.PIPE)
        dt = time.perf_counter() - t
    if p.returncode != 0:
        raise RuntimeError(f"{' '.join(argv)} failed on {src}: {p.stderr.decode(errors='replace').strip()}")
    return dt


def measure(path, setting, work, repeat):
    name, comp, decomp = setting
    packed, unpacked = os.path.join(work, "packed"), os.path.join(work, "unpacked")
    best_c = best_d = float("inf")
    for _ in range(repeat):
        best_c = min(best_c, timed(comp, path, packed))
        best_d = min(best_d, timed(decomp, packed, unpacked))
        if not filecmp.cmp(path, unpacked, shallow=False):
            raise RuntimeError(f"ROUND TRIP MISMATCH: {name} on {path}")
    size = os.path.getsize(packed)
    os.remove(packed)
    os.remove(unpacked)
    return {"size": size, "enc_s": best_c, "dec_s": best_d, "verified": True}


def collect(root):
    files = []
    for d, dirs, names in os.walk(root):
        dirs.sort()
        for n in sorted(names):
            p = os.path.join(d, n)
            if os.path.isfile(p) and not os.path.islink(p) and os.path.getsize(p) > 0:
                files.append(p)
    return files


def money(x):
    return f"-${-x:,.0f}" if x < 0 else f"${x:,.0f}"


def ratio(orig, comp):
    return orig / comp if comp else float("inf")


def mibs(orig, secs):
    return orig / MiB / secs if secs > 0 else float("inf")


def report_md(r, a):
    names = r["settings"]
    tot = r["total"]
    L = []
    L.append(f"# Kolmo sample report: {a.title}\n")
    L.append(f"Generated {r['date']} by `docs/outreach/sample_report.py`. "
             f"Sample: {len(r['files'])} files, {human(tot['bytes'])} ({tot['bytes']:,} bytes).\n")
    L.append("## Summary\n")
    best = r["best_kolmo"]
    cur = a.current
    ref = "zstd -3" if cur == "none" else cur
    rb, rr = tot["by"][best]["ratio"], tot["by"][ref]["ratio"]
    L.append(f"- Best Kolmo setting on this sample: **{best}**, ratio **{rb:.2f}** "
             f"against {rr:.2f} for {ref}: Kolmo's output is **{rb / rr:.2f}x smaller** "
             f"({100 * (1 - rr / rb):.0f}% less storage).")
    rt = tot["by"]["kolmo -3"] if "kolmo -3" in tot["by"] else None
    if rt:
        L.append(f"- Kolmo's default level (-3): ratio {rt['ratio']:.2f}, compress {rt['enc_mibs']:.0f} MiB/s, "
                 f"decompress {rt['dec_mibs']:.0f} MiB/s on one thread.")
    L.append(f"- Every output of every setting was decompressed and compared byte for byte with its original: "
             f"{r['verified']} round trips, 0 mismatches.")
    if r.get("projection"):
        p = r["projection"]
        L.append(f"- Projected for {human(p['volume_bytes'])} of this data at ${a.price}/GB-month: "
                 f"**{money(p['by'][best]['yearly'])}/year** with {best} against "
                 f"{money(p['by'][p['baseline']]['yearly'])}/year {p['baseline_label']}, "
                 f"a saving of **{money(p['by'][best]['saving'])}/year**.")
    L.append("")
    L.append("## Totals (whole sample)\n")
    L.append("| Setting | Compressed | Ratio | Compress MiB/s | Decompress MiB/s | Round trip |")
    L.append("|---|---:|---:|---:|---:|---|")
    for n in names:
        t = tot["by"][n]
        L.append(f"| {n} | {t['size']:,} | {t['ratio']:.2f} | {t['enc_mibs']:.1f} | {t['dec_mibs']:.1f} | verified |")
    L.append("")
    if r.get("projection"):
        p = r["projection"]
        L.append("## Projected storage cost\n")
        L.append(f"Stated volume: {human(p['volume_bytes'])} uncompressed"
                 + (f" (derived from {human(a.volume)} stored as {cur})" if a.volume_is == "current" else "")
                 + f". Price: ${a.price}/GB-month (GB = 10^9 bytes), 12 months. Baseline: {p['baseline_label']}.\n")
        L.append("| Setting | Stored volume | Yearly cost | Saving vs baseline | Saving |")
        L.append("|---|---:|---:|---:|---:|")
        for n in ["uncompressed"] + names:
            q = p["by"][n]
            L.append(f"| {n} | {human(q['stored_bytes'])} | {money(q['yearly'])} | {money(q['saving'])} | "
                     f"{q['saving_pct']:.0f}% |")
        L.append("\nThe projection applies the sample's total ratio to the whole volume. It holds only if the sample "
                 "is representative; storage requests, transfer and CPU cost are not included.\n")
    L.append("## Per file\n")
    L.append("Ratio per setting (the best per file in bold); speeds per file are in the JSON output.\n")
    L.append("| File | Size | " + " | ".join(names) + " |")
    L.append("|---|---:|" + "---:|" * len(names))
    for f in r["files"]:
        rs = {n: f["by"][n]["ratio"] for n in names}
        top = max(rs.values())
        cells = [f"**{rs[n]:.2f}**" if rs[n] == top else f"{rs[n]:.2f}" for n in names]
        L.append(f"| {f['name']} | {human(f['bytes'])} | " + " | ".join(cells) + " |")
    L.append("")
    L.append("## Method\n")
    L.append(f"- Machine: {r['machine']}.")
    L.append("- Tools: " + "; ".join(f"{k} ({v})" for k, v in r["versions"].items()) + ".")
    L.append(f"- Each tool ran as a command-line process on one thread (kolmo `-T 1`, zstd and xz `-T1`), file to file "
             f"through stdin/stdout; best of {a.repeat} timed run(s). Speeds are original MiB over wall time, start-up "
             f"and file I/O included.")
    L.append("- Every compressed output was decompressed by the same tool and compared byte for byte with the "
             "original; the run stops at the first mismatch or tool error.")
    L.append("- Reproduce: `python3 docs/outreach/sample_report.py SAMPLE_DIR"
             + (f" --volume {a.volume_text} --price {a.price}" if a.volume else "")
             + (f" --current {cur.replace(' ', '')}" if cur != "none" else "")
             + (" --volume-is current" if a.volume_is == "current" else "")
             + (" --gzip" if a.gzip else "") + (" --bzip2" if a.bzip2 else "")
             + "` in a checkout of github.com/adnaneben/kolmo.")
    return "\n".join(L) + "\n"


def summarize(res, a):
    """Totals, best Kolmo setting and the cost projection, from the per-file measurements."""
    names = res["settings"]
    total = sum(f["bytes"] for f in res["files"])
    by = {}
    for nm in names:
        size = sum(f["by"][nm]["size"] for f in res["files"])
        enc = sum(f["by"][nm]["enc_s"] for f in res["files"])
        dec = sum(f["by"][nm]["dec_s"] for f in res["files"])
        by[nm] = {"size": size, "ratio": ratio(total, size), "enc_mibs": mibs(total, enc), "dec_mibs": mibs(total, dec)}
    res["total"] = {"bytes": total, "by": by}
    res["verified"] = len(res["files"]) * len(names)
    res["best_kolmo"] = max((nm for nm in names if nm.startswith("kolmo")), key=lambda nm: by[nm]["ratio"])
    res.pop("projection", None)
    if a.volume:
        cur_ratio = 1.0 if a.current == "none" else by[a.current]["ratio"]
        vol = a.volume * cur_ratio if a.volume_is == "current" else a.volume
        months, gb = 12, 1e9
        rows = {"uncompressed": vol}
        rows.update({nm: vol / by[nm]["ratio"] for nm in names})
        base = "uncompressed" if a.current == "none" else a.current
        base_cost = rows[base] / gb * a.price * months
        res["projection"] = {
            "volume_bytes": vol, "price_gb_month": a.price, "months": months, "baseline": base,
            "baseline_label": "uncompressed" if base == "uncompressed" else f"as stored today ({base})",
            "by": {nm: {"stored_bytes": b, "yearly": b / gb * a.price * months,
                        "saving": base_cost - b / gb * a.price * months,
                        "saving_pct": 100 * (1 - b / rows[base])} for nm, b in rows.items()}}
    return res


def excluded(name, patterns):
    base = os.path.basename(name)
    return base.startswith(".") or any(fnmatch.fnmatch(base, p) or fnmatch.fnmatch(name, p) for p in patterns)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0], epilog=__doc__.split("\n\n", 1)[1],
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sample_dir", nargs="?", help="directory of sample files (not needed with --from-json)")
    ap.add_argument("--volume", help="stated total volume, e.g. 40TB or 2.5PiB (see --volume-is)")
    ap.add_argument("--volume-is", choices=["uncompressed", "current"], default="uncompressed",
                    help="whether --volume is uncompressed or as stored today with --current (default uncompressed)")
    ap.add_argument("--price", type=float, default=0.023, help="$ per GB-month (default 0.023, S3 Standard)")
    ap.add_argument("--current", default="none",
                    help="setting the data is stored with today: gzip (= gzip-6), bzip2 (= bzip2-9), zstd-3, "
                         "zstd-19, xz-9, kolmo-N, or none = uncompressed (the default)")
    ap.add_argument("--gzip", action="store_true", help="also measure gzip -6")
    ap.add_argument("--bzip2", action="store_true", help="also measure bzip2 -9")
    ap.add_argument("--levels", default="3,6,9", help="Kolmo levels (default 3,6,9)")
    ap.add_argument("--kolmo", default=os.environ.get("KOLMO", "kolmo"), help="kolmo binary (default: kolmo on PATH)")
    ap.add_argument("--repeat", type=int, default=1, help="timed runs per file and setting, best kept (default 1)")
    ap.add_argument("--exclude", action="append", default=[], metavar="GLOB",
                    help="skip files matching GLOB (name or relative path; repeatable); hidden files are always "
                         "skipped")
    ap.add_argument("--from-json", metavar="FILE",
                    help="re-render a report from an earlier --json output instead of measuring (e.g. with another "
                         "--volume, --price, --current or --exclude)")
    ap.add_argument("--title", default=None, help="report title (default: the sample directory's name)")
    ap.add_argument("--out", help="write the Markdown report here (default: standard output)")
    ap.add_argument("--json", help="also write the raw numbers here")
    a = ap.parse_args()
    if not a.sample_dir and not a.from_json:
        ap.error("give SAMPLE_DIR, or --from-json FILE")
    a.volume_text = a.volume or ""
    try:
        a.volume = parse_size(a.volume) if a.volume else None
    except argparse.ArgumentTypeError as e:
        ap.error(str(e))
    short = {"gzip": "gzip -6", "bzip2": "bzip2 -9"}
    a.current = short.get(a.current, a.current.replace("-", " -", 1).replace("  ", " "))
    if a.current.startswith("gzip"):
        a.gzip = True
    if a.current.startswith("bzip2"):
        a.bzip2 = True

    if a.from_json:
        with open(a.from_json) as f:
            res = json.load(f)
        names = res["settings"]
        a.gzip, a.bzip2 = "gzip -6" in names, "bzip2 -9" in names
        a.title = a.title or res.get("title") or os.path.splitext(os.path.basename(a.from_json))[0]
        res["files"] = [f for f in res["files"] if not excluded(f["name"], a.exclude)]
    else:
        a.title = a.title or os.path.basename(os.path.abspath(a.sample_dir))
        levels = [x.strip() for x in a.levels.split(",") if x.strip()]
        setts = settings(a.kolmo, levels, a.gzip, a.bzip2)
        names = [s[0] for s in setts]
        for argv0 in {s[1][0] for s in setts}:
            if not shutil.which(argv0):
                ap.error(f"{argv0} not found (set --kolmo or PATH)")
    if a.current != "none" and a.current not in names:
        ap.error(f"--current must be none or one of: {', '.join(names)}")
    if a.volume_is == "current" and a.current == "none":
        ap.error("--volume-is current needs --current")

    if not a.from_json:
        files = [p for p in collect(a.sample_dir) if not excluded(os.path.relpath(p, a.sample_dir), a.exclude)]
        if not files:
            ap.error(f"no files under {a.sample_dir}")
        res = {"date": datetime.datetime.now().strftime("%Y-%m-%d %H:%M"), "settings": names,
               "machine": f"{platform.system()} {platform.release()}, {platform.machine()}, {os.cpu_count()} cores",
               "versions": {n: tool_version(n) for n in dict.fromkeys(s[1][0] for s in setts)},
               "repeat": a.repeat, "files": []}
        with tempfile.TemporaryDirectory(prefix="kolmo-sample-") as work:
            for i, path in enumerate(files, 1):
                n = os.path.getsize(path)
                entry = {"name": os.path.relpath(path, a.sample_dir), "bytes": n, "by": {}}
                for s in setts:
                    print(f"[{i}/{len(files)}] {entry['name']}: {s[0]}", file=sys.stderr)
                    m = measure(path, s, work, a.repeat)
                    m.update(ratio=ratio(n, m["size"]), enc_mibs=mibs(n, m["enc_s"]), dec_mibs=mibs(n, m["dec_s"]))
                    entry["by"][s[0]] = m
                res["files"].append(entry)
    if not res["files"]:
        ap.error("no files left after --exclude")
    a.repeat = res.get("repeat", a.repeat)
    res["title"] = a.title
    summarize(res, a)

    md = report_md(res, a)
    if a.out:
        with open(a.out, "w") as f:
            f.write(md)
        print(f"wrote {a.out}", file=sys.stderr)
    else:
        sys.stdout.write(md)
    if a.json:
        with open(a.json, "w") as f:
            json.dump(res, f, indent=1)


if __name__ == "__main__":
    main()
