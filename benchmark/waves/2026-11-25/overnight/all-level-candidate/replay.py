"""Compare two built Kolmo binaries in randomized same-host A/A/B trials."""
import argparse
import csv
import hashlib
import json
import os
import platform
import re
import random
import subprocess
import tempfile
import time
from pathlib import Path

SOURCES = [
    ('text__ld30', 16384, 'text16k'),
    ('code__py-stdlib', 65536, 'python64k'),
    ('csv__gaia', 262144, 'gaia256k'),
    ('audio__esc50-5-103415', None, 'esc50-whole'),
]
LEVELS = [str(i) for i in range(1, 10)] + ['ultra']
ARMS = ['main', 'main-AA', 'candidate']
SEED = 251112770

def digest(data):
    return hashlib.sha256(data).hexdigest()

def run(args):
    args = list(map(str, args))
    start = time.perf_counter_ns()
    pid = os.posix_spawn(args[0], args, os.environ)
    _, status, usage = os.wait4(pid, 0)
    elapsed = time.perf_counter_ns() - start
    if status:
        raise RuntimeError((args, status))
    return elapsed, usage.ru_utime + usage.ru_stime, usage.ru_maxrss

def main():
    parser = argparse.ArgumentParser()
    for name in ('before', 'after', 'corpus', 'out'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--rounds', type=int, default=3)
    parser.add_argument('--candidate-sha', default='ac10ce80048ab0664074f7ba031634055f4d2225')
    args = parser.parse_args()
    if not re.fullmatch(r'[0-9a-f]{40}', args.candidate_sha):
        parser.error('candidate-sha requires the complete source revision')
    args.out.mkdir(parents=True, exist_ok=True)
    bins = {'main': args.before.resolve(), 'main-AA': args.before.resolve(), 'candidate': args.after.resolve()}
    source = {}
    provenance = {}
    for filename, limit, label in SOURCES:
        original = (args.corpus/filename).read_bytes()
        source[label] = original[:limit]
        provenance[label] = {'file': filename, 'source_sha256':digest(original), 'offset':0,
                             'bytes':len(source[label]), 'original_bytes':len(original),
                             'input_sha256':digest(source[label])}
    manifest = {'revisions': {'main':'a1d39559686a9ec17c8614b9154a64d7b5b98672','candidate':args.candidate_sha},'warmup_cross_read':'all candidate warmup frames decoded using main, outside timing', 'sources':provenance, 'binaries':{k:digest(p.read_bytes()) for k,p in bins.items()},
                'machine':platform.platform(), 'compiler':subprocess.check_output(['cc','--version'],text=True).strip(),
                'seed': SEED, 'rounds':args.rounds, 'levels':LEVELS, 'requested_threads':[1,4],
                'effective_worker_count':'not instrumented', 'cache':'warm-input, whole-file process launch included',
                'arm_order':'independently seeded permutations within shuffled cells per pass',
                'pass_0':'warmup, retained and excluded from scores', 'rss_unit':'Darwin bytes'}
    (args.out/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    rng = random.Random(SEED)
    with tempfile.TemporaryDirectory(dir=args.out) as directory, (args.out/'rounds.csv').open('x',newline='',buffering=1) as file:
        tmp = Path(directory)
        for label,data in source.items():
            (tmp/label).write_bytes(data)
        fields = ['input','level','threads','rep','order','arm','input_sha256','bytes_in','bytes_out','frame_sha256','restored_sha256','encode_ns','decode_ns','encode_cpu_s','decode_cpu_s','encode_rss_B','decode_rss_B','load1']
        writer = csv.DictWriter(file,fieldnames=fields)
        writer.writeheader()
        frame = tmp/'tmp.kolmo'
        restored = tmp/'recovered'
        for rep in range(args.rounds+1):
            cells=[(s,l,t) for s in source for l in LEVELS for t in (1,4)]
            rng.shuffle(cells)
            for sample,level,threads in cells:
                for order,arm in enumerate(rng.sample(ARMS,3)):
                    while (args.out/'pause').exists():
                        time.sleep(1)
                    load = os.getloadavg()[0]
                    enc = run([bins[arm], '--level',level,'-T',threads,'-q','-f',tmp/sample,'-o',frame])
                    dec = run([bins[arm],'-d','-T',threads,'-q','-f',frame,'-o',restored])
                    received=restored.read_bytes()
                    if received != source[sample]:
                        raise RuntimeError(f'bad restore {sample} {level} {arm} {rep}')
                    if rep==0 and arm=='candidate':
                        restored.unlink()
                        run([bins['main'],'-d','-T',threads,'-q','-f',frame,'-o',restored])
                        if restored.read_bytes()!=source[sample]:
                            raise RuntimeError(f'bad stock read {sample} {level} {threads}')
                    frame_data=frame.read_bytes()
                    values = [sample,level,threads,rep,order,arm,provenance[sample]['input_sha256'],
                              len(received),len(frame_data),digest(frame_data),digest(received),enc[0],dec[0],
                              enc[1],dec[1],enc[2],dec[2],load]
                    writer.writerow(dict(zip(fields,values)))
            print('pass',rep,'complete',flush=True)
if __name__=='__main__':
    main()
