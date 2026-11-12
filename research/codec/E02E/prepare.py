#!/usr/bin/env python3
"""E02E data preparation, fixed at registration (see PREREGISTRATION.md). No network: reads publisher files that were
already downloaded into DOWNLOAD_DIR, verifies their structure, writes derived .npy inputs and the manifest.

usage: prepare.py DOWNLOAD_DIR OUT_DIR MANIFEST_JSON
"""
import hashlib, json, pathlib, sys
import numpy as np

GHCN = ['USW00094728', 'USW00023174', 'USW00014739']
ELEMENTS = ['TMAX', 'TMIN', 'PRCP']
MITDB = ['100', '101', '200', '201']

def sha(b): return hashlib.sha256(b).hexdigest()

def npy(arr):
    import io
    f = io.BytesIO(); np.save(f, np.ascontiguousarray(arr), allow_pickle=False); b = f.getvalue()
    if b[:8] != b'\x93NUMPY\x01\x00': raise ValueError('unexpected .npy version')
    return b, 10 + int.from_bytes(b[8:10], 'little')

def ghcn_values(text, station, element):
    """values in publisher order; -9999 skipped; flags ignored. Returns (values, skipped_slots)."""
    out, skipped = [], 0
    for ln, line in enumerate(text.splitlines()):
        if not line.strip(): continue
        if len(line) < 269 or line[:11] != station: raise ValueError(f'{station}: malformed line {ln + 1}')
        if line[17:21] != element: continue
        for d in range(31):
            v = int(line[21 + 8 * d: 26 + 8 * d])
            if v == -9999: skipped += 1
            else: out.append(v)
    return out, skipped

def mitdb_signals(hea, dat):
    first = hea.split('\n')[0].split()
    nsig, nsamp = int(first[1]), int(first[3])
    sigs = [l.split() for l in hea.split('\n')[1:1 + nsig]]
    if nsig != 2 or any(s[1].split('x')[0].split(':')[0] != '212' for s in sigs): raise ValueError('expected 2 signals in format 212')
    if len(dat) != nsamp * 3: raise ValueError(f'.dat length {len(dat)} != 3 * {nsamp}')
    b = np.frombuffer(dat, np.uint8).reshape(-1, 3).astype(np.int32)
    s0 = b[:, 0] | ((b[:, 1] & 0x0F) << 8); s1 = b[:, 2] | ((b[:, 1] & 0xF0) << 4)
    s0 = np.where(s0 >= 2048, s0 - 4096, s0); s1 = np.where(s1 >= 2048, s1 - 4096, s1)
    return s0.astype('<i2'), s1.astype('<i2')

def main(dl, out, manifest):
    dl, out = pathlib.Path(dl), pathlib.Path(out); out.mkdir(parents=True, exist_ok=False)
    items, sources, missing = [], [], []
    def add(name, family, arr, source, conversion):
        b, hl = npy(arr); (out / name).write_bytes(b); w = arr.dtype.itemsize; cnt = (len(b) - hl) // w
        items.append(dict(name=name, family=family, origin='real', bytes=len(b), sha256=sha(b), width=w, head=hl, tail=len(b) - hl - cnt * w,
                          count=cnt, dtype=str(arr.dtype), source=source, conversion=conversion, split='E02E validation', holdout=False))
    for st in GHCN:
        p = dl / f'{st}.dly'
        if not p.exists(): missing.append(p.name); continue
        raw = p.read_bytes(); sources.append(dict(file=p.name, sha256=sha(raw), bytes=len(raw)))
        text = raw.decode('ascii')
        for el in ELEMENTS:
            v, sk = ghcn_values(text, st, el)
            if not v: missing.append(f'{st}:{el} (no values)'); continue
            add(f'ghcn_{st}_{el}.npy', 'F1_ghcn_daily', np.array(v, '<i4'), f'NCEI GHCN-Daily {st}.dly', f'{el} values in file order, {sk} missing slots skipped, int32 .npy')
    for rec in MITDB:
        h, d = dl / f'{rec}.hea', dl / f'{rec}.dat'
        if not (h.exists() and d.exists()): missing.append(f'{rec}.hea/.dat'); continue
        hb, db = h.read_bytes(), d.read_bytes(); sources += [dict(file=h.name, sha256=sha(hb), bytes=len(hb)), dict(file=d.name, sha256=sha(db), bytes=len(db))]
        s0, s1 = mitdb_signals(hb.decode('ascii'), db)
        for k, s in enumerate((s0, s1)):
            add(f'mitdb_{rec}_sig{k}.npy', 'F2_mitdb', s, f'PhysioNet mitdb 1.0.0 record {rec}', f'format 212 signal {k}, 12-bit two\'s complement, int16 .npy')
    m = dict(experiment='E02E', split='validation (not a final holdout)', generator='research/codec/E02E/prepare.py',
             numpy=np.__version__, sources=sources, missing=missing, inputs=items)
    pathlib.Path(manifest).write_text(json.dumps(m, indent=1) + '\n')
    print(f'{len(items)} inputs; missing: {missing or "none"}')
    return 0 if items else 1

if __name__ == '__main__': sys.exit(main(*sys.argv[1:4]))
