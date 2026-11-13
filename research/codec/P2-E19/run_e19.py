#!/usr/bin/env python3
"""P2-E19: GHCN-only archive check of the existing-leaf BWT frame (P2-E18 gf.c: KG_INT order 0 + C_BSC leaf, production
CZN1 frame decoded by the PRODUCTION kolmo CLI) against OpenZL typed le-i32 level 9, on N1's complete-CLI boundary.
Imports N1 baselines.py UNCHANGED; replaces policies()/command(). Development data (observed GHCN inputs).
usage: run_e19.py subset PREPARED OUT          (copy the E02E__ghcn_* rows and files)
       run_e19.py run PREPARED OUT --zstd Z --openzl O --kolmo K --tb P2E13 --gf GF [--seed S]
       run_e19.py confirm FIRST SECOND"""
import argparse, importlib.util, json, shutil, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('n1baselines', HERE.parent / 'N1-baselines' / 'baselines.py')
n1 = importlib.util.module_from_spec(spec); sys.modules['n1baselines'] = n1; spec.loader.exec_module(n1)
_n1_policies, _n1_command = n1.policies, n1.command
def policies(profile, tools):
    base = _n1_policies(profile, {k: v for k, v in tools.items() if k not in ('tb', 'gf')})
    res = [r for r in base if r['id'] in ('zstd_3', 'kolmo_fast') or r['id'] == f'openzl_{profile}_9']
    res.append(dict(id='tp_pent', tool='ext', research=True, mode_family='archive', enc=[tools['tb'], 'enc', 'tp_pent'], dec=[tools['kolmo'], '-d', '-T1']))
    for k, p in (('bsc', '0'), ('bsc', '1')):   # frozen before any P2-E19 measurement
        res.append(dict(id=f'gf_{k}_p{p}', tool='ext', research=True, mode_family='archive', enc=[tools['gf'], 'gf', k, p], dec=[tools['kolmo'], '-d', '-T1']))
    return res
def command(policy, operation, src, dst):
    if policy['tool'] == 'ext': return list(policy['enc' if operation == 'encode' else 'dec']) + [str(src), str(dst)]
    return _n1_command(policy, operation, src, dst)
n1.policies, n1.command = policies, command
def main():
    p = argparse.ArgumentParser(); sub = p.add_subparsers(dest='cmd', required=True)
    s = sub.add_parser('subset'); s.add_argument('prepared', type=Path); s.add_argument('output', type=Path)
    r = sub.add_parser('run'); r.add_argument('prepared', type=Path); r.add_argument('output', type=Path)
    for t in ('zstd', 'openzl', 'kolmo', 'tb', 'gf'): r.add_argument('--' + t, required=True)
    r.add_argument('--seed', type=int, default=20261112)
    c = sub.add_parser('confirm'); c.add_argument('first', type=Path); c.add_argument('second', type=Path)
    x = p.parse_args()
    if x.cmd == 'subset':
        m = json.loads((x.prepared / 'manifest.json').read_text()); rows = [r for r in m['inputs'] if r['id'].startswith('E02E__ghcn')]
        (x.output / 'inputs').mkdir(parents=True)
        for r in rows: shutil.copy2(x.prepared / r['file'], x.output / r['file'])
        m['inputs'] = rows; (x.output / 'manifest.json').write_text(json.dumps(m, indent=1)); print(len(rows), 'GHCN inputs')
    elif x.cmd == 'run':
        n1.run(x.prepared, x.output, dict(zstd=x.zstd, openzl=x.openzl, kolmo=x.kolmo, tb=x.tb, gf=x.gf), seed=x.seed)
    else:
        print(json.dumps(n1.confirm(x.first, x.second), indent=2))
if __name__ == '__main__': main()
