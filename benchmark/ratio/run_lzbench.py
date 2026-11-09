#!/usr/bin/env python3
"""Run a declared matrix through real lzbench, with shuffled codec/file order.

Each row has three timed compression/decompression samples, summarized by median.
No custom timing substitute or GPU setup exclusions are used in this CPU matrix.
"""
import argparse
import csv
import hashlib
import io
import json
import pathlib
import random
import re
import subprocess
import sys
import time

root = pathlib.Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('lzbench')
parser.add_argument('output')
parser.add_argument('--suite', choices=['development', 'validation', 'board', 'scaling', 'ratio-leaders'], required=True)
parser.add_argument('--memory', action='store_true', help='macOS: collect per-process peak RSS via /usr/bin/time -l')
parser.add_argument('--workers', type=int, choices=[1,10], default=10, help='external workers for the equal-block scaling suite')
args = parser.parse_args()
assert not args.memory or sys.platform=='darwin', '--memory currently uses macOS time -l'
out = pathlib.Path(args.output)
assert not out.exists(), 'Keep earlier evidence: use a new output name'
if args.suite == 'development':
    files = sorted((root/'benchmark/audit/data/silesia').glob('*'))
    files += sorted((root/'benchmark/next/data/canterbury').glob('*'))
    files += [root/'benchmark/audit/data/owid-co2-data.csv',root/'benchmark/next/data/mitdb-100.dat']
    codecs = ['kbc','krc','kolmo,1','lz4','zstd,3','zstd,9','zstd,19','brotli,8','bsc1','ppmd8,6']
elif args.suite == 'validation':
    m = json.loads((root/'benchmark/ratio/validation-inputs.json').read_text())
    for item in m['files']:
        assert hashlib.sha256((root/item['path']).read_bytes()).hexdigest() == item['sha256']
    files = [root/p for p in m['standard_calgary_files']]
    codecs = ['kbc','krc','kolmo,1','lz4','zstd,3','zstd,19','brotli,8','bsc1','ppmd8,6']
elif args.suite == 'scaling':
    files = [root/'benchmark/audit/data/silesia/mozilla']
    codecs = ['kbc','krc','lz4','zstd,3']
else:
    item = json.loads((root/'benchmark/ratio/board-input.json').read_text())
    files = [root/item['path']]
    assert hashlib.sha256(files[0].read_bytes()).hexdigest() == item['sha256']
    codecs = ['kbc','krc','kolmo,1','lz4','zstd,3','zstd,19','brotli,8','bsc1','ppmd8,6']

if args.suite == 'ratio-leaders':
    codecs = ['zpaq,5']
repeats = 1 if args.suite == 'ratio-leaders' else 3

jobs = [(p,c) for p in files for c in codecs]
random.Random(20261108).shuffle(jobs)
manifest = {'suite':args.suite,'seed':20261108,'repeats':repeats,'statistic':'single observation' if repeats==1 else 'median',
            'external_workers':args.workers if args.suite=='scaling' else 1,
            'external_block_bytes':1048576 if args.suite=='scaling' else 0,
            'lzbench_sha256':hashlib.sha256(pathlib.Path(args.lzbench).read_bytes()).hexdigest(),
            'inputs':[{'path':str(p.relative_to(root)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in files],
            'commands':[], 'compiler':'Apple clang 21; -mcpu=native; lzbench default -O3'}
writer = None
with out.open('w') as stream, out.with_suffix('.stderr').open('w') as errors:
    for i,(path,codec) in enumerate(jobs):
        command = [args.lzbench,'-e'+codec,'-I1','-T1','-t0,0',f'-i{repeats},{repeats}','-p3','-o4','-q',str(path.relative_to(root))]
        if args.suite == 'scaling':
            # Equal external blocks and worker count; every codec receives one-thread calls.
            command[3]=f'-T{args.workers}';command.insert(4,'-b1024')
        measured_command = ['/usr/bin/time','-l']+command if args.memory else command
        start = time.time()
        result = subprocess.run(measured_command,cwd=root,text=True,capture_output=True)
        errors.write(' '.join(command)+'\n'+result.stderr);errors.flush()
        assert result.returncode == 0, (command,result.stderr)
        rows = list(csv.DictReader(io.StringIO(result.stdout)))
        assert len(rows) == 1, (command,result.stdout)
        row = rows[0]
        assert int(row['Original size']) == path.stat().st_size and int(row['Compressed size'])>0
        assert float(row['Compression speed'])>0 and float(row['Decompression speed'])>0, row
        if writer is None:
            writer = csv.DictWriter(stream,fieldnames=list(row));writer.writeheader()
        writer.writerow(row);stream.flush()
        observation = {'argv':measured_command,'exit_code':result.returncode,'wall_seconds':time.time()-start}
        if args.memory:
            match = re.search(r'(\d+)\s+maximum resident set size',result.stderr)
            assert match, result.stderr
            observation['process_peak_rss_bytes']=int(match[1])
        manifest['commands'].append(observation)
        out.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
        print(f'{args.suite}: {i+1}/{len(jobs)} verified {codec} {path.name}',flush=True)
print('MATRIX PASSED',len(jobs),'configurations;',len(jobs)*repeats,'compression/decompression sample pairs')
