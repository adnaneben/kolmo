"""summarize.py RUNS.jsonl BASE AA NEW...: markdown tables of the fastest time per build, the speed-up of each NEW over
BASE (fastest of the rounds, and median in brackets), and the A/A control (AA is the same binary as BASE)."""
import json, statistics, sys
rows = [json.loads(l) for l in open(sys.argv[1])]; base, aa, new = sys.argv[2], sys.argv[3], sys.argv[4:]
def sp(r, n): return min(r["times_s"][base]) / min(r["times_s"][n]), statistics.median(r["times_s"][base]) / statistics.median(r["times_s"][n])
for d, title in (("c", "compression"), ("d", "decompression")):
    print(f"\n### {title}\n")
    print("| input | MB | mode | T | " + base + " ms | " + " | ".join(f"{n} ms | {n}/{base}" for n in new) + f" | A/A ({aa}) |")
    print("|---|---:|---|---:|---:|" + "---:|---:|" * len(new) + "---:|")
    for r in rows:
        if r["dir"] != d: continue
        cells = " | ".join(f"{min(r['times_s'][n]) * 1e3:.1f} | {sp(r, n)[0]:.2f}× ({sp(r, n)[1]:.2f})" for n in new)
        print(f"| {r['file']} | {r['bytes'] / 1e6:.1f} | {r['mode']} | {r['threads']} | {min(r['times_s'][base]) * 1e3:.1f} | {cells} | {sp(r, aa)[0]:.3f} |")
aas = sorted(abs(sp(r, aa)[0] - 1) for r in rows)
gap = sorted(sorted(r["times_s"][base])[1] / min(r["times_s"][base]) - 1 for r in rows if len(r["times_s"][base]) > 1)
q = lambda v, p: v[min(len(v) - 1, int(p * len(v)))]
print(f"\nNoise: |A/A - 1| on the fastest run: median {100 * q(aas, .5):.1f}%, 90th percentile {100 * q(aas, .9):.1f}%, max {100 * aas[-1]:.1f}% "
      f"({len(aas)} cases). Second-fastest over fastest of {base}: median {100 * q(gap, .5):.1f}%, 90th percentile {100 * q(gap, .9):.1f}%.")
for n in new:
    import math
    for d in ("c", "d"):
        v = [sp(r, n)[0] for r in rows if r["dir"] == d]
        print(f"Geometric mean {n}/{base}, {'compression' if d == 'c' else 'decompression'}: {math.exp(sum(map(math.log, v)) / len(v)):.3f}× over {len(v)} cases")
