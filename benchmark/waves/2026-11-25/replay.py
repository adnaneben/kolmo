"""Sequential randomized comparison with hash validation and macOS RSS receipts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import tempfile
import time

ROOT = Path.cwd()

def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for data in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(data)
    return digest.hexdigest()

def run(argv):
    start = time.perf_counter()
    pid = os.posix_spawn(str(argv[0]), [str(a) for a in argv], os.environ)
    _, status, usage = os.wait4(pid, 0)
    if status:
        raise RuntimeError(f'{argv} exited {os.waitstatus_to_exitcode(status)}')
    return {'wall_s': time.perf_counter() - start,
            'cpu_s': usage.ru_utime + usage.ru_stime, 'rss_bytes': usage.ru_maxrss * (1024 if os.uname().sysname == 'Linux' else 1)}

def make_cases(evidence):
    corpus = evidence/'impact/corpus/holdout'
    panel = ['text__reymont', 'csv__owid-co2', 'log__bgl', 'weights__smollm2-bf16',
             'code__jxl-cc', 'img16__mr', 'exe__wasmtime-arm64', 'audio__esc50-3-100018']
    cases = []
    for name in panel:
        path = corpus/name
        domain = name.split('__')[0]
        cases.append((path, domain, [str(x) for x in range(1,10)] + ['ultra'], 1, 3))
    holdout = evidence/'corpus-holdout'
    for file, domain, lvs, threads in [
        ('llm_bf16_47mb.safetensors','weights',['3','9'],1),
        ('llm_bf16_47mb.safetensors','weights',['9'],10),
        ('SRR2584863.fastq','genomics',['3','ultra'],10),
        ('esc50-2-100648-A-43.wav','audio',['5','9'],1),
    ]:
        cases.append((holdout/file,domain,lvs,threads,3))
    cases.append((evidence/'mkt/data/silesia.tar','mixed',['1','3','9'],1,3))
    return cases

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--arms', required=True, nargs='+', help='name=/absolute/path')
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--evidence', required=True, type=Path)
    parser.add_argument('--fast-recheck', action='store_true')
    args=parser.parse_args()
    arms=dict(text.split('=',1) for text in args.arms)
    assert len(arms)>=2
    cases=make_cases(args.evidence)
    if args.fast_recheck:
        controls = {'log__bgl', 'text__reymont', 'code__jxl-cc'}
        cases = [(path, domain, ['1'], 1, 15) for path, domain, _, _, _ in cases if path.name in controls]
    signatures={path:sha(path) for path,*_ in cases}
    binaries={n:sha(Path(path)) for n,path in arms.items()}
    meta=args.out.with_suffix('.manifest.json')
    identity={'binaries':binaries,'inputs':{p.name:signature for p,signature in signatures.items()},
              'compiler':'Apple Clang 21.0.0 -O3 -mcpu=native -ffp-contract=off',
              'bench_sha256':sha(Path(__file__).resolve()), 'fast_recheck':args.fast_recheck}
    if meta.exists():
        assert json.loads(meta.read_text())==identity, 'run manifest changed'
    else:
        meta.write_text(json.dumps(identity,indent=2)+'\n')
    observed={}
    if args.out.exists():
        for line in args.out.read_text().splitlines():
            row=json.loads(line)
            observed[row['input'],row['level'],row['threads'],row['arm'],row['repeat']]=row['frame_sha256']
    rng=random.Random(8841221)
    with tempfile.TemporaryDirectory(dir=ROOT) as directory, args.out.open('a',buffering=1) as sink:
        tmp=Path(directory)
        for path,domain,levels,threads,reps in cases:
            for level in levels:
                for rep in range(reps):
                    for variant in rng.sample(list(arms),len(arms)):
                        key=(path.name,level,threads,variant,rep)
                        if key in observed:
                            continue
                        binary=arms[variant]
                        frame, out = tmp/'out.kolmo', tmp/'original'
                        frame.unlink(missing_ok=True); out.unlink(missing_ok=True)
                        load=os.getloadavg()[0]
                        enc=run([binary,'--level',level,'-T',threads,'-q','-f',path,'-o',frame])
                        dec=run([binary,'-d','-T',threads,'-q','-f',frame,'-o',out])
                        if sha(out)!=signatures[path]:
                            raise RuntimeError(f'lossless mismatch: {key}')
                        row={'input':path.name,'domain':domain,'raw_bytes':path.stat().st_size,
                             'input_sha256':signatures[path],'frame_sha256':sha(frame),
                             'stored_bytes':frame.stat().st_size,'level':level,'threads':threads,
                             'arm':variant,'repeat':rep,'enc':enc,'dec':dec,'load1':load}
                        print(json.dumps(row),file=sink)
                print(path.name,level,threads,flush=True)
if __name__=='__main__':
    main()
