#!/usr/bin/env python3
"""P2-E17: ARCHIVE-mode frontier in one run: production-format type-first frames (encoder `p2e13 enc`, decoded by the
PRODUCTION kolmo CLI) together with production kolmo fast / hc / max, against OpenZL's typed level-9 profile, on the
P2-E10 61-input development cohort and N1's complete-CLI boundary. Development data.

Imports research/codec/N1-baselines/baselines.py UNCHANGED and replaces only policies() and command(). Rivals and
controls kept from N1's list: OpenZL typed profile level 9 (archive rival, whole file, set per input by P2-E10's
prepare_e10.py), zstd_3 (reference), production kolmo fast and hc; production kolmo -m max is added as in P2-E10.

usage: run_e17.py run PREPARED OUT --zstd Z --openzl O --kolmo K --tb P2E13 [--seed S]
       run_e17.py confirm FIRST SECOND
"""
import argparse, importlib.util, json, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('n1baselines', HERE.parent / 'N1-baselines' / 'baselines.py')
n1 = importlib.util.module_from_spec(spec); sys.modules['n1baselines'] = n1; spec.loader.exec_module(n1)

RESEARCH = [('tb_best', 'archive'), ('tp_pent', 'archive'), ('tp_arch19', 'archive')]   # frozen before any P2-E17 measurement
KEEP = {'zstd_3', 'kolmo_fast', 'kolmo_hc'}
_n1_policies, _n1_command = n1.policies, n1.command


def policies(profile, tools):
    base = _n1_policies(profile, {k: v for k, v in tools.items() if k != 'tb'})
    res = [r for r in base if r['id'] in KEEP or r['id'] == f'openzl_{profile}_9']
    for cand, fam in RESEARCH:
        res.append(dict(id=cand, tool='tb', research=True, mode_family=fam, decoder='production kolmo CLI',
                        enc=[tools['tb'], 'enc', cand], dec=[tools['kolmo'], '-d', '-T1']))
    res.append(dict(id='kolmo_max', tool='kolmo', enc=[tools['kolmo'], '-m', 'max', '-T1'], dec=[tools['kolmo'], '-d', '-T1']))
    seen, uniq = set(), []
    for r in res:
        if r['id'] not in seen: seen.add(r['id']); uniq.append(r)
    return uniq


def command(policy, operation, src, dst):
    if policy['tool'] == 'tb':
        return list(policy['enc' if operation == 'encode' else 'dec']) + [str(src), str(dst)]
    return _n1_command(policy, operation, src, dst)


n1.policies, n1.command = policies, command


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest='cmd', required=True)
    r = sub.add_parser('run'); r.add_argument('prepared', type=Path); r.add_argument('output', type=Path)
    for t in ('zstd', 'openzl', 'kolmo', 'tb'): r.add_argument('--' + t, required=True)
    r.add_argument('--seed', type=int, default=20261112)
    c = sub.add_parser('confirm'); c.add_argument('first', type=Path); c.add_argument('second', type=Path)
    x = p.parse_args()
    if x.cmd == 'run':
        n1.run(x.prepared, x.output, dict(zstd=x.zstd, openzl=x.openzl, kolmo=x.kolmo, tb=x.tb), seed=x.seed)
    else:
        print(json.dumps(n1.confirm(x.first, x.second), indent=2))


if __name__ == '__main__':
    main()
