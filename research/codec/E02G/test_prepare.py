#!/usr/bin/env python3
"""Synthetic-fixture tests for E02G prepare.py (no real data)."""
import io, json, pathlib, sys, tempfile, zipfile
import numpy as np
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import prepare as P

def main():
    csvt = 'date,serial_number,capacity_bytes,smart_9_raw,smart_194_raw\n2023-01-01,A,4000787030016,1000,30\n2023-01-01,B,,1001.0,31\n2023-01-01,C,-1,9223372036854775807,\n'
    c = P.int_columns(csvt, ['capacity_bytes', 'smart_9_raw', 'smart_194_raw', 'smart_241_raw'])
    assert c['capacity_bytes'][0].tolist() == [4000787030016, -1] and c['capacity_bytes'][1] == 1
    assert c['smart_9_raw'][0].tolist() == [1000, 1001, (1 << 63) - 1] and c['smart_194_raw'][0].tolist() == [30, 31]
    assert c['smart_241_raw'] == (None, None)
    for bad in ('x,smart_9_raw\n1,1.5\n', 'x,smart_9_raw\n1,abc\n', 'x,smart_9_raw\n1,9223372036854775808\n'):
        try: P.int_columns(bad, ['smart_9_raw']); raise SystemExit(f'bad token accepted: {bad!r}')
        except ValueError: pass
    with tempfile.TemporaryDirectory() as t:
        t = pathlib.Path(t); dl = t / 'dl'; dl.mkdir()
        zb = io.BytesIO(); z = zipfile.ZipFile(zb, 'w'); z.writestr('data_Q1_2023/2023-01-02.csv', 'smart_9_raw\n7\n'); z.writestr('data_Q1_2023/2023-01-01.csv', csvt); z.writestr('__MACOSX/._x.csv', 'junk'); z.close()
        (dl / 'data_Q1_2023.zip').write_bytes(zb.getvalue())
        (dl / 'owid-covid-data.csv').write_text('iso_code,date,total_cases,new_cases\nAFG,2020-01-05,,0.0\nAFG,2020-01-06,5,5\nAFG,2020-01-07,4,-1\n')
        assert P.main(dl, t / 'out', t / 'm.json') == 0
        m = json.loads((t / 'm.json').read_text()); byname = {i['name']: i for i in m['inputs']}
        assert '2023-01-01.csv' in [s['file'] for s in m['sources'] if ':' in s['file']][0]
        assert byname['backblaze_smart_9_raw.npy']['role'] == 'target' and byname['backblaze_smart_194_raw.npy']['role'] == 'control'
        assert np.load(t / 'out' / 'owid_new_cases.npy').tolist() == [0, 5, -1]
        assert 'backblaze smart_241_raw' in m['missing'] and 'owid total_deaths' in m['missing'] and 'S005R01.edf' in m['missing']
    print('E02G PREPARE TESTS PASSED')

if __name__ == '__main__': main()
