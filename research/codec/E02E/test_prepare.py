#!/usr/bin/env python3
"""Synthetic-fixture tests for prepare.py (no real data)."""
import pathlib, sys, tempfile
import numpy as np
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import prepare as P

def dly_line(st, y, mo, el, vals):
    s = f'{st}{y:04d}{mo:02d}{el}'
    for v in vals: s += f'{v:5d}   '
    return s

def main():
    st = 'USW00094728'
    lines = [dly_line(st, 2000, 1, 'TMAX', [10, -5, -9999] + [7] * 28), dly_line(st, 2000, 1, 'PRCP', [0] * 31),
             dly_line(st, 2000, 2, 'TMAX', [-9999] * 29 + [3, 4])]
    v, sk = P.ghcn_values('\n'.join(lines) + '\n', st, 'TMAX')
    assert v == [10, -5] + [7] * 28 + [3, 4] and sk == 30, (v[:5], sk)
    try: P.ghcn_values(dly_line('USW00000001', 2000, 1, 'TMAX', [1] * 31), st, 'TMAX'); raise SystemExit('wrong station accepted')
    except ValueError: pass
    a = np.array([0, 1, -1, 2047, -2048, 100, -100, 5], np.int32); b = np.array([-2048, 2047, 0, -1, 1, 7, -7, 0], np.int32)
    ua, ub = a & 0xFFF, b & 0xFFF
    dat = np.stack([ua & 0xFF, ((ua >> 8) & 0x0F) | (((ub >> 8) & 0x0F) << 4), ub & 0xFF], 1).astype(np.uint8).tobytes()
    hea = '100 2 360 8\n100.dat 212 200 11 1024 0 0 0 MLII\n100.dat 212 200 11 1024 0 0 0 V5\n'
    s0, s1 = P.mitdb_signals(hea, dat)
    assert s0.tolist() == a.tolist() and s1.tolist() == b.tolist()
    for bad in (hea.replace('212', '16'), hea.replace('2 360 8', '2 360 9')):
        try: P.mitdb_signals(bad, dat); raise SystemExit('bad header accepted')
        except ValueError: pass
    with tempfile.TemporaryDirectory() as t:
        t = pathlib.Path(t); dl = t / 'dl'; dl.mkdir()
        (dl / f'{st}.dly').write_text('\n'.join(lines) + '\n'); (dl / '100.hea').write_text(hea); (dl / '100.dat').write_bytes(dat)
        assert P.main(dl, t / 'out', t / 'm.json') == 0
        import json; m = json.loads((t / 'm.json').read_text())
        names = [i['name'] for i in m['inputs']]
        assert 'ghcn_USW00094728_TMAX.npy' in names and 'mitdb_100_sig1.npy' in names and 'USW00023174.dly' in m['missing']
        r = np.load(t / 'out' / 'mitdb_100_sig0.npy'); assert r.dtype == np.dtype('<i2') and r.tolist() == a.tolist()
    print('E02E PREPARE TESTS PASSED')

if __name__ == '__main__': main()
