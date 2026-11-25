#!/usr/bin/env python3
"""Summarise the cells written by run.py and apply the archival gates. usage: report.py OUT_DIR > tables.md

Gates (benchmark/waves/2026-11-25/overnight/prior-routing/README.md, the general ratio-priority promotion gate for
L8/L9/Ultra), candidate relative to stock Ultra on the same input:
  stored bytes <= 0.98x, encode throughput >= 0.67x, decode throughput >= 0.95x,
  peak RSS <= 1.10x in each phase, median combined latency <= 1.50x.
"""
import glob, json, os, statistics as st, sys

out = sys.argv[1]
cells = [json.load(open(f)) for f in sorted(glob.glob(os.path.join(out, "cells", "*.json")))]
order = json.load(open(os.path.join(out, "order.json")))
med = st.median
by = {}
for c in cells: by.setdefault((c["input"], c["arm"]), []).append(c)

def summ(rows):
    n = rows[0]["n"]
    e = [r["enc"]["wall_s"] for r in rows]; d = [r["dec"]["wall_s"] for r in rows]; ed = [a + b for a, b in zip(e, d)]
    def j(r, k):
        try: return json.loads(r[k]["stderr"].strip().splitlines()[-1])
        except Exception: return {}
    ji = [j(r, "enc") for r in rows]; jd = [j(r, "dec") for r in rows]
    return {"n": n, "rounds": len(rows), "frames": sorted({r["frame_bytes"] for r in rows}), "frame_sha": sorted({r["frame_sha256"] for r in rows}),
            "restored_ok": all(r["roundtrip_ok"] for r in rows), "restored_sha": sorted({r["restored_sha256"] for r in rows}), "input_sha": rows[0]["input_sha256"],
            "enc_s": med(e), "dec_s": med(d), "ed_s": med(ed), "enc_mbs": n / 1e6 / med(e), "dec_mbs": n / 1e6 / med(d), "comb_mbs": n / 1e6 / med(ed),
            "enc_rss": med(r["enc"]["maxrss_b"] for r in rows) / 2**20, "dec_rss": med(r["dec"]["maxrss_b"] for r in rows) / 2**20,
            "enc_cpu": med(r["enc"]["user_s"] + r["enc"]["sys_s"] for r in rows), "dec_cpu": med(r["dec"]["user_s"] + r["dec"]["sys_s"] for r in rows),
            "init_enc_s": med(x.get("init_s", float("nan")) for x in ji) if any("init_s" in x for x in ji) else None,
            "init_dec_s": med(x.get("init_s", float("nan")) for x in jd) if any("init_s" in x for x in jd) else None,
            "load": [round(r["load1_before"], 1) for r in rows]}

S = {k: summ(v) for k, v in by.items()}
inputs = sorted({k[0] for k in S}); arms = ["stock", "control", "candidate", "zpaq"]
print("## Per input and arm (medians over rounds; MB/s = 10^6 raw bytes / wall seconds; RSS in MiB)\n")
print("| input | arm | rounds | stored B | enc MB/s | dec MB/s | combined MB/s | enc / dec RSS MiB | enc / dec CPU s | init enc / dec s | 1-min load per round | round trips |")
print("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---|")
for i in inputs:
    for a in arms:
        s = S.get((i, a))
        if not s: continue
        init = "—" if s["init_enc_s"] is None else f"{s['init_enc_s']:.4f} / {s['init_dec_s']:.4f}"
        print(f"| {i} | {a} | {s['rounds']} | {', '.join(f'{x:,}' for x in s['frames'])} | {s['enc_mbs']:.4f} | {s['dec_mbs']:.4f} | {s['comb_mbs']:.4f} | "
              f"{s['enc_rss']:.1f} / {s['dec_rss']:.1f} | {s['enc_cpu']:.2f} / {s['dec_cpu']:.2f} | {init} | {s['load']} | {'all SHA-256 OK' if s['restored_ok'] else 'FAIL'} |")
print()
print("## Startup: a 1-byte input (process start, model allocation, initialisation; medians)\n")
print("| arm | encode ms | decode ms | enc / dec RSS MiB | in-process init enc / dec ms |")
print("|---|---:|---:|---:|---:|")
for a in arms:
    s = S.get(("tiny1", a))
    if not s: continue
    init = "—" if s["init_enc_s"] is None else "%.2f / %.2f" % (s["init_enc_s"] * 1e3, s["init_dec_s"] * 1e3)
    print(f"| {a} | {s['enc_s'] * 1e3:.1f} | {s['dec_s'] * 1e3:.1f} | {s['enc_rss']:.1f} / {s['dec_rss']:.1f} | {init} |")
print()
print("## Gates: candidate against stock Ultra (prior-routing archival gate)\n")
print("| input | bytes ratio (<= 0.98) | enc thr (>= 0.67) | dec thr (>= 0.95) | enc RSS (<= 1.10) | dec RSS (<= 1.10) | combined latency (<= 1.50) | verdict |")
print("|---|---:|---:|---:|---:|---:|---:|---|")
gate_rows = {}
for i in inputs:
    if i.startswith("tiny"): continue
    s, c = S.get((i, "stock")), S.get((i, "candidate"))
    if not (s and c): continue
    g = {"bytes": c["frames"][0] / s["frames"][0], "enc": c["enc_mbs"] / s["enc_mbs"], "dec": c["dec_mbs"] / s["dec_mbs"],
         "ersa": c["enc_rss"] / s["enc_rss"], "drss": c["dec_rss"] / s["dec_rss"], "lat": c["ed_s"] / s["ed_s"]}
    ok = {"bytes": g["bytes"] <= 0.98, "enc": g["enc"] >= 0.67, "dec": g["dec"] >= 0.95, "ersa": g["ersa"] <= 1.10, "drss": g["drss"] <= 1.10, "lat": g["lat"] <= 1.50}
    gate_rows[i] = (g, ok)
    f = lambda k: f"{g[k]:.3f} {'pass' if ok[k] else '**FAIL**'}"
    print(f"| {i} | {f('bytes')} | {f('enc')} | {f('dec')} | {f('ersa')} | {f('drss')} | {f('lat')} | {'PASS' if all(ok.values()) else '**REJECT**'} |")
print()
print("## Isolating the LSTM: candidate against the harness control (the same container and process, LSTM off)\n")
print("| input | bytes ratio | enc time ratio | dec time ratio | candidate − control stored B |")
print("|---|---:|---:|---:|---:|")
for i in inputs:
    c, k = S.get((i, "candidate")), S.get((i, "control"))
    if c and k: print(f"| {i} | {c['frames'][0] / k['frames'][0]:.4f} | {c['enc_s'] / k['enc_s']:.2f} | {c['dec_s'] / k['dec_s']:.2f} | {c['frames'][0] - k['frames'][0]:+,} |")
print()
print("## Hashes\n")
print("| input | arm | input SHA-256 | frame SHA-256 | restored SHA-256 |")
print("|---|---|---|---|---|")
for i in inputs:
    for a in arms:
        s = S.get((i, a))
        if s: print(f"| {i} | {a} | `{s['input_sha']}` | `{' '.join(s['frame_sha'])}` | `{' '.join(s['restored_sha'])}` |")
print()
print("## Run order\n")
print(" → ".join(order))
json.dump({"summary": {f"{k[0]}|{k[1]}": v for k, v in S.items()}, "gates": {k: {"ratios": v[0], "pass": v[1]} for k, v in gate_rows.items()}},
          open(os.path.join(out, "summary.json"), "w"), indent=1)
