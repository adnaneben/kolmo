#!/usr/bin/env python3
"""N1-compatible experiment replacing nonexistent OpenZL 'numpy' profiles with
exact NPY-header-preserving numeric profiles. Complete CLI; development only.
"""
import argparse
import importlib.util
from pathlib import Path
import sys

HERE=Path(__file__).resolve().parent
N1=HERE.parent/'N1-baselines'/'baselines.py'
spec=importlib.util.spec_from_file_location('n1_npy_compat',N1)
n1=importlib.util.module_from_spec(spec)
sys.modules['n1_npy_compat']=n1
spec.loader.exec_module(n1)
_saved_policies,_saved_command=n1.policies,n1.command
ADAPTER=HERE/'npy_openzl.py'

def policies(profile,tools):
    rows=_saved_policies(profile,tools)
    if profile!='numpy': return rows
    rows=[r for r in rows if not r['id'].startswith('openzl_numpy_')]
    for level in (6,7,9):
        rows.append(dict(id=f'openzl_exact_npy_{level}',
          tool='openzl_exact_npy',profile='header_described_exact_int_payload',
          level=level,training_regime='untrained_cli_builtin_models',
          research=True,mode_family='archive',original_byte_boundary=True,
          enc=[sys.executable,str(ADAPTER),'compress'],
          dec=[sys.executable,str(ADAPTER),'decompress'],
          zli=tools['openzl']))
    return rows

def command(policy,operation,src,dst):
    if policy['tool']=='openzl_exact_npy':
        args=list(policy['enc' if operation=='encode' else 'dec'])
        return args+[str(src),str(dst),'--zli',policy['zli'],'--level',str(policy['level'])]
    return _saved_command(policy,operation,src,dst)

n1.policies,n1.command=policies,command

def main():
    p=argparse.ArgumentParser(description=__doc__)
    sub=p.add_subparsers(dest='action',required=True)
    a=sub.add_parser('prepare');a.add_argument('archive',type=Path);a.add_argument('output',type=Path)
    a=sub.add_parser('run');a.add_argument('prepared',type=Path);a.add_argument('output',type=Path)
    for t in ('zstd','openzl','kolmo'):a.add_argument('--'+t,required=True)
    a.add_argument('--seed',type=int,default=20261113)
    a=sub.add_parser('confirm');a.add_argument('first',type=Path);a.add_argument('second',type=Path)
    x=p.parse_args()
    if x.action=='prepare':n1.prepare(x.archive,x.output)
    elif x.action=='run':
        n1.run(x.prepared,x.output,
               {t:str(Path(getattr(x,t)).resolve()) for t in ('zstd','openzl','kolmo')},
               seed=x.seed)
    else:print(n1.confirm(x.first,x.second))

if __name__=='__main__':main()
