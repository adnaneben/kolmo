#!/usr/bin/env python3
"""E02G data preparation, fixed at registration (PREREGISTRATION.md). Reads already-downloaded publisher files.

usage: prepare.py DOWNLOAD_DIR OUT_DIR MANIFEST_JSON
"""
import csv, hashlib, io, json, pathlib, sys, zipfile
from decimal import Decimal, InvalidOperation
import numpy as np
import importlib.util as _u
_spec = _u.spec_from_file_location('e02f_prepare', pathlib.Path(__file__).resolve().parents[1] / 'E02F' / 'prepare.py')
_e02f = _u.module_from_spec(_spec); _spec.loader.exec_module(_e02f)
edf_signals = _e02f.edf_signals                      # E02F's EDF conversion, unchanged

BB_TARGET = ['capacity_bytes', 'smart_9_raw', 'smart_241_raw', 'smart_242_raw', 'smart_5_raw', 'smart_197_raw']
BB_CONTROL = ['smart_194_raw']
OWID = ['total_cases', 'new_cases', 'total_deaths', 'new_deaths', 'total_vaccinations', 'people_vaccinated']
EEG = ['S005', 'S006', 'S007', 'S008']
I64 = (-(1 << 63), (1 << 63) - 1)

def sha(b): return hashlib.sha256(b).hexdigest()

def npy(arr):
    f = io.BytesIO(); np.save(f, np.ascontiguousarray(arr), allow_pickle=False); b = f.getvalue()
    if b[:8] != b'\x93NUMPY\x01\x00': raise ValueError('unexpected .npy version')
    return b, 10 + int.from_bytes(b[8:10], 'little')

def int_columns(text, fields):
    """{field: (int64 array, blanks)} in row order; blank tokens skipped; non-integers rejected. Missing field -> None."""
    rd = csv.reader(io.StringIO(text)); hdr = next(rd); idx = {f: hdr.index(f) for f in fields if f in hdr}
    vals = {f: [] for f in idx}; blanks = {f: 0 for f in idx}
    for row in rd:
        for f, j in idx.items():
            t = row[j].strip() if j < len(row) else ''
            if t == '': blanks[f] += 1; continue
            try: d = Decimal(t)
            except InvalidOperation: raise ValueError(f'{f}: not a number: {t!r}')
            if d != d.to_integral_value(): raise ValueError(f'{f}: not an integer: {t!r}')
            v = int(d)
            if not I64[0] <= v <= I64[1]: raise ValueError(f'{f}: outside int64: {t!r}')
            vals[f].append(v)
    return {f: (np.array(vals[f], '<i8') if f in vals else None, blanks.get(f)) for f in fields}

def main(dl, out, manifest):
    dl, out = pathlib.Path(dl), pathlib.Path(out); out.mkdir(parents=True, exist_ok=False)
    items, sources, missing = [], [], []
    def src(p): b = p.read_bytes(); sources.append(dict(file=p.name, sha256=sha(b), bytes=len(b))); return b
    def add(name, family, role, arr, source, conversion):
        b, hl = npy(arr); (out / name).write_bytes(b); w = arr.dtype.itemsize; cnt = (len(b) - hl) // w
        items.append(dict(name=name, family=family, role=role, origin='real', bytes=len(b), sha256=sha(b), width=w, head=hl,
                          tail=len(b) - hl - cnt * w, count=cnt, dtype=str(arr.dtype), source=source, conversion=conversion,
                          split='E02G validation', holdout=False))
    zp = dl / 'data_Q1_2023.zip'
    if zp.exists():
        z = zipfile.ZipFile(io.BytesIO(src(zp)))
        members = sorted(n for n in z.namelist() if n.endswith('.csv') and not n.split('/')[-1].startswith('.'))
        if not members: missing.append('Backblaze zip: no csv member')
        else:
            mb = z.read(members[0]); sources.append(dict(file=f'{zp.name}:{members[0]}', sha256=sha(mb), bytes=len(mb)))
            cols = int_columns(mb.decode('utf-8'), BB_TARGET + BB_CONTROL)
            for f in BB_TARGET + BB_CONTROL:
                a, bl = cols[f]
                if a is None or not len(a): missing.append(f'backblaze {f}'); continue
                ctl = f in BB_CONTROL
                add(f'backblaze_{f}.npy', 'C2_single_byte_control' if ctl else 'F1_backblaze', 'control' if ctl else 'target', a,
                    f'Backblaze drive stats Q1 2023, {members[0]}', f'{f}, {bl} blank tokens skipped, int64')
    else: missing.append(zp.name)
    op = dl / 'owid-covid-data.csv'
    if op.exists():
        cols = int_columns(src(op).decode('utf-8'), OWID)
        for f in OWID:
            a, bl = cols[f]
            if a is None or not len(a): missing.append(f'owid {f}'); continue
            add(f'owid_{f}.npy', 'F2_owid_covid', 'target', a, 'Our World in Data owid-covid-data.csv', f'{f}, {bl} blank tokens skipped, int64')
    else: missing.append(op.name)
    for s in EEG:
        p = dl / f'{s}R01.edf'
        if not p.exists(): missing.append(p.name); continue
        a, nsig = edf_signals(src(p))
        add(f'eeg_{s}R01.npy', 'C1_dense_control', 'control', a, f'PhysioNet eegmmidb 1.0.0 {s}/{s}R01.edf', f'{nsig} data signals, channel-major int16')
    m = dict(experiment='E02G', split='validation (not a final holdout)', generator='research/codec/E02G/prepare.py', numpy=np.__version__,
             sources=sources, missing=missing, inputs=items)
    pathlib.Path(manifest).write_text(json.dumps(m, indent=1) + '\n')
    print(f'{len(items)} inputs; {len(missing)} missing entries (listed in the manifest)')
    return 0 if items else 1

if __name__ == '__main__': sys.exit(main(*sys.argv[1:4]))
