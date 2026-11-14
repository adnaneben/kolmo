#!/usr/bin/env python3
"""P2-E10: the frozen type-first policies on a broader development cohort (E02E/E02F/E02G derived NPY + whole original
FSDD WAVs and a Backblaze daily CSV) on N1's complete-CLI boundary. OpenZL typed le-* profiles are applied to whole
files (prepare_e10.py sets the profile per input). Development data.

Imports research/codec/N1-baselines/baselines.py UNCHANGED and replaces only its module-level policies() and
command() so that the same run() measures, on the same prepared inputs and pinned rival binaries:
  N1's own policies (zstd-3, kolmo fast -q / fast / hc, OpenZL typed + serial profiles at levels 6 and 9), plus
  research policies 'tfcli -p CANDIDATE -T1 SRC DST' / 'tfcli -d -T1 SRC DST' (research/codec/P2-E2/tfcli.c).
Timing, affinity, rlimits, byte-exact verification, determinism checks and the cost formulas are N1's code.

usage: run_e10.py prepare R3.zip OUT            (same as N1)
       run_e10.py run PREPARED OUT --zstd Z --openzl O --kolmo K --tfcli T --mcli M [--seed S]
       run_e3.py confirm FIRST SECOND
"""
import argparse, importlib.util, json, sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('n1baselines', HERE.parent / 'N1-baselines' / 'baselines.py')
n1 = importlib.util.module_from_spec(spec); sys.modules['n1baselines'] = n1; spec.loader.exec_module(n1)

RESEARCH = [  # (policy id, tool, candidate, mode family) -- frozen before any P2-E10 measurement
    ('tf_planes_huf', 'tfcli', 'tf_planes_huf', 'speed'), ('tf_planes_huf_pg16', 'tfcli', 'tf_planes_huf_pg16', 'speed'),
    ('tf_prod19', 'tfcli', 'tf_prod19', 'archive'), ('tf_planes_ent19', 'tfcli', 'tf_planes_ent19', 'archive'),
    ('mcsv_planes_huf', 'mcli', 'mcsv_planes_huf', 'speed'), ('mcsv_prod19', 'mcli', 'mcsv_prod19', 'archive'),
]
_n1_policies, _n1_command = n1.policies, n1.command


def policies(profile, tools):
    res = _n1_policies(profile, {k: v for k, v in tools.items() if k not in ('tfcli', 'mcli')})
    for pid, tool, cand, fam in RESEARCH:
        res.append(dict(id=pid, tool=tool, research=True, mode_family=fam,
                        enc=[tools[tool], '-p', cand, '-T1'], dec=[tools[tool], '-d', '-T1']))
    res.append(dict(id='kolmo_max', tool='kolmo', enc=[tools['kolmo'], '-m', 'max', '-T1'], dec=[tools['kolmo'], '-d', '-T1']))
    seen, uniq = set(), []                       # profile 'serial' would duplicate N1's serial controls
    for r in res:
        if r['id'].startswith('openzl_') and r['id'].endswith('_6'): continue   # level 9 only: bound the runtime
        if r['id'] not in seen: seen.add(r['id']); uniq.append(r)
    return uniq


def command(policy, operation, src, dst):
    if policy['tool'] in ('tfcli', 'mcli'):
        return list(policy['enc' if operation == 'encode' else 'dec']) + [str(src), str(dst)]
    return _n1_command(policy, operation, src, dst)


n1.policies, n1.command = policies, command


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest='cmd', required=True)
    a = sub.add_parser('prepare'); a.add_argument('archive', type=Path); a.add_argument('output', type=Path)
    r = sub.add_parser('run'); r.add_argument('prepared', type=Path); r.add_argument('output', type=Path)
    for t in ('zstd', 'openzl', 'kolmo', 'tfcli', 'mcli'): r.add_argument('--' + t, required=True)
    r.add_argument('--seed', type=int, default=20261112)
    c = sub.add_parser('confirm'); c.add_argument('first', type=Path); c.add_argument('second', type=Path)
    x = p.parse_args()
    if x.cmd == 'prepare':
        n1.prepare(x.archive, x.output)
    elif x.cmd == 'run':
        n1.run(x.prepared, x.output, dict(zstd=x.zstd, openzl=x.openzl, kolmo=x.kolmo, tfcli=x.tfcli, mcli=x.mcli), seed=x.seed)
    else:
        print(json.dumps(n1.confirm(x.first, x.second), indent=2))


if __name__ == '__main__':
    main()
