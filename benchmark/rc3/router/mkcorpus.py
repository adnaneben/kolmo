#!/usr/bin/env python3
"""Build the routing corpus: one directory per input type, from the public downloads in RDATA (each source in its own
directory, untrusted: only read, never executed). Run with python3 -I.
usage: python3 -I mkcorpus.py RDATA OUT"""
import gzip, io, json, os, struct, sys, tarfile
import numpy as np

R, O = sys.argv[1], sys.argv[2]
X = os.path.join(R, 'x')
MB = 1 << 20


def put(typ, name, data):
    d = os.path.join(O, typ)
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, name), 'wb') as f:
        f.write(data)


def rd(*p, n=None, off=0):
    with open(os.path.join(*p), 'rb') as f:
        f.seek(off)
        return f.read() if n is None else f.read(n)


def cut_lines(b, n):
    b = b[:n]
    return b[:b.rfind(b'\n') + 1]


S = os.path.join(X, 'silesia')
# prose, markup, source, executables, binary databases (Silesia, enwik8)
put('prose', 'dickens', rd(S, 'dickens'))
put('prose', 'reymont', rd(S, 'reymont'))
put('prose', 'webster16', cut_lines(rd(S, 'webster'), 16 * MB))
put('xml', 'xml', rd(S, 'xml'))
put('xml', 'enwik8_16', cut_lines(rd(X, 'enwik8', 'enwik8'), 16 * MB))
put('source', 'samba16', rd(S, 'samba', n=16 * MB))
put('exe', 'ooffice', rd(S, 'ooffice'))
put('exe', 'mozilla16', rd(S, 'mozilla', n=16 * MB))
put('other', 'osdb', rd(S, 'osdb'))
put('other', 'nci16', cut_lines(rd(S, 'nci'), 16 * MB))
put('float', 'sao', rd(S, 'sao'))
put('img16', 'x-ray', rd(S, 'x-ray'))
put('img16', 'mr', rd(S, 'mr'))

# tables (OWID, covid)
co2 = rd(R, 'owid', 'owid-co2-data.csv')
put('csv_wide', 'owid-co2.csv', co2)
put('csv_wide', 'owid-energy.csv', rd(R, 'owid', 'owid-energy-data.csv'))
put('csv_long', 'covid-countries.csv', rd(R, 'owid', 'countries-aggregated.csv'))
rows = [l.split(b',') for l in co2.splitlines()]
put('tsv', 'owid-co2.tsv', b'\n'.join(b'\t'.join(r) for r in rows) + b'\n')
# a long table of few columns: country,year,one value (melted from the CO2 table)
hdr = rows[0]; ci = hdr.index(b'co2'); pi = hdr.index(b'population')
put('csv_long', 'owid-co2-long.csv', b'country,year,variable,value\n' + b''.join(
    b'%s,%s,%s,%s\n' % (r[0], r[1], hdr[j], r[j]) for r in rows[1:] if len(r) == len(hdr) for j in (ci, pi) if r[j]))

# numeric arrays derived from the CO2 table
def col(name, typ):
    j = hdr.index(name); v = []
    for r in rows[1:]:
        if len(r) == len(hdr):
            try: v.append(typ(r[j]) if r[j] else (0 if typ is int else np.nan))
            except ValueError: v.append(0 if typ is int else np.nan)
    return v

def npy(a):
    b = io.BytesIO(); np.save(b, a); return b.getvalue()

put('npy', 'co2_f64.npy', npy(np.array(col(b'co2', float), dtype='<f8')))
put('npy', 'population_i64.npy', npy(np.nan_to_num(np.array(col(b'population', float))).astype('<i8')))
put('npy', 'year_i32.npy', npy(np.array(col(b'year', int), dtype='<i4')))
g = np.nan_to_num(np.array(col(b'gdp', float)))
put('npy', 'gdp_per_cap_f32.npy', npy((g / np.maximum(np.nan_to_num(np.array(col(b'population', float))), 1)).astype('<f4')))
put('raw_int', 'population.i64', np.nan_to_num(np.array(col(b'population', float))).astype('<i8').tobytes())
put('raw_float', 'co2.f64', np.array(col(b'co2', float), dtype='<f8').tobytes())

# logs
put('logs', 'apache.log', rd(X, 'loghub_Apache', 'Apache.log'))
put('logs', 'linux.log', rd(X, 'loghub_Linux', 'Linux.log'))
put('logs', 'ssh16.log', cut_lines(rd(X, 'loghub_SSH', 'SSH.log'), 16 * MB))
if os.path.exists(os.path.join(X, 'loghub_HDFS', 'HDFS.log')):
    put('logs', 'hdfs16.log', cut_lines(rd(X, 'loghub_HDFS', 'HDFS.log'), 16 * MB))

# JSON from GH Archive (JSON lines, real); pretty and minified forms of the same records
gh = os.path.join(R, 'gharchive', '2015-01-01-15.json.gz')
if os.path.exists(gh):
    jl = cut_lines(gzip.open(gh).read(), 16 * MB)
    put('jsonl', 'gharchive16.jsonl', jl)
    recs = [json.loads(l) for l in jl.splitlines()[:4000]]
    put('json', 'gharchive_pretty.json', json.dumps(recs, indent=2).encode())
    put('json', 'gharchive_min.json', json.dumps(recs, separators=(',', ':')).encode())

# images: Kodak PNG -> PPM (8-bit RGB) and PGM (grey); a JPEG for the already-compressed group
from PIL import Image
K = os.path.join(R, 'kodak')
for i in ('01', '05', '08', '12'):
    im = Image.open(os.path.join(K, 'kodim%s.png' % i)).convert('RGB')
    b = io.BytesIO(); im.save(b, format='PPM'); put('img8', 'kodim%s.ppm' % i, b.getvalue())
im = Image.open(os.path.join(K, 'kodim03.png'))
b = io.BytesIO(); im.convert('L').save(b, format='PPM'); put('img8', 'kodim03_grey.pgm', b.getvalue())
b = io.BytesIO(); im.convert('RGB').save(b, format='JPEG', quality=90); put('compressed', 'kodim03.jpg', b.getvalue())
put('compressed', 'kodim02.png', rd(K, 'kodim02.png'))
put('compressed', 'apache.tar.gz', rd(R, 'loghub', 'Apache.tar.gz'))

# audio and video
E = os.path.join(R, 'esc50'); wavs = sorted(f for f in os.listdir(E) if f.endswith('.wav'))
for f in wavs[:5]:
    put('audio', f, rd(E, f))
# the clips joined as one mono WAV (PCM data of each clip after its 44-byte header)
pcm = b''.join(rd(E, f)[44:] for f in wavs)
put('audio', 'esc_joined.wav', b'RIFF' + struct.pack('<I', 36 + len(pcm)) + b'WAVEfmt ' +
    struct.pack('<IHHIIHH', 16, 1, 1, 44100, 88200, 2, 16) + b'data' + struct.pack('<I', len(pcm)) + pcm)
for f in ('akiyo_cif.y4m', 'foreman_cif.y4m'):
    put('video', f, rd(R, 'xiph', f))

# genomics
fq = os.path.join(R, 'ena', 'SRR062634_1.50MB.fastq')
if os.path.exists(fq):
    b = rd(fq, n=24 * MB); k = b.rfind(b'\n@'); put('fastq', 'SRR062634_24M.fastq', b[:k + 1])
fa = os.path.join(R, 'fasta', 'chr22.fa.gz')
if os.path.exists(fa):
    b = gzip.open(fa).read(); put('fasta', 'chr22_16M.fa', b[:60] + cut_lines(b[20 * MB:], 16 * MB))

# model weights: MiniLM f32, and the same tensors in bf16 (top halves)
W = os.path.join(R, 'weights', 'model.safetensors')
if os.path.exists(W):
    b = rd(W); hl = struct.unpack('<Q', b[:8])[0]; head = b[:8 + hl]
    put('weights', 'minilm_f32_24M.safetensors', b[:24 * MB])
    body = np.frombuffer(b[8 + hl:len(b) - (len(b) - 8 - hl) % 4], dtype='<u4')
    put('weights', 'minilm_bf16.safetensors', head + (body >> 16).astype('<u2').tobytes())

# an archive of mixed members
def tar_of(members):
    b = io.BytesIO()
    with tarfile.open(fileobj=b, mode='w', format=tarfile.USTAR_FORMAT) as t:
        for name, data in members:
            ti = tarfile.TarInfo(name); ti.size = len(data); ti.mtime = 0; t.addfile(ti, io.BytesIO(data))
    return b.getvalue()

put('archive', 'mixed.tar', tar_of([
    ('dickens.txt', rd(S, 'dickens', n=3 * MB)), ('owid-co2.csv', co2[:3 * MB]),
    ('kodim01.ppm', rd(O, 'img8', 'kodim01.ppm')), ('clip.wav', rd(E, wavs[0])),
    ('ooffice.dll', rd(S, 'ooffice', n=3 * MB)), ('apache.log', rd(X, 'loghub_Apache', 'Apache.log', n=2 * MB)),
    ('x-ray.raw', rd(S, 'x-ray', n=3 * MB))]))
print('corpus written to', O)
