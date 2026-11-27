#!/usr/bin/env python3
"""P2-E27 (policies identical to P2-E26): cross-aggregator preregistered fresh test of the P2-E25 CSV picker (pickcli: line regularity > 0.9916 -> zstd-12 table frame,
else production eco) vs OpenZL csv-9 on N1's complete-CLI boundary (N1 baselines.py UNCHANGED; policies()/command() only).
Frozen policies: zstd_3, openzl_csv_9 (rival), production kolmo fast and eco, tt_zstd12 (ttcli), pick (pickcli).
usage: run_e26.py run PREPARED OUT --zstd Z --openzl O --kolmo K --tt T --pick P [--seed S] | run_e26.py confirm A B"""
import argparse, importlib.util, json, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('n1baselines', HERE.parent / 'N1-baselines' / 'baselines.py')
n1 = importlib.util.module_from_spec(spec); sys.modules['n1baselines'] = n1; spec.loader.exec_module(n1)
_n1_policies, _n1_command = n1.policies, n1.command
def policies(profile, tools):
    base = _n1_policies(profile, {k: v for k, v in tools.items() if k not in ('tt', 'pick')})
    res = [r for r in base if r['id'] in ('zstd_3', 'kolmo_fast') or r['id'] == f'openzl_{profile}_9']
    res.append(dict(id='kolmo_eco', tool='kolmo', enc=[tools['kolmo'], '-m', 'eco', '-T1'], dec=[tools['kolmo'], '-d', '-T1']))
    res.append(dict(id='tt_zstd12', tool='ext', research=True, frame='production', enc=[tools['tt'], 'tt', 'zstd', '12'], dec=[tools['kolmo'], '-d', '-T1']))
    res.append(dict(id='pick', tool='ext', research=True, frame='production', enc=[tools['pick'], 'pick'], dec=[tools['kolmo'], '-d', '-T1']))
    return res
def command(policy, operation, src, dst):
    if policy['tool'] == 'ext': return list(policy['enc' if operation == 'encode' else 'dec']) + [str(src), str(dst)]
    return _n1_command(policy, operation, src, dst)
n1.policies, n1.command = policies, command
def main():
    p = argparse.ArgumentParser(); sub = p.add_subparsers(dest='cmd', required=True)
    r = sub.add_parser('run'); r.add_argument('prepared', type=Path); r.add_argument('output', type=Path)
    for t in ('zstd', 'openzl', 'kolmo', 'tt', 'pick'): r.add_argument('--' + t, required=True)
    r.add_argument('--seed', type=int, default=20261112)
    c = sub.add_parser('confirm'); c.add_argument('first', type=Path); c.add_argument('second', type=Path)
    x = p.parse_args()
    if x.cmd == 'run': n1.run(x.prepared, x.output, dict(zstd=x.zstd, openzl=x.openzl, kolmo=x.kolmo, tt=x.tt, pick=x.pick), seed=x.seed)
    else: print(json.dumps(n1.confirm(x.first, x.second), indent=2))
if __name__ == '__main__': main()
