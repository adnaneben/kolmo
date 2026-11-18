"""Pairs table from perfile.py rows: python3 table.py RESULTS.tsv rival:native [...]; per file and totals (time sums)."""
import sys, collections
rows = [l.rstrip("\n").split("\t") for l in open(sys.argv[1]) if l.strip()]
R = {(r[0], r[1], r[2]): r for r in rows}
for pair in sys.argv[2:]:
    a, b = pair.split(":")
    for T in sorted({r[1] for r in rows}):
        print(f"\n**{b} against {a}, {T}**\n\n| file | {a} bytes | {b} bytes | size | {a} c / d MB/s | {b} c / d MB/s | compress | decompress | RSS KiB {a} / {b} |\n|---|---:|---:|---:|---:|---:|---:|---:|---:|")
        tot = collections.Counter()
        for f in sorted({r[0] for r in rows}):
            if (f, T, a) not in R or (f, T, b) not in R: continue
            x, y = R[(f, T, a)], R[(f, T, b)]; n = int(x[3]); za, zb = int(x[4]), int(y[4])
            ca, da, cb, db = map(float, (x[5], x[6], y[5], y[6]))
            tot["n"] += n; tot["za"] += za; tot["zb"] += zb
            tot["tca"] += n / ca; tot["tda"] += n / da; tot["tcb"] += n / cb; tot["tdb"] += n / db
            print(f"| {f} | {za:,} | {zb:,} | {100*(zb/za-1):+.2f}% | {ca:.0f} / {da:.0f} | {cb:.0f} / {db:.0f} | {ca and cb/ca:.2f}x | {db/da:.2f}x | {x[7]} / {y[7]} |")
        if tot["n"]:
            print(f"| **total** | {tot['za']:,} | {tot['zb']:,} | {100*(tot['zb']/tot['za']-1):+.2f}% | {tot['n']/tot['tca']:.0f} / {tot['n']/tot['tda']:.0f} | {tot['n']/tot['tcb']:.0f} / {tot['n']/tot['tdb']:.0f} | {tot['tca']/tot['tcb']:.2f}x | {tot['tda']/tot['tdb']:.2f}x | |")
