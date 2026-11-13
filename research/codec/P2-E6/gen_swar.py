#!/usr/bin/env python3
"""40 SWAR stress CSVs: integers of 1..19 digits incl. int64 bounds, decimals of scale 1..18 up to 19 digits (seed 7).
usage: gen_swar.py OUT_DIR"""
import random, sys, pathlib
out = pathlib.Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True); random.seed(7)
def rint():
    d = random.randint(1, 19); v = random.randint(10**(d-1) if d > 1 else 0, 10**d - 1)
    if d == 19: v = min(v, 2**63 - 1)
    s = str(v); return ('-' + s) if random.random() < 0.3 and v != 0 else s
def rdec(sc):
    tot = random.randint(sc + 1, 19); v = random.randint(0, 10**tot - 1)
    if tot == 19: v = min(v, 2**63 - 1)
    ip, fp = divmod(v, 10**sc); s = f"{ip}.{fp:0{sc}d}"
    return ('-' + s) if random.random() < 0.3 and v != 0 else s
for t in range(40):
    rows = random.choice([16, 33, 500, 3000]); sc = random.randint(1, 18)
    cols = [[rint() for _ in range(rows)], [rdec(sc) for _ in range(rows)], [random.choice([str(2**63-1), str(-2**63), '0', '9'*18, '1' + '0'*18]) for _ in range(rows)]]
    (out / f's{t:02d}.csv').write_text('a,b,c\n' + ''.join(','.join(r) + '\n' for r in zip(*cols)))
