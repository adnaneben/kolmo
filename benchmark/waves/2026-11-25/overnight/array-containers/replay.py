"""Check full NPY and Arrow containers on normal CLI paths and with an independent reader."""
import argparse
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import random
import statistics
import subprocess
import tempfile
import time
import wave

import numpy as np
import pyarrow as pa
import pyarrow.feather as feather

def sha(data):
    return hashlib.sha256(data).hexdigest()

def npy(array):
    out = io.BytesIO()
    np.save(out, array, allow_pickle=False)
    return out.getvalue()

def ipc(array):
    values = pa.array(array, mask=np.arange(len(array)) % 31 == 0)
    table = pa.table({'values': values, 'row': np.arange(len(array), dtype='int32')})
    sink = pa.BufferOutputStream()
    with pa.ipc.new_file(sink, table.schema) as writer:
        writer.write_table(table)
    return sink.getvalue().to_pybytes()

def describe(name, data):
    if name.endswith('.npy'):
        a = np.load(io.BytesIO(data), allow_pickle=False)
        result = {'shape': list(a.shape), 'dtype': a.dtype.str, 'value_bytes_sha256': sha(a.tobytes())}
        if name.startswith('bits'):
            assert a.view('<u8')[:8].tolist() == BIT_PATTERNS
        return result
    table = feather.read_table(pa.BufferReader(data)) if name.endswith('.feather') else pa.ipc.open_file(pa.BufferReader(data)).read_all()
    result = {'schema': str(table.schema), 'rows': table.num_rows, 'nulls': [col.null_count for col in table.columns]}
    return result

# Positive/negative zero, signed NaNs with different payloads, infinities and subnormals.
BIT_PATTERNS = [0, 0x8000000000000000, 0x7ff8000000000001, 0xfff8000000001234,
                0x7ff0000000000000, 0xfff0000000000000, 1, 0x8000000000000001]

def timed(argv):
    start = time.monotonic_ns()
    subprocess.run(argv, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return time.monotonic_ns() - start

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for arg in ('ncep','esc50','feather','main','candidate','reader','out'):
        ap.add_argument('--'+arg, type=Path, required=True)
    args = ap.parse_args()
    out = args.out
    out.mkdir(parents=True, exist_ok=True)
    originals = {k: getattr(args,k).read_bytes() for k in ('ncep','esc50','feather')}
    climate = np.load(io.BytesIO(originals['ncep']), allow_pickle=False).ravel()[:32768]
    with wave.open(io.BytesIO(originals['esc50'])) as wav:
        assert wav.getsampwidth()==2 and wav.getnchannels()==1
        audio = np.frombuffer(wav.readframes(32768), dtype='<i2')
    patterns = np.tile(np.asarray(BIT_PATTERNS,dtype='<u8'),2048).view('<f8')
    sources = {'climate-le.npy': npy(climate.astype('<f4')), 'climate-be.npy': npy(climate.astype('>f4')),
               'audio-le.npy': npy(audio), 'audio-be.npy': npy(audio.astype('>i2')), 'bits-le.npy': npy(patterns),
               'climate.arrow': ipc(climate), 'audio.arrow': ipc(audio), 'published.feather': originals['feather']}
    data = {'numpy':np.__version__, 'pyarrow':pa.__version__, 'seed':725119, 'rounds':3,
            'parents':{k:{'name':getattr(args,k).name, 'size':len(b), 'sha256':sha(b)} for k,b in originals.items()},
            'binaries':{k:sha(getattr(args,k).read_bytes()) for k in ('main','candidate','reader')},
            'files':{k:{'bytes':len(b),'sha256':sha(b),**describe(k,b)} for k,b in sources.items()}}
    (out/'manifest.json').write_text(json.dumps(data,indent=2)+'\n')
    rng=random.Random(data['seed'])
    rows=[]
    with tempfile.TemporaryDirectory() as temp:
        d=Path(temp)
        for name, payload in sources.items():
            (d/name).write_bytes(payload)
        for rep in range(data['rounds']+1):
            cells=[(source,level) for source in sources for level in ('1','3','7','ultra')]
            rng.shuffle(cells)
            for source,level in cells:
                arms=['main','candidate']
                rng.shuffle(arms)
                for arm in arms:
                    frame, restored=d/'frame', d/'restored'
                    exe=getattr(args,arm)
                    enc=timed([exe,'--level',level,'-T1','-fq',d/source,'-o',frame])
                    dec=timed([exe,'-d','-fq','-T1',frame,'-o',restored])
                    raw=restored.read_bytes()
                    assert raw==sources[source] and describe(source,raw)==describe(source,sources[source])
                    if rep==0:
                        subprocess.run([args.reader,'-d','-fq','-T1',frame,'-o',restored],check=True,
                                       stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
                        assert restored.read_bytes()==sources[source]
                    archive=frame.read_bytes()
                    rows.append({'source':source,'level':level,'arm':arm,'repeat':rep,'input_bytes':len(raw),
                                 'stored_bytes':len(archive),'frame_sha256':sha(archive),'restored_sha256':sha(raw),
                                 'enc_ns':enc,'dec_ns':dec})
    with (out/'raw.csv').open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=rows[0]);writer.writeheader();writer.writerows(rows)
    scores=[]
    for source in sources:
        for level in ('1','3','7','ultra'):
            selected=[r for r in rows if r['source']==source and r['level']==level and r['repeat']>0]
            for arm in ('main','candidate'):
                part=[r for r in selected if r['arm']==arm]
                assert len(part)==3 and len({r['frame_sha256'] for r in part})==1
                rate=lambda times: len(sources[source])*1000/statistics.median(times)
                scores.append({'source':source,'level':level,'arm':arm,'stored_bytes':part[0]['stored_bytes'],
                    'frame_sha256':part[0]['frame_sha256'],'encode_MBps':rate(r['enc_ns'] for r in part),
                    'decode_MBps':rate(r['dec_ns'] for r in part),'combined_MBps':rate(r['enc_ns']+r['dec_ns'] for r in part)})
    with (out/'score.csv').open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=scores[0]);writer.writeheader();writer.writerows(scores)
    print(len(rows),'complete CLI restores and',len(sources)*4*2,'independent warmup reads')
if __name__=='__main__':
    main()
