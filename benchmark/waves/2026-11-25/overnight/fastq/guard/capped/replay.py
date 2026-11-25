"""Recheck stock and optional encoder on the previously pinned opaque file windows."""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import tempfile
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def timed(command, env):
    start = time.perf_counter_ns()
    subprocess.run(command, check=True, env=env, stdout=subprocess.DEVNULL)
    return time.perf_counter_ns() - start


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for arg in ('stock', 'candidate', 'inputs', 'out', 'reference'):
        ap.add_argument('--' + arg, type=Path, required=True)
    ap.add_argument('--candidate-sha', required=True)
    ap.add_argument('--rounds', type=int, default=2)
    args = ap.parse_args()
    stock, candidate = args.stock.resolve(), args.candidate.resolve()
    args.out.mkdir(parents=True, exist_ok=True)
    with args.reference.open() as src:
        refs = {(r['accession'], r['arm']): r for r in csv.DictReader(src)}
    names = sorted({name for name, arm in refs})
    assert len(names) == 2
    env = dict(os.environ)
    env.pop('KOLMO_BENCH_READ_CUTS', None)
    recorded = []
    fields = ['accession','round','order','arm','input_bytes','input_sha256','frame_bytes',
              'frame_sha256','encode_ns','stock_decode_ns','restored_sha256']
    with (args.out/'trials.csv').open('w', newline='') as dest, tempfile.TemporaryDirectory() as tmp:
        writer = csv.DictWriter(dest, fields, lineterminator='\n')
        writer.writeheader()
        frame, restored = Path(tmp)/'frame', Path(tmp)/'restored'
        for rnd in range(args.rounds):
            for index, name in enumerate(names):
                source = (args.inputs/(name + '-capped-large.fastq')).resolve()
                data_digest = digest(source)
                arms = ('aligned','fixed') if (rnd + index) % 2 == 0 else ('fixed','aligned')
                for order, arm in enumerate(arms):
                    reference = refs[name,arm]
                    assert data_digest == reference['sha256'] and source.stat().st_size == int(reference['raw_bytes'])
                    tool = candidate if arm == 'aligned' else stock
                    settings = dict(env, KOLMO_BENCH_READ_CUTS='1') if arm == 'aligned' else env
                    enc = timed([tool, '-q', '-T1', '-U', '--memlimit', '256M', '-o', frame, source], settings)
                    dec = timed([stock, '-q', '-T1', '-d', '-o', restored, frame], env)
                    assert digest(restored) == data_digest
                    assert digest(frame) == reference['stored_sha256'] and frame.stat().st_size == int(reference['stored_bytes'])
                    row = dict(zip(fields, (name, rnd, order, arm, source.stat().st_size, data_digest,
                                            frame.stat().st_size, digest(frame), enc, dec, data_digest)))
                    recorded.append(row)
                    writer.writerow(row)
                    dest.flush()
                    print(name, rnd, arm, row['frame_bytes'], enc, dec, flush=True)
                    frame.unlink()
                    restored.unlink()
    scores = []
    for (name, arm) in sorted(refs):
        rows = [r for r in recorded if r['accession'] == name and r['arm'] == arm]
        assert len(rows) == args.rounds
        n = rows[0]['input_bytes']
        median = statistics.median
        enc = median(r['encode_ns'] for r in rows)
        dec = median(r['stock_decode_ns'] for r in rows)
        both = median(r['encode_ns'] + r['stock_decode_ns'] for r in rows)
        scores.append(dict(accession=name, arm=arm, input_bytes=n, archive_bytes=rows[0]['frame_bytes'],
                           encode_ms=enc/1e6, stock_decode_ms=dec/1e6, encode_MBps=n*1000/enc,
                           stock_decode_MBps=n*1000/dec, combined_MBps=n*1000/both))
    with (args.out/'scores.csv').open('w', newline='') as dest:
        writer = csv.DictWriter(dest, scores[0].keys(), lineterminator='\n')
        writer.writeheader()
        writer.writerows(scores)
    manifest = dict(candidate_commit=args.candidate_sha, stock_commit='258ca7b60db43cfdfba308c703b2e576cb628fa1',
                    candidate_binary_sha256=digest(candidate), stock_binary_sha256=digest(stock),
                    reference_csv_sha256=digest(args.reference), rounds_per_arm=args.rounds,
                    host=platform.platform(), stock_build='make build/kolmo',
                    candidate_build='make DEFS=-DKOLMO_FASTQ_RECORD_BLOCKS build/kolmo',
                    encoder_args=['-q','-T1','-U','--memlimit','256M'], stock_decode_args=['-q','-T1','-d'],
                    matched_reference_archive_digests=True, restores=len(recorded))
    (args.out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')


if __name__ == '__main__':
    main()
