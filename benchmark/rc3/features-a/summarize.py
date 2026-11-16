#!/usr/bin/env python3
"""Summarise kpbench.jsonl (profiles) and tgbench.jsonl (targets) into the Markdown tables of RESULTS.md.
Usage: python3 summarize.py kpbench.jsonl tgbench.jsonl"""
import json, sys
from collections import defaultdict

def kp(path):
    rows = defaultdict(dict)
    for l in open(path):
        d = json.loads(l); rows[(d["mode"], d["threads"], d["family"])][d["row"].split("-")[0] + ("-dict" if d["row"].endswith("dict") else "-P" if d["row"].endswith("-P") else "")] = d
    for key in [k for k, r in rows.items() if "kolmo" not in r or "kolmo-P" not in r]: del rows[key]   # unfinished runs
    for mode, th in sorted({(m, t) for m, t, _ in rows}, key=lambda x: (x[1], ["speed", "balanced", "hc"].index(x[0]))):
        zl = {"speed": 1, "balanced": 3, "hc": 19}[mode]
        print(f"\n**`{mode}`, {th} thread{'s' if th > 1 else ''}** (zstd level {zl}; ratio = original / compressed; MB/s compress / decompress)\n")
        print("| family | files | MB | untrained | profile | zstd | zstd + dict | profile vs untrained | profile vs zstd + dict | untrained MB/s | profile MB/s | zstd + dict MB/s |")
        print("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
        for (m, t, fam), r in sorted(rows.items(), key=lambda x: x[0][2]):
            if (m, t) != (mode, th): continue
            k, p, z, zd = r.get("kolmo"), r.get("kolmo-P"), r.get("zstd"), r.get("zstd-dict")
            def pct(a, b): return "%+.1f%%" % (100.0 * (a["size"] / b["size"] - 1)) if a and b else "—"
            def sp(a): return "%.0f / %.0f" % (a["c_MBps"], a["d_MBps"]) if a else "—"
            print(f"| {fam} | {k['files']} | {k['bytes']/1e6:.1f} | {k['ratio']:.3f} | **{p['ratio']:.3f}** | {z['ratio']:.3f} | {zd['ratio'] if zd else float('nan'):.3f} | "
                  f"{pct(p, k)} | {pct(p, zd)} | {sp(k)} | {sp(p)} | {sp(zd)} |")
        if mode == "speed" and any("lz4-dict" in r for (m, t, f), r in rows.items() if (m, t) == (mode, th)):
            print(f"\n`speed` (LZ4 class) against LZ4 with the same dictionary (`lz4 -D`, its last 64 KiB):\n")
            print("| family | profile | lz4 | lz4 + dict | profile vs lz4 + dict | profile MB/s | lz4 + dict MB/s |")
            print("|---|---:|---:|---:|---:|---:|---:|")
            for (m, t, fam), r in sorted(rows.items(), key=lambda x: x[0][2]):
                if (m, t) != (mode, th) or "lz4-dict" not in r: continue
                p, l, ld = r["kolmo-P"], r["lz4"], r["lz4-dict"]
                print(f"| {fam} | **{p['ratio']:.3f}** | {l['ratio']:.3f} | {ld['ratio']:.3f} | {pct(p, ld)} | {sp(p)} | {sp(ld)} |")

def cost(target, size, n, enc, dec):
    if target.startswith("cost:cloud"):
        m = float(target.split("=")[1]) if "=" in target else 1.0
        return size / 1e9 * 0.023 * m + (enc + 3.3 * dec) * 0.054 / 3600
    if target.startswith("cost:link="):
        r = target.split("=")[1]
        bps = float(r.rstrip("Mbit").rstrip("Gbit")) * (125e3 if "Mbit" in r else 125e6)
        return enc + size / bps + dec
    return None

def floor(target):
    v = target.split(">=")[1]; f = 1e9 if "GB/s" in v else 1e6
    return target.split(">=")[0], float(v.rstrip("GMB/s")) * f

def tg(path):
    fixed = defaultdict(dict); targets = []
    for l in open(path):
        d = json.loads(l)
        if d["kind"] == "fixed": fixed[d["file"]][d["setting"]] = d
        else: targets.append(d)
    print("\n| file | MB | target | chosen (chunks) | size | best fixed setting | its size | met | regret |")
    print("|---|---:|---|---|---:|---|---:|---|---:|")
    summ = defaultdict(list)
    for t in targets:
        F = fixed[t["file"]]; n = t["bytes"]; tgt = t["target"]
        if tgt == "size":
            best = min(F.values(), key=lambda d: d["size"]); met = "—"
            regret = 100.0 * (t["size"] / best["size"] - 1); bs = best["size"]
        elif ">=" in tgt:
            kind, fl = floor(tgt)
            ok = lambda d: n / (d["dec_s"] if kind == "decode" else d["enc_s"]) >= fl
            cands = [d for d in F.values() if ok(d)]
            best = min(cands, key=lambda d: d["size"]) if cands else max(F.values(), key=lambda d: n / d["dec_s"])
            met = "yes" if ok(t) else "no (%.0f MB/s)" % (n / (t["dec_s"] if kind == "decode" else t["enc_s"]) / 1e6)
            regret = 100.0 * (t["size"] / best["size"] - 1); bs = best["size"]
        else:
            c = lambda d: cost(tgt, d["size"], n, d["enc_s"], d["dec_s"])
            best = min(F.values(), key=c); met = "—"
            regret = 100.0 * (c(t) / c(best) - 1); bs = best["size"]
        summ[tgt].append(regret)
        print(f"| {t['file']} | {n/1e6:.1f} | `{tgt}` | {t['chosen']} ({t['chunks']}) | {t['size']} | {best['setting']} | {bs} | {met} | {regret:+.1f}% |")
    print("\n| target | files | median regret | worst regret |\n|---|---:|---:|---:|")
    for k, v in summ.items():
        v = sorted(v); print(f"| `{k}` | {len(v)} | {v[len(v)//2]:+.1f}% | {v[-1]:+.1f}% |")

if __name__ == "__main__":
    kp(sys.argv[1])
    if len(sys.argv) > 2: tg(sys.argv[2])
