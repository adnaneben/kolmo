#!/usr/bin/env python3
"""Build the P2-E10 development cohort in N1's prepared-input format (observed sources only; development).

usage: prepare_e10.py E02E_OUT E02F_OUT E02G_OUT E02F_DL E02G_DL OUT
Inputs:
  - every derived .npy written by the unchanged E02E/E02F/E02G prepare.py scripts (their manifests are checked);
  - whole original files: FSDD recordings of speaker 'jackson' digits 0-9, take 0 (10 WAVs, 16-bit PCM, as published
    inside the pinned FSDD zip), and the Backblaze daily CSV data_Q1_2023/2023-01-01.csv (as published inside the zip).
Each row gets an OpenZL typed profile matching its declared element type (NPY descr / 16-bit WAV); the typed profile
is applied by OpenZL to the WHOLE original file (headers become a few extra values; lossless). CSV -> 'csv'.
Selection rules are fixed here before any P2-E10 measurement."""
import hashlib, io, json, pathlib, shutil, sys, zipfile, ast
PROF = {'<u2': 'le-u16', '<i2': 'le-i16', '<u4': 'le-u32', '<i4': 'le-i32', '<u8': 'le-u64', '<i8': 'le-i64'}
def sha(b): return hashlib.sha256(b).hexdigest()
def npy_descr(b):
    if b[:6] != b'\x93NUMPY': raise ValueError('not npy')
    hl = int.from_bytes(b[8:10], 'little') if b[6] == 1 else int.from_bytes(b[8:12], 'little')
    o = 10 if b[6] == 1 else 12; meta = ast.literal_eval(b[o:o + hl].decode('latin1').strip())
    return meta['descr'], (len(b) % int(meta['descr'][-1])) == 0
def main(e, f, g, fdl, gdl, out):
    out = pathlib.Path(out); inp = out / 'inputs'; inp.mkdir(parents=True, exist_ok=False); rows = []
    for tag, d in (('E02E', e), ('E02F', f), ('E02G', g)):
        man = json.loads((pathlib.Path(d) / 'manifest.json').read_text())
        for x in man['inputs']:
            p = pathlib.Path(d) / x['name']; b = p.read_bytes()
            if sha(b) != x['sha256']: raise ValueError(f'changed derived input {p}')
            descr, whole = npy_descr(b)
            rows.append(dict(id=f"{tag}__{x['name'][:-4]}", family=f"{tag}:{x['family']}", view='npy', profile=PROF.get(descr, 'serial') if whole else 'serial',
                             file='inputs/' + f"{tag}__{x['name']}", input_bytes=len(b), input_sha256=sha(b), origin='derived'))
            (inp / f"{tag}__{x['name']}").write_bytes(b)
    z = zipfile.ZipFile(pathlib.Path(fdl) / 'fsdd-master.zip'); names = {pathlib.PurePosixPath(n).name: n for n in z.namelist() if '/recordings/' in n}
    for dgt in range(10):
        k = f'{dgt}_jackson_0.wav'; b = z.read(names[k])
        rows.append(dict(id=f'FSDD__{k[:-4]}', family='FSDD:whole_wav', view='wav', profile='le-i16', file='inputs/FSDD__' + k,
                         input_bytes=len(b), input_sha256=sha(b), origin='whole original file (inside the pinned FSDD zip)'))
        (inp / ('FSDD__' + k)).write_bytes(b)
    z = zipfile.ZipFile(pathlib.Path(gdl) / 'data_Q1_2023.zip'); b = z.read('data_Q1_2023/2023-01-01.csv')
    rows.append(dict(id='BB__2023-01-01', family='Backblaze:whole_csv', view='csv', profile='csv', file='inputs/BB__2023-01-01.csv',
                     input_bytes=len(b), input_sha256=sha(b), origin='whole original file (inside the pinned Backblaze zip)'))
    (inp / 'BB__2023-01-01.csv').write_bytes(b)
    if len({r['profile'] for r in rows} & {'serial'}): pass
    (out / 'manifest.json').write_text(json.dumps(dict(experiment='P2-E10', split='development (observed sources)', inputs=rows), indent=1) + '\n')
    print(len(rows), 'inputs;', sum(r['input_bytes'] for r in rows), 'bytes')
if __name__ == '__main__': main(*sys.argv[1:7])
