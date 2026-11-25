"""Full-file product comparison. Use a fresh output directory and an exact CLI."""
import argparse
import hashlib
import json
from pathlib import Path
import os
import platform
import resource
import shutil
import subprocess
import time

def sha(path):
    with path.open('rb') as stream:
        digest = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1 << 20), b''):
            digest.update(chunk)
        return digest.hexdigest()

def record(path, obj):
    tmp = path.with_suffix('.tmp')
    tmp.write_text(json.dumps(obj, indent=2) + '\n')
    tmp.replace(path)

def run():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('kolmo', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    data, kolmo, folder = (p.resolve() for p in (args.input, args.kolmo, args.output))
    folder.mkdir(exist_ok=False, parents=True)
    if data.stat().st_size != 100_000_000:
        raise ValueError('expected whole enwik8')
    digest = sha(data)
    if digest != '2b49720ec4d78c3c9fabaee6e4179a5e997302b3a70029f30f2d582218c024a8':
        raise ValueError('unexpected enwik8 content')
    archive = folder/'baseline.kolmo'
    rival = folder/'enwik8.paq8px217'
    restored = folder/'restored'
    restored.mkdir()
    tools = {n: shutil.which(n) for n in ('xz','zstd','paq8px')}
    tasks = [
        ('zstd-19', [tools['zstd'],'-q','-T1','-19','-c',str(data)],
         [tools['zstd'],'-q','-d','-c'], folder/'baseline.zst'),
        ('xz-9', [tools['xz'],'--threads=1','-9','-c',str(data)],
         [tools['xz'],'-d','-c'], folder/'baseline.xz'),
        ('kolmo-ultra', [str(kolmo),'--ultra','-T1','-c',str(data)],
         [str(kolmo),'-T1','-dc'], archive),
        ('paq8px-8L', [tools['paq8px'],'-8L',str(data),str(rival)],
         [tools['paq8px'],'-d',str(rival),str(restored)], rival)]
    report = {'source_sha256':digest, 'input_bytes':data.stat().st_size,
              'platform':platform.platform(), 'kolmo_sha256':sha(kolmo),
              'tool_sha256':{k:sha(Path(v)) for k,v in tools.items()}, 'pid':os.getpid(), 'results':[]}
    state = folder/'status.json'
    for name, enc, dec, frame in tasks:
        is_paq = name.startswith('paq')
        raw = restored/rival.stem if is_paq else restored/(name+'.raw')
        for phase, command, product in [('encode',enc,frame),('decode',dec if is_paq else [*dec,str(frame)],raw)]:
            report['current'] = [name, phase]
            record(state,report)
            start = time.perf_counter()
            usage = resource.getrusage(resource.RUSAGE_CHILDREN)
            with (folder/(name+'-'+phase+'.log')).open('wb') as log:
                if is_paq:
                    subprocess.run(command, check=True, cwd=folder, stdout=log, stderr=log)
                else:
                    with product.open('xb') as output:
                        subprocess.run(command, check=True, stdout=output, stderr=log)
            elapsed = time.perf_counter()-start
            end_usage = resource.getrusage(resource.RUSAGE_CHILDREN)
            cpu = (end_usage.ru_utime - usage.ru_utime) + (end_usage.ru_stime - usage.ru_stime)
            if phase=='encode':
                entry = {'tool':name,'encode_seconds':elapsed,'stored_bytes':frame.stat().st_size,'encode_argv':command, 'encode_cpu_seconds':cpu, 'frame_sha256':sha(frame)}
            else:
                if sha(raw)!=digest:
                    raise ValueError('recovered file differs')
                entry.update(decode_seconds=elapsed, decode_cpu_seconds=cpu, decode_argv=command, recovered_sha256=digest,
                             combined_MB_s=data.stat().st_size/(entry['encode_seconds']+elapsed)/1e6)
                report['results'].append(entry)
                raw.unlink()
        record(state,report)
    report['current'] = 'complete'
    record(state,report)

if __name__=='__main__':
    run()
