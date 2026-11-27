#!/usr/bin/env python3
"""Per-cut effect table for Ultra+ against stock ultra (-U) and ultra level 2 from the kcuts TSV files.
usage: table.py RUNS_DIR SET [ULTRA_CUTS_TXT] [CMIX_SIZES]  -> markdown on stdout"""
import sys, os
d, st = sys.argv[1], sys.argv[2]
def tsv(p):
    r = {}
    if os.path.exists(p):
        for l in open(p):
            f = l.split('\t')
            if len(f) >= 9: r[f[0]] = dict(n=int(f[1]), z=int(f[2]), ok=f[3], enc=float(f[4]), dec=float(f[5]), rss_e=int(f[6]), rss_d=int(f[7]), ver=f[8].strip())
    return r
up, ul2 = tsv(f'{d}/up3.{st}.tsv'), tsv(f'{d}/ul2.{st}.tsv')
ult = {}
if len(sys.argv) > 3:
    for l in open(sys.argv[3]):
        s, n, z = l.split(); ult[n] = int(z) if s == st else ult.get(n)
cm = {}
if len(sys.argv) > 4 and os.path.exists(sys.argv[4]):
    for l in open(sys.argv[4]):
        f = l.split()
        if f[0] == st: cm[f[1]] = int(f[3])
print('| cut | bytes | -U | -U level 2 | **Ultra+** | vs -U | vs level 2 | cmix v21 | enc s | dec s | RSS enc/dec MB | frame | RT |')
print('|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|')
T = [0, 0, 0, 0]
for n in sorted(up):
    u = up[n]; a = ult.get(n); b = ul2.get(n, {}).get('z')
    vs = lambda x: f'{(u["z"] / x - 1) * 100:+.2f}%' if x else ''
    c = cm.get(n)
    print(f'| {n} | {u["n"]:,} | {a or "":,} | {b or "":,} | **{u["z"]:,}** | {vs(a)} | {vs(b)} | {f"{c:,}" if c else ""} | {u["enc"]:.0f} | {u["dec"]:.0f} | {u["rss_e"] // 1000000}/{u["rss_d"] // 1000000} | v{int(u["ver"], 16)} | {u["ok"]} |')
    if a and b: T[0] += a; T[1] += b; T[2] += u['z']; T[3] += u['n']
if T[0]:
    print(f'| **total** | {T[3]:,} | {T[0]:,} | {T[1]:,} | **{T[2]:,}** | {(T[2] / T[0] - 1) * 100:+.2f}% | {(T[2] / T[1] - 1) * 100:+.2f}% | | | | | | |')
