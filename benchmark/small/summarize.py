#!/usr/bin/env python3
"""Tables for benchmark/small/RESULTS.md from two smallbench runs (the base branch and this build).

usage: summarize.py BASE.tsv NEW.tsv > tables.md
Rows are keyed by (kind, size, coder, level, threads). Rival rows come from NEW (the same library build in both runs).
"""
import csv
import math
import sys
from collections import defaultdict

KINDS = ["json", "log", "csv", "kv", "code", "binary"]
SIZES = [64, 256, 1024, 4096, 16384, 65536]
LEVELS = list(range(1, 10))


def load(path):
    t = {}
    for r in csv.DictReader(open(path), delimiter="\t"):
        k = (r["kind"], int(r["size"]), r["coder"], int(r["level"]), int(r["threads"]))
        t[k] = r
    return t


def sz(n):
    return "%d B" % n if n < 1024 else "%d KiB" % (n // 1024)


def gmean(v):
    v = [x for x in v if x > 0]
    return math.exp(sum(math.log(x) for x in v) / len(v)) if v else float("nan")


def main():
    base, new = load(sys.argv[1]), load(sys.argv[2])
    out = []
    P = out.append

    # 1. this PR against main, per level and size (all six kinds together)
    P("### Before and after, per level and input size (one thread, six kinds together)\n")
    P("Size: total output bytes, PR against main. Times: geometric mean over the six kinds of the per-call time ratio "
      "(below 1.00 is faster). Heap: the largest per-call heap working set over the six kinds, main -> PR.\n")
    P("| level | size | bytes main | bytes PR | change | compress time PR/main | decompress time PR/main | compress heap KiB | decompress heap KiB |")
    P("|---|---|---|---|---|---|---|---|---|")
    for lv in LEVELS:
        for s in SIZES:
            rb = [base.get((k, s, "kolmo", lv, 1)) for k in KINDS]
            rn = [new.get((k, s, "kolmo", lv, 1)) for k in KINDS]
            if not all(rb) or not all(rn):
                continue
            ob = sum(int(r["out_bytes"]) for r in rb)
            on = sum(int(r["out_bytes"]) for r in rn)
            ct = gmean([float(b["c_us"]) and float(n["c_us"]) / float(b["c_us"]) for b, n in zip(rb, rn)])
            dt = gmean([float(b["d_us"]) and float(n["d_us"]) / float(b["d_us"]) for b, n in zip(rb, rn)])
            ckb = max(float(r["c_kib"]) for r in rb); ckn = max(float(r["c_kib"]) for r in rn)
            dkb = max(float(r["d_kib"]) for r in rb); dkn = max(float(r["d_kib"]) for r in rn)
            P("| %d | %s | %d | %d | %+.2f%% | %.2f | %.2f | %.0f -> %.0f | %.0f -> %.0f |"
              % (lv, sz(s), ob, on, 100.0 * (on / ob - 1), ct, dt, ckb, ckn, dkb, dkn))
    P("")

    # 2. per kind detail of the size changes
    P("### Size change per kind (PR against main, one thread)\n")
    P("Only settings whose bytes changed; every other (kind, size, level) wrote identical bytes (same FNV-1a hash).\n")
    P("| kind | size | level | main | PR | change |")
    P("|---|---|---|---|---|---|")
    same = diff = 0
    for k in KINDS:
        for s in SIZES:
            for lv in LEVELS:
                b, n = base.get((k, s, "kolmo", lv, 1)), new.get((k, s, "kolmo", lv, 1))
                if not b or not n:
                    continue
                if b["hash"] == n["hash"]:
                    same += 1
                    continue
                diff += 1
                P("| %s | %s | %d | %s | %s | %+.3f%% |" % (k, sz(s), lv, b["out_bytes"], n["out_bytes"],
                                                         100.0 * (int(n["out_bytes"]) / int(b["out_bytes"]) - 1)))
    P("\n%d settings identical, %d changed.\n" % (same, diff))

    # 3. one and ten threads
    P("### One and ten threads (PR)\n")
    P("| level | settings | same bytes at -T1 and -T10 | compress time T10/T1 (geo. mean) | decompress time T10/T1 |")
    P("|---|---|---|---|---|")
    for lv in LEVELS:
        pairs = [(new.get((k, s, "kolmo", lv, 1)), new.get((k, s, "kolmo", lv, 10))) for k in KINDS for s in SIZES]
        pairs = [(a, b) for a, b in pairs if a and b]
        if not pairs:
            continue
        eq = sum(a["hash"] == b["hash"] for a, b in pairs)
        ct = gmean([float(b["c_us"]) / float(a["c_us"]) for a, b in pairs if float(a["c_us"]) > 0])
        dt = gmean([float(b["d_us"]) / float(a["d_us"]) for a, b in pairs if float(a["d_us"]) > 0])
        P("| %d | %d | %d | %.2f | %.2f |" % (lv, len(pairs), eq, ct, dt))
    P("")

    # 4. against the rivals, per kind and size
    riv = [("lz4", 1), ("zstd", 1), ("zstd", 3), ("zstd", 9), ("zstd", 19), ("brotli", 5), ("brotli", 11)]
    kl = [1, 2, 3, 5, 7, 9]
    P("### Ratio against zstd, Brotli and LZ4 (PR, one thread)\n")
    P("Input bytes / output bytes over all items of the kind and size (higher is better). Kolmo frames carry 52 bytes "
      "of container per item (32 header, 16 index, 4 plan), zstd 9-13 (content size, no checksum), the LZ4 frame "
      "7-15 + 4, Brotli none.\n")
    P("| kind | size | " + " | ".join("K%d" % l for l in kl) + " | " + " | ".join("%s %d" % r for r in riv) + " |")
    P("|---|---|" + "---|" * (len(kl) + len(riv)))
    for k in KINDS:
        for s in SIZES:
            cells = []
            for l in kl:
                r = new.get((k, s, "kolmo", l, 1)); cells.append("%.3f" % float(r["ratio"]) if r else "")
            for c, l in riv:
                r = new.get((k, s, c, l, 1)); cells.append("%.3f" % float(r["ratio"]) if r else "")
            P("| %s | %s | %s |" % (k, sz(s), " | ".join(cells)))
    P("")
    P("### Time per call, microseconds (PR, one thread): compress / decompress\n")
    P("| kind | size | " + " | ".join("K%d" % l for l in kl) + " | " + " | ".join("%s %d" % r for r in riv) + " |")
    P("|---|---|" + "---|" * (len(kl) + len(riv)))
    for k in KINDS:
        for s in SIZES:
            cells = []
            for key in [("kolmo", l) for l in kl] + riv:
                r = new.get((k, s, key[0], key[1], 1))
                cells.append("%s / %s" % (fmt_us(r["c_us"]), fmt_us(r["d_us"])) if r else "")
            P("| %s | %s | %s |" % (k, sz(s), " | ".join(cells)))
    P("")
    P("### Heap working set of one call, KiB (PR): compress / decompress, largest over the six kinds\n")
    P("| size | " + " | ".join("K%d" % l for l in kl) + " | " + " | ".join("%s %d" % r for r in riv) + " |")
    P("|---|" + "---|" * (len(kl) + len(riv)))
    for s in SIZES:
        cells = []
        for key in [("kolmo", l) for l in kl] + riv:
            rs = [new.get((k, s, key[0], key[1], 1)) for k in KINDS]
            rs = [r for r in rs if r]
            cells.append("%.0f / %.0f" % (max(float(r["c_kib"]) for r in rs), max(float(r["d_kib"]) for r in rs)) if rs else "")
        P("| %s | %s |" % (sz(s), " | ".join(cells)))
    P("")

    # 5. container share: what a compact frame would save
    P("### Container share of Kolmo's output (PR, level 3, one thread)\n")
    P("frame_bytes / out_bytes; a one-block compact frame (docs/proposals/small-inputs.md) would replace 52 bytes per "
      "item by about 13.\n")
    P("| kind | " + " | ".join(sz(s) for s in SIZES) + " |")
    P("|---|" + "---|" * len(SIZES))
    for k in KINDS:
        cells = []
        for s in SIZES:
            r = new.get((k, s, "kolmo", 3, 1))
            cells.append("%.1f%%" % (100.0 * int(r["frame_bytes"]) / int(r["out_bytes"])) if r and "frame_bytes" in r else "")
        P("| %s | %s |" % (k, " | ".join(cells)))
    P("")
    print("\n".join(out))


def fmt_us(x):
    v = float(x)
    return "%.1f" % v if v < 100 else "%.0f" % v


if __name__ == "__main__":
    main()
