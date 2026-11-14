#!/usr/bin/env python3
"""P2-E31 (policies identical to P2-E30; preregistered FRESH audio): audio levers.
Frames: tp_phuf_pg16 (reference, p2e13), ta_phuf (fast forward pass, same bytes), ta_rep (repetition-aware leaf
choice), ta_rep_fb (repetitive -> whole-input fallback). Each decoded by the production CLI and by the P2-E28 research
fast-inverse decoder (_fd). Rival zstd-3; production kolmo fast control. N1 baselines.py UNCHANGED.
usage: run_e30.py merge OUT DIR... | run_e30.py run PREPARED OUT --zstd Z --openzl O --kolmo K --tb P13 --ta P30 --fastdec F [--seed S]
       run_e30.py confirm A B"""
import argparse, importlib.util, json, shutil, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('n1baselines', HERE.parent / 'N1-baselines' / 'baselines.py')
n1 = importlib.util.module_from_spec(spec); sys.modules['n1baselines'] = n1; spec.loader.exec_module(n1)
_n1_policies, _n1_command = n1.policies, n1.command
def policies(profile, tools):
    base = _n1_policies(profile, {k: v for k, v in tools.items() if k not in ('tb', 'ta', 'fastdec')})
    res = [r for r in base if r['id'] in ('zstd_3', 'kolmo_fast')]
    frames = [('tp_phuf_pg16', tools['tb'])] + [(c, tools['ta']) for c in ('ta_phuf', 'ta_rep', 'ta_rep_fb')]
    for c, enc in frames:
        res.append(dict(id=c, tool='ext', research=True, enc=[enc, 'enc', c], dec=[tools['kolmo'], '-d', '-T1']))
        res.append(dict(id=c + '_fd', tool='ext', research=True, enc=[enc, 'enc', c], dec=[tools['fastdec'], '-d', '-T1']))
    return res
def command(policy, operation, src, dst):
    if policy['tool'] == 'ext': return list(policy['enc' if operation == 'encode' else 'dec']) + [str(src), str(dst)]
    return _n1_command(policy, operation, src, dst)
n1.policies, n1.command = policies, command
def main():
    p = argparse.ArgumentParser(); sub = p.add_subparsers(dest='cmd', required=True)
    g = sub.add_parser('merge'); g.add_argument('output', type=Path); g.add_argument('dirs', type=Path, nargs='+')
    r = sub.add_parser('run'); r.add_argument('prepared', type=Path); r.add_argument('output', type=Path)
    for t in ('zstd', 'openzl', 'kolmo', 'tb', 'ta', 'fastdec'): r.add_argument('--' + t, required=True)
    r.add_argument('--seed', type=int, default=20261112)
    c = sub.add_parser('confirm'); c.add_argument('first', type=Path); c.add_argument('second', type=Path)
    x = p.parse_args()
    if x.cmd == 'merge':
        rows = []; (x.output / 'inputs').mkdir(parents=True)
        for d in x.dirs:
            m = json.loads((d / 'manifest.json').read_text())
            for r in m['inputs']:
                if r['view'] != 'wav': continue
                shutil.copy2(d / r['file'], x.output / r['file']); rows.append(dict(r, origin='development_observed'))
        (x.output / 'manifest.json').write_text(json.dumps(dict(experiment='P2-E30', split='development', inputs=rows), indent=1)); print(len(rows), 'WAV')
    elif x.cmd == 'run': n1.run(x.prepared, x.output, dict(zstd=x.zstd, openzl=x.openzl, kolmo=x.kolmo, tb=x.tb, ta=x.ta, fastdec=x.fastdec), seed=x.seed)
    else: print(json.dumps(n1.confirm(x.first, x.second), indent=2))
if __name__ == '__main__': main()
