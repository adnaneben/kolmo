#!/usr/bin/env python3
"""Synthetic-fixture tests for E02F prepare.py (no real data)."""
import io, json, pathlib, sys, tempfile, wave, zipfile
import numpy as np
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import prepare as P

def edf(sigs, spr, nrec, labels):
    ns = len(spr); h = bytearray(b' ' * (256 * (ns + 1)))
    def put(o, s, w): h[o:o + w] = s.encode().ljust(w)
    put(184, str(256 * (ns + 1)), 8); put(236, str(nrec), 8); put(244, '1', 8); put(252, str(ns), 4)
    for i, l in enumerate(labels): put(256 + 16 * i, l, 16)
    off = 256 + ns * (16 + 80 + 8 + 8 + 8 + 8 + 8 + 80)
    for i, n in enumerate(spr): put(off + 8 * i, str(n), 8)
    data = bytearray()
    for r in range(nrec):
        for s, n in zip(sigs, spr): data += np.asarray(s[r * n:(r + 1) * n], '<i2').tobytes()
    return bytes(h) + bytes(data)

def main():
    a = np.arange(-6, 6); b = np.array([100, -100, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]); ann = np.zeros(6)
    blob = edf([a, b, ann], [4, 4, 2], 3, ['Fc5.', 'Fc3.', 'EDF Annotations'])
    x, n = P.edf_signals(blob); assert n == 2 and x.tolist() == a.tolist() + b.tolist(), x
    try: P.edf_signals(blob[:-2]); raise SystemExit('short EDF accepted')
    except ValueError: pass
    pv = {'items': [{'timestamp': '2016010100', 'views': 5}, {'timestamp': '2016010200', 'views': 70000}, {'timestamp': '2016010400', 'views': 0}]}
    v, t0, t1 = P.pageviews(json.dumps(pv)); assert v.tolist() == [5, 70000, 0] and t0 == '2016010100'
    for bad in ({'items': [{'timestamp': '2', 'views': 1}, {'timestamp': '1', 'views': 1}]}, {'items': [{'timestamp': '1', 'views': -1}]}):
        try: P.pageviews(json.dumps(bad)); raise SystemExit('bad pageviews accepted')
        except ValueError: pass
    zb = io.BytesIO(); z = zipfile.ZipFile(zb, 'w')
    def wav(samples, rate=8000):
        f = io.BytesIO(); w = wave.open(f, 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate); w.writeframes(np.asarray(samples, '<i2').tobytes()); w.close(); return f.getvalue()
    z.writestr('free-spoken-digit-dataset-master/recordings/0_theo_0.wav', wav([1, 2])); z.writestr('free-spoken-digit-dataset-master/recordings/0_theo_1.wav', wav([3]))
    z.writestr('free-spoken-digit-dataset-master/recordings/1_theo_0.wav', wav([-7, 9])); z.close()
    s, used, miss = P.fsdd(zb.getvalue(), 'theo'); assert s.tolist() == [1, 2, 3, -7, 9] and used == 3 and len(miss) == 497
    zb2 = io.BytesIO(); z = zipfile.ZipFile(zb2, 'w'); z.writestr('x/recordings/0_theo_0.wav', wav([1], 16000)); z.close()
    try: P.fsdd(zb2.getvalue(), 'theo'); raise SystemExit('wrong rate accepted')
    except ValueError: pass
    with tempfile.TemporaryDirectory() as t:
        t = pathlib.Path(t); dl = t / 'dl'; dl.mkdir()
        (dl / 'S001R01.edf').write_bytes(blob); (dl / 'wiki_Earth.json').write_text(json.dumps(pv)); (dl / 'fsdd-master.zip').write_bytes(zb.getvalue())
        assert P.main(dl, t / 'out', t / 'm.json') == 0
        m = json.loads((t / 'm.json').read_text()); names = [i['name'] for i in m['inputs']]
        assert {'eeg_S001R01.npy', 'pageviews_Earth.npy', 'fsdd_theo.npy'} <= set(names) and 'S002R01.edf' in m['missing']
        assert np.load(t / 'out' / 'fsdd_theo.npy').tolist() == [1, 2, 3, -7, 9]
    print('E02F PREPARE TESTS PASSED')

if __name__ == '__main__': main()
