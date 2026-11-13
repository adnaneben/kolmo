#!/usr/bin/env python3
"""Build the E02C DEVELOPMENT corpus outside the repository and write its manifest.

Every input is development data. None of it is an untouched holdout, and none of the repository's declared
holdouts (benchmark/holdout, benchmark/frontier holdout-inputs, the OPSD electricity holdout) is used.
Sources are files present in the container image (LibreOffice gallery sounds, matplotlib sample data) plus
derived filesystem metadata and labelled synthetic arrays; network downloads were not available.

usage: build_corpus.py OUT_DIR MANIFEST_JSON
"""
import gzip, hashlib, json, os, pathlib, struct, sys, wave
import numpy as np

MPL = pathlib.Path('/usr/local/lib/python3.13/site-packages/matplotlib/mpl-data/sample_data')
SND = pathlib.Path('/usr/lib/libreoffice/share/gallery/sounds')
WAVS = ['roll', 'space2', 'gong', 'untie', 'soft', 'drama', 'space3', 'curve']   # the largest 16-bit mono files

def sha(b): return hashlib.sha256(b).hexdigest()

def npy(arr):
    """NumPy .npy v1.0 (the format the production planner recognises): header, then little-endian data."""
    buf = pathlib.Path('/tmp/e02c-npy.npy'); np.save(buf, np.ascontiguousarray(arr), allow_pickle=False)
    b = buf.read_bytes(); buf.unlink(); hl = 10 + struct.unpack('<H', b[8:10])[0]; return b, hl

def wav_data_offset(b):
    i = 12
    while i + 8 <= len(b):
        cid, sz = b[i:i + 4], struct.unpack('<I', b[i + 4:i + 8])[0]
        if cid == b'data': return i + 8, sz
        i += 8 + sz + (sz & 1)
    raise ValueError('no data chunk')

def main(out, manifest):
    out = pathlib.Path(out); out.mkdir(parents=True, exist_ok=True); items = []
    def add(name, family, origin, data, w, head, dtype, source, conversion, source_sha=None):
        (out / name).write_bytes(data)
        count = (len(data) - head) // w
        items.append(dict(name=name, family=family, origin=origin, bytes=len(data), sha256=sha(data), width=w, head=head,
                          tail=len(data) - head - count * w, count=count, dtype=dtype, source=source,
                          source_sha256=source_sha, conversion=conversion, split='development', holdout=False))
    # real: 16-bit PCM audio, complete WAV files as published in the image (header and any trailing chunks kept)
    for s in WAVS:
        p = SND / f'{s}.wav'; b = p.read_bytes(); w = wave.open(str(p)); assert w.getsampwidth() == 2 and w.getnchannels() == 1
        off, sz = wav_data_offset(b)
        add(f'audio_{s}.wav', 'audio_s16', 'real', b, 2, off, '<i2 mono', str(p), 'none (file as published)', sha(b))
    # derived: all 35 gallery sounds' PCM payloads concatenated (larger input for timing), raw s16le, no header
    pcm = b''.join(wave.open(str(p)).readframes(10 ** 9) for p in sorted(SND.glob('*.wav')))
    add('audio_concat_s16.raw', 'audio_s16', 'derived', pcm, 2, 0, '<i2', f'{SND}/*.wav (35 files, sorted by name)', 'PCM data chunks concatenated')
    # real: MRI slice (big-endian uint16 in the source), DEM elevation, bathymetry/topography
    raw = gzip.open(MPL / 's1045.ima.gz').read()
    arr = np.frombuffer(raw, '>u2').astype('<u2').reshape(256, 256); b, hl = npy(arr)
    add('mri_s1045_u16.npy', 'raster_16', 'real', b, 2, hl, '<u2 256x256', str(MPL / 's1045.ima.gz'), 'gunzip; big-endian to little-endian; .npy wrap', sha((MPL / 's1045.ima.gz').read_bytes()))
    z = np.load(MPL / 'jacksboro_fault_dem.npz'); b, hl = npy(z['elevation'].astype('<i2'))
    add('dem_jacksboro_i16.npy', 'raster_16', 'real', b, 2, hl, '<i2 344x403', str(MPL / 'jacksboro_fault_dem.npz'), "array 'elevation' extracted; .npy wrap", sha((MPL / 'jacksboro_fault_dem.npz').read_bytes()))
    t = np.load(MPL / 'topobathy.npz')['topo']; assert np.all(t == np.round(t)); b, hl = npy(t.astype('<i2'))
    add('topobathy_i16.npy', 'raster_16', 'real', b, 2, hl, '<i2 91x120', str(MPL / 'topobathy.npz'), "array 'topo' (integral float32) cast to int16; .npy wrap", sha((MPL / 'topobathy.npz').read_bytes()))
    # real: daily market series (dates as int64 day numbers, volumes as int64)
    g = np.load(MPL / 'goog.npz')['price_data']
    b, hl = npy(g['date'].astype('datetime64[D]').astype('<i8')); add('goog_date_i64.npy', 'series_64', 'real', b, 8, hl, '<i8 days', str(MPL / 'goog.npz'), "field 'date' as int64 day numbers; .npy wrap", sha((MPL / 'goog.npz').read_bytes()))
    b, hl = npy(g['volume'].astype('<i8')); add('goog_volume_i64.npy', 'series_64', 'real', b, 8, hl, '<i8', str(MPL / 'goog.npz'), "field 'volume'; .npy wrap", sha((MPL / 'goog.npz').read_bytes()))
    # derived: filesystem metadata of /usr in sorted walk order (machine-specific; the hashes identify the bytes used)
    sizes, mt, ino = [], [], []
    for root, dirs, files in os.walk('/usr'):
        dirs.sort()
        for f in sorted(files):
            try: st = os.lstat(os.path.join(root, f))
            except OSError: continue
            sizes.append(st.st_size); mt.append(st.st_mtime_ns); ino.append(st.st_ino)
            if len(sizes) == 200000: break
        if len(sizes) == 200000: break
    b, hl = npy(np.array(sizes, '<u4')); add('fs_size_u32.npy', 'fs_meta', 'derived', b, 4, hl, '<u4 bytes', '/usr lstat, sorted walk, first 200000 files', 'st_size; .npy wrap')
    b, hl = npy(np.array(mt, '<i8')); add('fs_mtime_i64.npy', 'fs_meta', 'derived', b, 8, hl, '<i8 ns', '/usr lstat, sorted walk, first 200000 files', 'st_mtime_ns; .npy wrap')
    b, hl = npy(np.array(ino, '<u8')); add('fs_inode_u64.npy', 'fs_meta', 'derived', b, 8, hl, '<u8', '/usr lstat, sorted walk, first 200000 files', 'st_ino; .npy wrap')
    # synthetic (labelled; reported separately): the planner-ownership .npy int32 random-walk fixture and outlier stress
    rng = np.random.default_rng(20261112)
    walk = np.cumsum(rng.integers(-10, 11, 300000)).astype('<i4'); b, hl = npy(walk)
    add('syn_walk_i32.npy', 'synthetic', 'synthetic', b, 4, hl, '<i4', 'numpy default_rng(20261112)', 'random walk, steps -10..10 (as the planner-ownership fixture kind 1)')
    sp = rng.integers(0, 64, 500000).astype('<u4'); idx = rng.choice(500000, 500, replace=False); sp[idx] = rng.integers(1 << 20, 1 << 31, 500)
    b, hl = npy(sp); add('syn_sparse_outliers_u32.npy', 'synthetic', 'synthetic', b, 4, hl, '<u4', 'numpy default_rng(20261112)', 'uniform 0..63 with 0.1% outliers in 2^20..2^31')
    cl = rng.integers(0, 16, 500000).astype('<u4')
    for s0 in rng.choice(500000 - 300, 40, replace=False): cl[s0:s0 + 300] = rng.integers(0, 1 << 24, 300)
    b, hl = npy(cl); add('syn_clustered_outliers_u32.npy', 'synthetic', 'synthetic', b, 4, hl, '<u4', 'numpy default_rng(20261112)', 'uniform 0..15 with 40 runs of 300 values in 0..2^24')
    m = dict(experiment='E02C', split='development only; no holdout', generator='research/codec/E02C-harness/corpus/build_corpus.py',
             numpy=np.__version__, inputs=items)
    pathlib.Path(manifest).write_text(json.dumps(m, indent=1) + '\n')
    print(f'{len(items)} inputs, {sum(i["bytes"] for i in items)} bytes')

if __name__ == '__main__': main(*sys.argv[1:3])
