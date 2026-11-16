#!/usr/bin/env python3
"""Markdown tables from bench/krans/run.sh results.  usage: summarize.py <results dir> [more dirs: extra compilers]

Aggregates over the 12 Silesia files per stream type (raw, lo, hi): sizes are summed; a speed is total input bytes
over total time (each file's time = size / its MB/s), i.e. the throughput of coding the 12 files back to back."""
import json
import os
import sys
from collections import defaultdict

STREAMS = ["raw", "lo", "hi"]


def load(d):
    k = [json.loads(l) for l in open(os.path.join(d, "krans.jsonl")) if l.strip()]
    rp = os.path.join(d, "rivals.jsonl")
    r = [json.loads(l) for l in open(rp) if l.strip()] if os.path.exists(rp) else []
    return k, r


def agg(rows, key):
    """rows of one coder on one stream type -> (size, csize, enc, {dec...}, extra)"""
    n = sum(x["size"] for x in rows)
    z = sum(x["csize"] for x in rows)
    def thr(get):
        t = 0.0
        for x in rows:
            v = get(x)
            if not v:
                return 0.0
            t += x["size"] / 1e6 / v
        return n / 1e6 / t if t else 0.0
    return n, z, thr


def main():
    d = sys.argv[1]
    k, r = load(d)
    ok = all(x.get("verified") for x in k + r if "verified" in x)
    by = defaultdict(list)
    for x in k:
        by[(x["config"], x["stream"])].append(x)
    rv = defaultdict(list)
    for x in r:
        if "error" not in x:
            rv[(x["coder"], x["stream"])].append(x)
    configs = []
    for x in k:
        if x["config"] not in configs:
            configs.append(x["config"])
    out = []
    p = out.append
    p(f"All round trips verified: {'yes' if ok else 'NO'}\n")
    for s in STREAMS:
        n = sum(x["size"] for x in by[(configs[0], s)])
        p(f"\n#### Stream `{s}` ({n / 1e6:.1f} MB over 12 files)\n")
        p("| coder | config | size (bytes) | % of input | vs huf | enc MB/s | dec MB/s AVX2 | dec MB/s scalar |")
        p("|---|---|---:|---:|---:|---:|---:|---:|")
        hz = sum(x["csize"] for x in rv[("huf", s)]) if rv[("huf", s)] else 0
        rows = []
        for c in configs:
            g = by[(c, s)]
            if not g:
                continue
            n, z, thr = agg(g, c)
            mt = ""
            if "threads" in g[0]:
                mt = f" (T{g[0]['threads']}: enc {thr(lambda x: x['enc_mbs_mt']):.0f}, dec {thr(lambda x: x['dec_mbs_mt']):.0f})"
            rows.append(("krans", c + mt, z, n, thr(lambda x: x["enc_mbs"]), thr(lambda x: x["dec_mbs"].get("avx2")),
                         thr(lambda x: x["dec_mbs"].get("scalar"))))
        for c in ["huf", "huf-f0", "fse", "lz4", "zstd1", "zstd3"]:
            g = rv[(c, s)]
            if not g:
                continue
            n, z, thr = agg(g, c)
            rows.append((c, "128 KiB chunks" if c in ("huf", "huf-f0", "fse") else "whole stream", z, n,
                         thr(lambda x: x["enc_mbs"]), thr(lambda x: x["dec_mbs"]), None))
        for name, c, z, n, e, da, ds in rows:
            vs = f"{100.0 * (z / hz - 1):+.2f}%" if hz else ""
            p(f"| {name} | {c} | {z:,} | {100.0 * z / n:.2f} | {vs} | {e:.0f} | {da:.0f} | {'' if ds is None else f'{ds:.0f}'} |")
    # order-1 gain per file
    p("\n#### Order 1 against order 0, per file (32 ways, 256 KiB blocks)\n")
    p("| file | stream | o0 p12 | o1 p10 (forced) | o1 gain | auto | auto gain | auto blocks o1/all | huf |")
    p("|---|---|---:|---:|---:|---:|---:|---:|---:|")
    def idx(c, s):
        return {x["file"]: x for x in by[(c, s)]}
    for s in STREAMS:
        a, b, au = idx("o0-w32-p12", s), idx("o1-w32-p10", s), idx("auto-w32", s)
        hh = {x["file"]: x for x in rv[("huf", s)]}
        for f in sorted(a):
            if f not in b or f not in au:
                continue
            z0, z1, za = a[f]["csize"], b[f]["csize"], au[f]["csize"]
            bl = au[f]["blocks"]
            tot = bl["stored"] + bl["rle"] + bl["o0"] + bl["o1"]
            h = hh.get(f, {}).get("csize", 0)
            p(f"| {f} | {s} | {z0:,} | {z1:,} | {100.0 * (1 - z1 / z0):+.1f}% | {za:,} | {100.0 * (1 - za / z0):+.1f}% | {bl['o1']}/{tot} | {h:,} |")
    print("\n".join(out))


if __name__ == "__main__":
    main()
