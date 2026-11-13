#!/usr/bin/env python3
"""Exact-frame and cross-decoding regression, not a performance benchmark."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import random
import struct
import subprocess


def npy(payload: bytes, dtype: str, shape: tuple[int, ...]) -> bytes:
    header = f"{{'descr': '{dtype}', 'fortran_order': False, 'shape': {shape!r}, }}".encode('ascii')
    length = ((10 + len(header) + 1 + 63) // 64) * 64 - 10
    return b'\x93NUMPY\x01\x00' + struct.pack('<H', length) + header.ljust(length - 1, b' ') + b'\n' + payload


def generate(out: Path) -> list[dict]:
    out.mkdir(parents=True, exist_ok=False)
    rng = random.Random(731094)
    floats = bytearray(b'ABC')
    for i in range(300000):
        g = sum(rng.randrange(20001) / 10000.0 - 1.0 for _ in range(4))
        floats.extend(struct.pack('<f', 0.0 if i % 5000 == 7 else g * .02))
    floats.extend(b'\x01\x02')
    half = b''.join(struct.pack('<H', (0x3800 + rng.randrange(2048) - 1024) |
                  (0x8000 if rng.randrange(2) else 0)) for _ in range(400000))
    rgb = bytearray()
    for y in range(300):
        for x in range(640):
            for c in range(3):
                v = (x * 37 + c * 11) % 199 + y // 4 + rng.randrange(5) - 2
                rgb.append(max(0, min(255, v)))
    rgb.extend(bytes(range(7)))
    integer = bytearray(); v = 0
    for _ in range(300000):
        v += rng.randrange(21) - 10
        integer.extend(struct.pack('<i', v))
    wrap = b''.join(struct.pack('<Q', ((1 << 63) + i % 7 if i % 3 == 0 else
                    (1 << 63) - 1 - i % 7 if i % 3 == 1 else i % 1000)) for i in range(200000))
    records = b''.join(struct.pack('<QdIIII', i * 1000, (i % 101) * .125,
                                   i % 19, i // 251, 0, 0) for i in range(40000))
    entries = [('empty.bin', b''), ('random.bin', bytes(rng.getrandbits(8) for _ in range(65536))),
               ('repeated.log', b'2026-11-11 device=17 reading=1024 status=OK\n' * 4096),
               ('int32.npy', npy(bytes(integer), '<i4', (300000,))), ('int_wrap.bin', wrap),
               ('float32_odd.bin', bytes(floats)), ('float16.npy', npy(half, '<f2', (400000,))),
               ('rgb_odd.bin', bytes(rgb)), ('records.bin', records)]
    manifest = []
    for name, data in entries:
        (out / name).write_bytes(data)
        manifest.append({'name': name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
                         'partition': 'development-regression', 'seed': 731094})
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--candidate', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    baseline, candidate, out = args.baseline.resolve(), args.candidate.resolve(), args.output.resolve()
    for exe in (baseline, candidate):
        if not exe.is_file(): p.error(f'Missing executable: {exe}')
    out.mkdir(parents=True, exist_ok=False)
    state = {'status': 'running', 'scope': 'deterministic regression, NOT performance or release acceptance', 'pairs': []}
    def save():
        (out / 'identity.json').write_text(json.dumps(state, indent=2) + '\n')
    def run(cmd):
        with (out / 'commands.log').open('a') as log:
            log.write(json.dumps(cmd) + '\n'); log.flush()
            subprocess.run(cmd, check=True, timeout=600, stdout=log, stderr=subprocess.STDOUT)
    save()
    try:
        corpus = out / 'corpus'; manifest = generate(corpus)
        frames = out / 'frames'; frames.mkdir()
        profiles = [('speed', []), ('balanced', []), ('hc', []), ('max', []), ('balanced-1MiB', ['-B', '1'])]
        for item in manifest:
            source = corpus / item['name']; original = source.read_bytes()
            for label, extra in profiles:
                mode = 'balanced' if label == 'balanced-1MiB' else label
                paths = []
                for name, exe in (('baseline', baseline), ('candidate', candidate)):
                    dest = frames / f"{item['name']}.{label}.{name}.kmo"
                    run([str(exe), '-m', mode, '-T', '1', *extra, str(source), str(dest)])
                    paths.append(dest)
                data = paths[0].read_bytes()
                if data != paths[1].read_bytes():
                    raise RuntimeError(f"Compressed bytes changed: {item['name']}/{label}")
                for name, exe, encoded in (('baseline', baseline, paths[1]), ('candidate', candidate, paths[0])):
                    decoded = frames / f"{item['name']}.{label}.{name}.out"
                    run([str(exe), '-d', '-T', '1', str(encoded), str(decoded)])
                    if decoded.read_bytes() != original:
                        raise RuntimeError(f"Cross-decoding failed: {item['name']}/{label}/{name}")
                state['pairs'].append({'file': item['name'], 'profile': label,
                    'input_sha256': item['sha256'], 'frame_sha256': hashlib.sha256(data).hexdigest(),
                    'stored_bytes': len(data), 'cross_decodes': 2})
                save()
        if len(state['pairs']) != 45: raise RuntimeError('Incomplete identity matrix')
        state['status'] = 'passed_45_frame_pairs_90_cross_decodes_NOT_RELEASE_ACCEPTANCE'
        save(); print(state['status'])
    except BaseException as exc:
        state['status'] = 'failed_or_interrupted'; state['error'] = str(exc); save(); raise


if __name__ == '__main__':
    main()
