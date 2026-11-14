#!/usr/bin/env python3
"""P2-E12: type-first pipelines written as GENUINE Kolmo frames (research encoder tpcli, P2-E11) and decoded by the
PRODUCTION kolmo CLI, on N1's complete-CLI boundary with N1's rivals and controls. Development data.

Imports research/codec/N1-baselines/baselines.py UNCHANGED and replaces only its module-level policies() and
command() so that the same run() measures, on the same prepared inputs and pinned rival binaries:
  N1's own policies (zstd-3, kolmo fast -q / fast / hc, OpenZL typed + serial profiles at levels 6 and 9), plus
  research policies 'tfcli -p CANDIDATE -T1 SRC DST' / 'tfcli -d -T1 SRC DST' (research/codec/P2-E2/tfcli.c).
Timing, affinity, rlimits, byte-exact verification, determinism checks and the cost formulas are N1's code.

usage: run_e12.py prepare R3.zip OUT            (same as N1)
       run_e12.py run PREPARED OUT --zstd Z --openzl O --kolmo K --tpcli T [--seed S]
       run_e3.py confirm FIRST SECOND
"""
import argparse, importlib.util, json, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('n1baselines', HERE.parent / 'N1-baselines' / 'baselines.py')
n1 = importlib.util.module_from_spec(spec); sys.modules['n1baselines'] = n1; spec.loader.exec_module(n1)

RESEARCH = [  # (policy id, candidate) -- frozen before any P2-E12 measurement; encode tpcli, decode production kolmo
    ('tp_phuf', 'tp_phuf'), ('tp_phuf_pg16', 'tp_phuf_pg16'), ('tp_prod3', 'tp_prod3'), ('tp_table3', 'tp_table3'),
]
_n1_policies, _n1_command = n1.policies, n1.command


def policies(profile, tools):
    res = _n1_policies(profile, {k: v for k, v in tools.items() if k != 'tpcli'})
    for pid, cand in RESEARCH:
        res.append(dict(id=pid, tool='tpcli', research=True, mode_family='speed', decoder='production kolmo CLI',
                        enc=[tools['tpcli'], '-p', cand, '-T1'], dec=[tools['kolmo'], '-d', '-T1']))
    return res


def command(policy, operation, src, dst):
    if policy['tool'] == 'tpcli':
        return list(policy['enc' if operation == 'encode' else 'dec']) + [str(src), str(dst)]
    return _n1_command(policy, operation, src, dst)


n1.policies, n1.command = policies, command


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest='cmd', required=True)
    a = sub.add_parser('prepare'); a.add_argument('archive', type=Path); a.add_argument('output', type=Path)
    r = sub.add_parser('run'); r.add_argument('prepared', type=Path); r.add_argument('output', type=Path)
    for t in ('zstd', 'openzl', 'kolmo', 'tpcli'): r.add_argument('--' + t, required=True)
    r.add_argument('--seed', type=int, default=20261112)
    c = sub.add_parser('confirm'); c.add_argument('first', type=Path); c.add_argument('second', type=Path)
    x = p.parse_args()
    if x.cmd == 'prepare':
        n1.prepare(x.archive, x.output)
    elif x.cmd == 'run':
        n1.run(x.prepared, x.output, dict(zstd=x.zstd, openzl=x.openzl, kolmo=x.kolmo, tpcli=x.tpcli), seed=x.seed)
    else:
        print(json.dumps(n1.confirm(x.first, x.second), indent=2))


if __name__ == '__main__':
    main()
