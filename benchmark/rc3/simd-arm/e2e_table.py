"""Markdown table of ab.py --base results (e2e.sh): per input, mode and direction, base and new times and the ratio.

usage: python3 -I benchmark/rc3/simd-arm/e2e_table.py results/e2e.jsonl
"""
import json
import sys

MODE = {"speed": "speed", "balanced": "balanced", "eco": "eco", "hc": "hc", "ratio": "ratio", "x": "-x"}
rows = [json.loads(l) for l in open(sys.argv[1]) if l.strip()]
print("| input | mode | direction | base ms | new ms | new/base [IQR] | A/A [IQR] | change |")
print("|---|---|---|---:|---:|---:|---:|---:|")
for r in rows:
    name, mode, d = r["case"].split()
    noise = max(abs(r["aa_q1"] - 1), abs(r["aa_q3"] - 1), abs(r["aa"] - 1))
    sig = r["ratio_q3"] < 1 - noise or r["ratio_q1"] > 1 + noise
    ch = f"{(r['ratio'] - 1) * 100:+.1f}%" if sig else "noise"
    print(f"| {name} | {MODE.get(mode, mode)} | {'decode' if d == 'd' else 'compress'} | {r['A_ms']:.2f} | {r['B_ms']:.2f} | "
          f"{r['ratio']:.3f} [{r['ratio_q1']:.3f}, {r['ratio_q3']:.3f}] | {r['aa']:.3f} [{r['aa_q1']:.3f}, {r['aa_q3']:.3f}] | {ch} |")
