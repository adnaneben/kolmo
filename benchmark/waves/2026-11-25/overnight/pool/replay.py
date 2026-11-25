"""Replay two independently compiled macOS probes against a completed Kolmo default build."""
import argparse
import csv
import hashlib
import json
import platform
import random
import subprocess
import tempfile
from pathlib import Path
FIELDS = 'pair policy role stage operation input_bytes requested_threads frame_bytes elapsed_ns rss_before rss_after peak_before peak_after footprint_before footprint_after minor_before minor_after major_before major_after helper_count tls_count caller_capacity helper_capacity cpu_us pool_wakeups'.split()
def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    parser = argparse.ArgumentParser()
    for s in ['source','large','out']:
        parser.add_argument('--'+s,type=Path,required=True)
    parser.add_argument('--pairs',type=int,default=9)
    a=parser.parse_args()
    out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    src=a.source.resolve();lib=src/'build/libkolmo.a';arch=src/'build/libkolmoarch.a'
    small=src/'src/core/pool.c';large=a.large.resolve()
    state={'commit':subprocess.check_output(['git','-C',src,'rev-parse','HEAD'],text=True).strip(),
           'host':platform.platform(),'pairs':a.pairs,'seed':202611251,'compiler':subprocess.check_output(['cc','--version'],text=True).splitlines()[0],
           'level':3,'block_bytes':1048576,'inputs':{},'binaries':{'default_library':digest(lib),'aux_library':digest(arch)}}
    with tempfile.TemporaryDirectory(dir=out) as workdir:
        d=Path(workdir)
        bins={name:d/name for name in ['public','diagnostic']}
        flags=['cc','-O3','-mcpu=native','-ffp-contract=off','-I'+str(src/'src')]
        cfile=Path(__file__).with_name('probe.c')
        for k,exe in bins.items():
            diag=['-DDIAGNOSTIC'] if k=='diagnostic' else []
            subprocess.run(flags+diag+[cfile,arch if diag else lib,'-lpthread','-lm','-Wl,-dead_strip','-o',exe],check=True)
            state['binaries'][k]=digest(exe)
        paths=[small,large,d/'small.frame',d/'large.frame']
        subprocess.run([bins['public'],'0','prepare',*paths],check=True)
        for s,p,f in [('small',small,paths[2]),('large',large,paths[3])]:
            state['inputs'][s]={'filename':p.name,'source_bytes':p.stat().st_size,'source_sha256':digest(p),
                'frame_bytes':f.stat().st_size,'frame_sha256':digest(f),'frame_blocks':int.from_bytes(f.read_bytes()[20:24],'little')}
        rng=random.Random(state['seed'])
        for k,exe in bins.items():
            with (out/(k+'.csv')).open('x',newline='') as f:
                f.write(','.join(FIELDS)+'\n');f.flush()
                for pair in range(a.pairs if k=='public' else 1):
                    arms=rng.sample(['retain','release'],2)
                    for arm in arms:
                        subprocess.run([exe,str(pair),arm,*paths],check=True,stdout=f,timeout=60)
            print(k,'completed',flush=True)
        (out/'manifest.json').write_text(json.dumps(state,indent=2)+'\n')
if __name__=='__main__':
    main()
