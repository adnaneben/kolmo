#!/usr/bin/env python3
"""E02F data preparation, fixed at registration (PREREGISTRATION.md). Reads already-downloaded publisher files.

usage: prepare.py DOWNLOAD_DIR OUT_DIR MANIFEST_JSON
"""
import hashlib, io, json, pathlib, sys, wave, zipfile
import numpy as np

EEG = ['S001', 'S002', 'S003', 'S004']
ARTICLES = ['Earth', 'Moon', 'Mathematics', 'Water', 'London', 'Music', 'Albert_Einstein', 'Python_(programming_language)']
SPEAKERS = ['jackson', 'nicolas', 'theo', 'yweweler']

def sha(b): return hashlib.sha256(b).hexdigest()

def npy(arr):
    f = io.BytesIO(); np.save(f, np.ascontiguousarray(arr), allow_pickle=False); b = f.getvalue()
    if b[:8] != b'\x93NUMPY\x01\x00': raise ValueError('unexpected .npy version')
    return b, 10 + int.from_bytes(b[8:10], 'little')

def edf_signals(b):
    """all non-annotation signals, channel-major, int16"""
    if len(b) < 256: raise ValueError('short EDF')
    nrec, ns = int(b[236:244].decode().strip()), int(b[252:256].decode().strip())
    hl = 256 * (ns + 1)
    if int(b[184:192].decode().strip()) != hl: raise ValueError('EDF header length mismatch')
    off = 256; labels = [b[off + 16 * i: off + 16 * i + 16].decode('ascii').strip() for i in range(ns)]
    off = 256 + ns * (16 + 80 + 8 + 8 + 8 + 8 + 8 + 80)
    spr = [int(b[off + 8 * i: off + 8 * i + 8].decode().strip()) for i in range(ns)]
    tot = sum(spr)
    if nrec < 1 or len(b) != hl + nrec * tot * 2: raise ValueError(f'EDF data length {len(b) - hl} != {nrec} x {tot} x 2')
    d = np.frombuffer(b, '<i2', offset=hl).reshape(nrec, tot)
    parts, start = [], 0
    for lab, n in zip(labels, spr):
        if not lab.startswith('EDF Annotations'): parts.append(d[:, start:start + n].reshape(-1))
        start += n
    if not parts: raise ValueError('no data signals')
    return np.concatenate(parts).astype('<i2'), len(parts)

def pageviews(text):
    items = json.loads(text)['items']; ts = [it['timestamp'] for it in items]; v = [it['views'] for it in items]
    if any(b <= a for a, b in zip(ts, ts[1:])): raise ValueError('timestamps not strictly increasing')
    if any(not isinstance(x, int) or x < 0 or x > 2 ** 31 - 1 for x in v): raise ValueError('views out of range')
    return np.array(v, '<i4'), ts[0] if ts else None, ts[-1] if ts else None

def fsdd(zbytes, speaker):
    z = zipfile.ZipFile(io.BytesIO(zbytes)); names = {pathlib.PurePosixPath(n).name: n for n in z.namelist() if '/recordings/' in n and n.endswith('.wav')}
    frames, missing, used = [], [], 0
    for d in range(10):
        for i in range(50):
            k = f'{d}_{speaker}_{i}.wav'
            if k not in names: missing.append(k); continue
            w = wave.open(io.BytesIO(z.read(names[k])))
            if (w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getcomptype()) != (1, 2, 8000, 'NONE'): raise ValueError(f'{k}: not PCM mono 16-bit 8 kHz')
            frames.append(w.readframes(w.getnframes())); used += 1
    if not used: raise ValueError(f'no recordings for {speaker}')
    return np.frombuffer(b''.join(frames), '<i2').copy(), used, missing

def main(dl, out, manifest):
    dl, out = pathlib.Path(dl), pathlib.Path(out); out.mkdir(parents=True, exist_ok=False)
    items, sources, missing = [], [], []
    def src(p): b = p.read_bytes(); sources.append(dict(file=p.name, sha256=sha(b), bytes=len(b))); return b
    def add(name, family, arr, source, conversion):
        b, hl = npy(arr); (out / name).write_bytes(b); w = arr.dtype.itemsize; cnt = (len(b) - hl) // w
        items.append(dict(name=name, family=family, origin='real', bytes=len(b), sha256=sha(b), width=w, head=hl, tail=len(b) - hl - cnt * w,
                          count=cnt, dtype=str(arr.dtype), source=source, conversion=conversion, split='E02F validation', holdout=False))
    for s in EEG:
        p = dl / f'{s}R01.edf'
        if not p.exists(): missing.append(p.name); continue
        a, nsig = edf_signals(src(p))
        add(f'eeg_{s}R01.npy', 'F1_eeg', a, f'PhysioNet eegmmidb 1.0.0 {s}/{s}R01.edf', f'{nsig} data signals, channel-major int16')
    for art in ARTICLES:
        p = dl / f'wiki_{art}.json'
        if not p.exists(): missing.append(p.name); continue
        a, t0, t1 = pageviews(src(p).decode('utf-8'))
        if not len(a): missing.append(f'{p.name} (no items)'); continue
        add(f'pageviews_{art.replace("(", "").replace(")", "")}.npy', 'F2_pageviews', a, f'Wikimedia pageviews en.wikipedia {art} user daily', f'views {t0}..{t1}, {len(a)} days, int32')
    zp = dl / 'fsdd-master.zip'
    if zp.exists():
        zb = src(zp)
        for spk in SPEAKERS:
            try: a, used, miss = fsdd(zb, spk)
            except ValueError as e:
                if 'no recordings' not in str(e): raise
                missing.append(f'fsdd speaker {spk} (no recordings)'); continue
            missing += miss
            add(f'fsdd_{spk}.npy', 'F3_speech', a, f'Free Spoken Digit Dataset master.zip, speaker {spk}', f'{used} recordings concatenated in (digit, index) order, int16')
    else: missing.append(zp.name)
    m = dict(experiment='E02F', split='validation (not a final holdout)', generator='research/codec/E02F/prepare.py', numpy=np.__version__,
             sources=sources, missing=missing, inputs=items)
    pathlib.Path(manifest).write_text(json.dumps(m, indent=1) + '\n')
    print(f'{len(items)} inputs; {len(missing)} missing entries (listed in the manifest)')
    return 0 if items else 1

if __name__ == '__main__': sys.exit(main(*sys.argv[1:4]))
