#!/usr/bin/env python3
"""P2-E29 (policies identical to P2-E28; preregistered FRESH audio): does a faster KG_INT inverse in the DECODER close the
speed-mode gap to zstd-3? Same research frames (p2e13 enc), decoded either by the production kolmo CLI or by the
research decoder kolmo-fastinv (gen_fastdec.py: pinned src with kg_decode's KG_INT inverse -> P2-E2 fi_inverse).
N1 baselines.py UNCHANGED (policies()/command() only).
usage: run_e28.py subset PREPARED OUT | run_e28.py run PREPARED OUT --zstd Z --openzl O --kolmo K --tb P --fastdec F [--seed S]
       run_e28.py confirm A B"""
import argparse, importlib.util, json, shutil, sys
from pathlib import Path
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('n1baselines', HERE.parent / 'N1-baselines' / 'baselines.py')
n1 = importlib.util.module_from_spec(spec); sys.modules['n1baselines'] = n1; spec.loader.exec_module(n1)
_n1_policies, _n1_command = n1.policies, n1.command
FRAMES = ['tp_phuf_pg16', 'tp_phuf', 'tb_lazy', 'tb_best', 'tp_prod3']
def policies(profile, tools):
    base = _n1_policies(profile, {k: v for k, v in tools.items() if k not in ('tb', 'fastdec')})
    res = [r for r in base if r['id'] in ('zstd_3', 'kolmo_fast')]
    for c in FRAMES:
        res.append(dict(id=c, tool='ext', research=True, enc=[tools['tb'], 'enc', c], dec=[tools['kolmo'], '-d', '-T1']))
        res.append(dict(id=c + '_fd', tool='ext', research=True, enc=[tools['tb'], 'enc', c], dec=[tools['fastdec'], '-d', '-T1']))
    res.append(dict(id='kolmo_fast_fd', tool='ext', research=True, enc=[tools['kolmo'], '-m', 'fast', '-T1'], dec=[tools['fastdec'], '-d', '-T1']))
    return res
def command(policy, operation, src, dst):
    if policy['tool'] == 'ext': return list(policy['enc' if operation == 'encode' else 'dec']) + [str(src), str(dst)]
    return _n1_command(policy, operation, src, dst)
n1.policies, n1.command = policies, command
def main():
    p = argparse.ArgumentParser(); sub = p.add_subparsers(dest='cmd', required=True)
    s = sub.add_parser('subset'); s.add_argument('prepared', type=Path); s.add_argument('output', type=Path)
    r = sub.add_parser('run'); r.add_argument('prepared', type=Path); r.add_argument('output', type=Path)
    for t in ('zstd', 'openzl', 'kolmo', 'tb', 'fastdec'): r.add_argument('--' + t, required=True)
    r.add_argument('--seed', type=int, default=20261112)
    c = sub.add_parser('confirm'); c.add_argument('first', type=Path); c.add_argument('second', type=Path)
    x = p.parse_args()
    if x.cmd == 'subset':
        m = json.loads((x.prepared / 'manifest.json').read_text()); rows = [r for r in m['inputs'] if r['view'] == 'wav']
        (x.output / 'inputs').mkdir(parents=True)
        for r in rows: shutil.copy2(x.prepared / r['file'], x.output / r['file'])
        m['inputs'] = rows; m['split'] = 'development (observed in P2-E20)'; (x.output / 'manifest.json').write_text(json.dumps(m, indent=1)); print(len(rows), 'WAV')
    elif x.cmd == 'run': n1.run(x.prepared, x.output, dict(zstd=x.zstd, openzl=x.openzl, kolmo=x.kolmo, tb=x.tb, fastdec=x.fastdec), seed=x.seed)
    else: print(json.dumps(n1.confirm(x.first, x.second), indent=2))
if __name__ == '__main__': main()
