#!/usr/bin/env python3
"""P2-E20: preregistered FRESH source-disjoint validation (PROTOCOL.md) on N1's complete-CLI boundary.
Imports research/codec/N1-baselines/baselines.py UNCHANGED; replaces only policies() and command().
Policies (frozen before any P2-E20 data was read):
  rivals/reference: zstd_3 (speed rival), openzl_<profile>_9 (archive rival), production kolmo fast / hc / -m max
  production-format research frames (encoder p2e13 enc, decoder PRODUCTION kolmo -d -T1):
    speed: tp_phuf_pg16 (H1 rule), tp_phuf, tb_lazy, tb_best, tp_pent     archive: tp_pent, tb_best, tp_arch19
  research containers (P2-D1 test, encoder and decoder mcli): mcsv_planes_huf (speed), mcsv_prod19 (archive)
usage: run_e20.py run PREPARED OUT --zstd Z --openzl O --kolmo K --tb P2E13 --mcli M [--seed S]
       run_e20.py confirm FIRST SECOND"""
import argparse, importlib.util, json, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('n1baselines', HERE.parent / 'N1-baselines' / 'baselines.py')
n1 = importlib.util.module_from_spec(spec); sys.modules['n1baselines'] = n1; spec.loader.exec_module(n1)
FRAMES = ['tp_phuf_pg16', 'tp_phuf', 'tb_lazy', 'tb_best', 'tp_pent', 'tp_arch19']
CONTAINERS = ['mcsv_planes_huf', 'mcsv_prod19']
_n1_policies, _n1_command = n1.policies, n1.command
def policies(profile, tools):
    base = _n1_policies(profile, {k: v for k, v in tools.items() if k not in ('tb', 'mcli')})
    res = [r for r in base if r['id'] in ('zstd_3', 'kolmo_fast', 'kolmo_hc') or r['id'] == f'openzl_{profile}_9']
    res.append(dict(id='kolmo_max', tool='kolmo', enc=[tools['kolmo'], '-m', 'max', '-T1'], dec=[tools['kolmo'], '-d', '-T1']))
    for c in FRAMES:
        res.append(dict(id=c, tool='ext', research=True, frame='production', enc=[tools['tb'], 'enc', c], dec=[tools['kolmo'], '-d', '-T1']))
    if profile == 'csv':
        for c in CONTAINERS:
            res.append(dict(id=c, tool='ext', research=True, frame='research_container', enc=[tools['mcli'], '-p', c, '-T1'], dec=[tools['mcli'], '-d', '-T1']))
    return res
def command(policy, operation, src, dst):
    if policy['tool'] == 'ext': return list(policy['enc' if operation == 'encode' else 'dec']) + [str(src), str(dst)]
    return _n1_command(policy, operation, src, dst)
n1.policies, n1.command = policies, command
def main():
    p = argparse.ArgumentParser(); sub = p.add_subparsers(dest='cmd', required=True)
    r = sub.add_parser('run'); r.add_argument('prepared', type=Path); r.add_argument('output', type=Path)
    for t in ('zstd', 'openzl', 'kolmo', 'tb', 'mcli'): r.add_argument('--' + t, required=True)
    r.add_argument('--seed', type=int, default=20261112)
    c = sub.add_parser('confirm'); c.add_argument('first', type=Path); c.add_argument('second', type=Path)
    x = p.parse_args()
    if x.cmd == 'run': n1.run(x.prepared, x.output, dict(zstd=x.zstd, openzl=x.openzl, kolmo=x.kolmo, tb=x.tb, mcli=x.mcli), seed=x.seed)
    else: print(json.dumps(n1.confirm(x.first, x.second), indent=2))
if __name__ == '__main__': main()
