#!/usr/bin/env python3
"""300 random small CSVs exercising P2-E6 typing edge cases (seed 20261113). usage: gen_edge.py OUT_DIR"""
import random, sys, pathlib
out = pathlib.Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True); random.seed(20261113)
def field(kind):
    r = random.random()
    if kind == 'int': return random.choice([str(random.randint(-10**6, 10**6)), '0', str(2**63-1), str(-2**63), '-0' if r < 0.02 else '7', '007' if r < 0.02 else '12', str(2**63) if r < 0.01 else '5'])
    if kind == 'dec':
        d = random.choice([1, 2, 3]); v = random.randint(-10**5, 10**5); s = ('-' if v < 0 else '') + str(abs(v) // 10**d) + '.' + str(abs(v) % 10**d).zfill(d)
        return random.choice([s, s, s, '-0.' + '0' * d if r < 0.02 else s, '1.' if r < 0.01 else s, '.5' if r < 0.01 else s])
    if kind == 'dec2': return '%d.%02d' % (random.randint(0, 999), random.randint(0, 99))
    return random.choice(['abc', '', 'x y', 'a-b', 'é', ' 12', '12 ', '1e5', '0x1F', 'NaN', '-', '+3'])
for t in range(300):
    k = random.randint(1, 6); rows = random.choice([16, 17, 40, 200]); kinds = [random.choice(['int', 'dec', 'dec2', 'text']) for _ in range(k)]
    s = '\n'.join(['h' + ','.join('c%d' % j for j in range(k))] + [','.join(field(kd) for kd in kinds) for _ in range(rows)]) + '\n'
    r = random.random()
    if r < 0.03: s = s.replace('\n', '\r\n')
    elif r < 0.06: s = s[:-1]
    elif r < 0.08: s = s.replace(',', '","', 1)
    elif r < 0.10: s = s.replace('a', '\0', 1)
    (out / ('e%03d.csv' % t)).write_text(s, encoding='utf-8', newline='')
