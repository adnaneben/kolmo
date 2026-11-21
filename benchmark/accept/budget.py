#!/usr/bin/env python3
"""The budget rule (TARGETS.md, 12 November 2026): inside each mode, the smallest option whose measured
cost stays within the cost of that mode's competitor on the same file.

  speed tier    competitor zstd -3; cost = compress + transfer at the reference link + decompress (TRADEOFFS model 2)
  archive tier  competitor OpenZL (its cheapest setting for the file); cost = CPU + storage (TRADEOFFS model 3)

For every kolmo setting the script reports on how many files it is within the budget and how its size compares;
"oracle" is the smallest kolmo result within the budget, which is what a perfect planner would pick.

  python3 benchmark/accept/budget.py speed   nightly/<branch>-<hash>.json [--gbit 1]
  python3 benchmark/accept/budget.py archive benchmark/openzl/results.json [--days 30]
"""
import argparse, collections, json

def one(x, k):   # one thread means one thread (TARGETS.md target 5)
    return x[k + '_cpu'] if x.get(k + '_cpu', 0) > x[k] * 1.03 else x[k]

def report(files, comp_name, cost, show):
    """files: {file: {setting: measurement}} with the competitor under comp_name."""
    sets = sorted({s for d in files.values() for s in d if s != comp_name})
    tot = {s: [0, 0, 0, 0, 0] for s in sets + ['oracle']}   # files, in budget, bytes, competitor bytes, smaller
    over = collections.defaultdict(list); none = []
    for f, d in sorted(files.items()):
        z = d[comp_name]; zc = cost(z); inb = []
        for s in sets:
            a = d.get(s)
            if not a: continue
            T = tot[s]; T[0] += 1; T[2] += a['size']; T[3] += z['size']; T[4] += a['size'] < z['size']
            if cost(a) <= zc: T[1] += 1; inb.append((a['size'], s))
            else: over[s].append((cost(a) / zc, f))
        T = tot['oracle']; T[0] += 1; T[3] += z['size']
        if inb: T[1] += 1; T[2] += min(inb)[0]; T[4] += min(inb)[0] < z['size']
        else: none.append(f); T[2] += z['size']
    print(f"{'setting':22} {'in budget':>10} {'smaller':>8} {'size vs competitor':>19}")
    for s in sets + ['oracle']:
        T = tot[s]
        if T[0]: print(f"{s:22} {T[1]:>4} of {T[0]:<3} {T[4]:>8} {T[2] / T[3]:>19.3f}")
    if none: print('no kolmo setting within the budget:', ', '.join(none))
    for s in show:
        if over.get(s): print(f"{s} over budget: " + ', '.join(f"{f} x{c:.2f}" for c, f in sorted(over[s], reverse=True)))

def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('tier', choices=['speed', 'archive']); p.add_argument('results')
    p.add_argument('--gbit', type=float, default=1.0, help='reference link for the speed tier')
    p.add_argument('--days', type=float, default=30, help='storage lifetime for the archive tier')
    p.add_argument('--storage', type=float, default=0.023); p.add_argument('--vcpu-hour', type=float, default=3.89 / 72)
    p.add_argument('--reads', type=float, default=3.3)
    a = p.parse_args(); r = json.load(open(a.results)); files = collections.defaultdict(dict)
    if a.tier == 'speed':
        for x in r:
            if not x.get('ok'): continue
            s = x['set']
            if s == 'zstd -3': files[x['file']]['zstd -3'] = x
            elif s.startswith('kolmo cand ') and s.split()[2] in ('speed', 'rt', 'fast'): files[x['file']][s[11:]] = x
        cost = lambda m: one(m, 'tc') + one(m, 'td') + m['size'] * 8 / (a.gbit * 1e9)
        print(f"speed tier against zstd -3, total time at {a.gbit:g} Gbit/s")
        report(files, 'zstd -3', cost, ['fast', 'speed'])
    else:
        for e in r:
            k = {x['set']: dict(x['m'], n=e['n']) for x in e['rows'] if x['m'] and x['m'].get('ok')}
            cost = lambda m: (m['tc'] + a.reads * m['td']) / 3600 * a.vcpu_hour + m['size'] / 1e9 * a.storage * a.days / 30
            oz = [m for s, m in k.items() if s.startswith('openzl')]
            if not oz: continue
            d = {s[6:]: m for s, m in k.items() if s.startswith('kolmo') and 'ultra' not in s}
            d['OpenZL'] = min(oz, key=cost); files[e['file']] = d
        print(f"archive tier against OpenZL's cheapest setting per file, CPU + storage for {a.days:g} days")
        report(files, 'OpenZL', cost, ['hc', 'max'])

if __name__ == '__main__':
    main()
