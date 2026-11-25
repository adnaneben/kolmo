"""Paired end-to-end v12 measurements with explicit encoder-only overrides."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import tempfile
import time

def sha(path):
    with path.open('rb') as source:
        digest = hashlib.sha256()
        for data in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(data)
        return digest.hexdigest()

def run(binary, args, env):
    start = time.perf_counter()
    pid = os.posix_spawn(str(binary), [str(binary), *map(str, args)], env)
    _, status, resources = os.wait4(pid, 0)
    if status:
        raise RuntimeError(f'{binary} {args} exited {os.waitstatus_to_exitcode(status)}')
    return dict(wall_s=time.perf_counter()-start, cpu_s=resources.ru_utime+resources.ru_stime,
                rss_bytes=resources.ru_maxrss * (1024 if os.uname().sysname == 'Linux' else 1))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--wave2', type=Path, required=True)
    parser.add_argument('--decoder', type=Path, required=True)
    parser.add_argument('--weights', type=Path, nargs='+', required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--threads', type=int, nargs='+', default=[1])
    parser.add_argument('--repeats', type=int, default=7)
    args = parser.parse_args()
    env = {k:v for k,v in os.environ.items() if not k.startswith('KOLMO_')}
    enabled = dict(env, KOLMO_TENSOR_RAW_RETRY='1')
    inputs = {path:sha(path) for path in args.weights}
    manifest = dict(inputs={str(path.name):digest for path,digest in inputs.items()},
                    binary=sha(args.wave2), stock_decoder=sha(args.decoder),
                    threads=args.threads, repeats=args.repeats)
    args.out.with_suffix('.manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    rng = random.Random(179331)
    with tempfile.TemporaryDirectory(dir=args.out.parent) as scratch, args.out.open('x',buffering=1) as records:
        tmp = Path(scratch)
        frame, output = tmp/'trial.kolmo', tmp/'trial.raw'
        for path, input_sha in inputs.items():
            for threads in args.threads:
                for rep in range(args.repeats):
                    for name, enc_env in rng.sample([('wave2-default',env),('wave2-tensor-opt-in',enabled)],2):
                        frame.unlink(missing_ok=True)
                        output.unlink(missing_ok=True)
                        enc = run(args.wave2, ['--level','9','-T',threads,'-q','-f',path,'-o',frame],enc_env)
                        dec = run(args.decoder,['-d','-T',threads,'-q','-f',frame,'-o',output],env)
                        if sha(output) != input_sha:
                            raise RuntimeError(f'round trip failed: {path.name} {threads} {name}')
                        print(json.dumps(dict(input=path.name,input_sha256=input_sha,domain='weights',raw_bytes=path.stat().st_size,
                                              frame_sha256=sha(frame),stored_bytes=frame.stat().st_size,level='9',
                                              threads=threads,arm=name,repeat=rep,enc=enc,dec=dec)),file=records)
if __name__ == '__main__':
    main()
