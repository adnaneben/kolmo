#!/usr/bin/env python3
"""Matched independent compressed frames inside deterministic ZIP_STORED archives."""
import argparse
import csv
import email
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import random
import subprocess
import tempfile
from time import perf_counter_ns
from zipfile import ZIP_STORED, ZipFile, ZipInfo

TRAIN = {'networkx': '3.7', 'pillow': '12.1.1', 'pytest': '9.2.0'}
TEST = {'numpy': '2.5.5', 'pydantic': '2.12.5', 'sympy': '1.14.0'}
SKIP = {'tests', 'test', 'vendored', '_vendor', 'vendor', 'extern', '_pytester', 'third_party'}
DICT = '_dictionary.zstd'
ARMS = ['kolmo', 'zstd', 'zstd-dict']
SEED = 20261125


def digest(data):
    return hashlib.sha256(data).hexdigest()


def write_csv(path, rows):
    with path.open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)


def choose(cache):
    wheels = {}
    for info in sorted(cache.glob('*/*.dist-info')):
        meta = email.message_from_bytes((info / 'METADATA').read_bytes())
        name = meta['Name'].lower()
        if (TRAIN | TEST).get(name) == meta['Version']:
            wheels[name] = info.parent
    assert set(wheels) == set(TRAIN) | set(TEST), 'Expected exact wheel versions in local uv cache'
    chosen, paths, seen = [], {}, set()
    for role, group, cap in [('train', TRAIN, 32), ('holdout', TEST, 12)]:
        for name, version in group.items():
            root = wheels[name] / {'pillow': 'PIL', 'pytest': '_pytest'}.get(name, name)
            files = sorted(root.rglob('*.py'), key=lambda p: digest(str(p.relative_to(root)).encode()))
            for cohort, lo, hi in [('tiny', 64, 2048), ('mid', 4096, 32768)]:
                count = 0
                for p in files:
                    rel = p.relative_to(root)
                    if set(rel.parts) & SKIP or not lo <= p.stat().st_size <= hi:
                        continue
                    b = p.read_bytes()
                    sha = digest(b)
                    if sha in seen:
                        continue
                    seen.add(sha)
                    key = name + '/' + rel.as_posix()
                    paths[key] = p
                    chosen.append(dict(role=role, project=name, version=version, cohort=cohort,
                                       path=key, input_bytes=len(b), input_sha256=sha))
                    count += 1
                    if count == cap:
                        break
                assert role == 'train' or count == cap
    return chosen, paths


def entry(zf, name, data):
    info = ZipInfo(name, (2000, 1, 1, 0, 0, 0))
    info.compress_type = ZIP_STORED
    info.create_system = 3
    info.external_attr = 0o100644 << 16
    zf.writestr(info, data)


def cmd(*args, data=None):
    return subprocess.run(args, input=data, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True).stdout


def encode(arm, path, opts, dictionary):
    if arm == 'kolmo':
        return cmd(opts.kolmo, '-3', '-T1', '-q', '-c', path)
    flags = ['-D', dictionary] if arm == 'zstd-dict' else []
    return cmd(opts.zstd, '-3', '--single-thread', '-q', '-c', *flags, path)


def decode(arm, frame, opts, dictionary, stock=False):
    if arm == 'kolmo':
        return cmd(opts.main if stock else opts.kolmo, '-d', '-T1', '-q', '-c', data=frame)
    flags = ['-D', dictionary] if arm == 'zstd-dict' else []
    return cmd(opts.zstd, '-d', '--single-thread', '-q', '-c', *flags, data=frame)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for flag in ['cache', 'kolmo', 'main', 'zstd', 'out']:
        parser.add_argument('--'+flag, type=Path, required=True)
    opts = parser.parse_args()
    opts.out.mkdir(exist_ok=True, parents=True)
    for k in list(os.environ):
        if k.startswith('KOLMO_') or k in ('ZSTD_CLEVEL','ZSTD_NBTHREADS'):
            del os.environ[k]
    selected, paths = choose(opts.cache)
    write_csv(opts.out/'inputs.csv', selected)
    train = [paths[r['path']] for r in selected if r['role'] == 'train']
    holdout = [r for r in selected if r['role'] == 'holdout']
    times, frames, identities = [], {}, []
    rng = random.Random(SEED)
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        dictionary = work/'train.dict'
        flags = ['--train-fastcover=k=50,d=8,f=20,steps=4,split=80,accel=1',
                 '--maxdict=8192', '--dictID=1125003', '-T1', '-q']
        cmd(opts.zstd, *flags, *train, '-o', dictionary)
        dict_data = dictionary.read_bytes()
        extracted = work/'embedded.dict'
        manifest = dict(train=TRAIN, holdout=TEST, seed=SEED, warmups=1, scored=5,
                        train_flags=flags, dict_sha256=digest(dict_data), dict_bytes=len(dict_data),
                        samples=len(train), train_bytes=sum(p.stat().st_size for p in train),
                        system=platform.platform(), build={k:digest(getattr(opts,k).read_bytes()) for k in ['kolmo','main','zstd']},
                        candidate_sha='8d23095b2f09d5c9e3c3425123bea63641ca5d71', main_sha='a1d39559686a9ec17c8614b9154a64d7b5b98672',
                        zstd_version=cmd(opts.zstd,'--version').decode().strip())
        (opts.out/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
        archive = work/'members.zip'
        for rep in range(6):
            cohorts = ['tiny','mid']
            rng.shuffle(cohorts)
            for cohort in cohorts:
                members = [r for r in holdout if r['cohort']==cohort]
                arms = rng.sample(ARMS, len(ARMS))
                for order, arm in enumerate(arms):
                    if extracted.exists():
                        extracted.unlink()
                    lengths = []
                    tic = perf_counter_ns()
                    with ZipFile(archive, 'w') as zf:
                        if arm == 'zstd-dict':
                            entry(zf, DICT, dict_data)
                        for row in members:
                            key = row['path']
                            packet = encode(arm, paths[key], opts, dictionary)
                            lengths.append((key, packet))
                            entry(zf, key, packet)
                    enc_ns = perf_counter_ns()-tic
                    tic = perf_counter_ns()
                    with ZipFile(archive) as zf:
                        if arm == 'zstd-dict':
                            extracted.write_bytes(zf.read(DICT))
                        for i, row in enumerate(members):
                            decoded = decode(arm, zf.read(row['path']), opts, extracted)
                            (work/str(i)).write_bytes(decoded)
                    dec_ns = perf_counter_ns()-tic
                    for i, row in enumerate(members):
                        assert digest((work/str(i)).read_bytes())==row['input_sha256']
                    for key, packet in lengths:
                        if rep==0:
                            frames[arm,key]=packet
                            identities.append(dict(arm=arm, path=key, frame_bytes=len(packet),frame_sha256=digest(packet)))
                            if arm=='kolmo':
                                assert decode(arm,packet,opts,extracted,stock=True)==paths[key].read_bytes()
                        else:
                            assert packet==frames[arm,key]
                    times.append(dict(rep=rep,cohort=cohort,order=order,arm=arm,files=len(members),
                                input_bytes=sum(row['input_bytes'] for row in members),
                                frame_bytes=sum(len(frame) for _,frame in lengths),archive_bytes=archive.stat().st_size,
                                archive_sha256=digest(archive.read_bytes()),encode_ns=enc_ns,decode_ns=dec_ns))
        totals = []
        for arm in ARMS:
            for include in ([False, True] if arm=='zstd-dict' else [False]):
                dst=io.BytesIO()
                with ZipFile(dst, 'w') as zf:
                    if include: entry(zf, DICT, dict_data)
                    for r in holdout: entry(zf, r['path'], frames[arm,r['path']])
                totals.append(dict(arm=arm,dictionary_embedded=include,files=len(holdout),
                                   archive_bytes=len(dst.getvalue()),archive_sha256=digest(dst.getvalue())))
        write_csv(opts.out/'combined-archives.csv',totals)
    write_csv(opts.out/'rounds.csv',times)
    write_csv(opts.out/'frames.csv',identities)


if __name__=='__main__':
    main()
