"""Inputs of the gpu2 measurements, built from the benchmark set (read only) into OUT.

python3 -I benchmark/rc3w2/gpu2/mkdata.py DATA_DIR VIDEO_CIF_Y4M OUT_DIR

  owid8.csv       owid-co2-data.csv, its rows 8 times (one header): 115 MB, 79 columns
  energy16.csv    energy_test.csv, its rows 16 times: 111 MB, 130 columns (wide)
  owid.jsonl      owid-co2-data.csv as JSON lines (every key on every line, null when empty): 114 MB
  mix{1,16,64,256,512}.bin   prefixes of all benchmark files concatenated (sorted by name)
  text256.bin     distinct text and DNA (webster, mozilla, chr22.fa, nci, ld30.txt, samba, dickens, ...), 256 MB cap
  bf16x5.bin      llm_bf16_47mb.safetensors 5 times (236 MB)
  owid_{1,4,16,64}mb.csv, energy_k{8,16,32,64}.csv   smaller tables (prefixes; first K columns)
  v4k.y4m, v1080.y4m   3840x2160 and 1920x1080 4:2:0, 16 frames: the CIF clip upscaled (bilinear) with fixed-seed noise (+-2)
The video step needs numpy (a venv); the rest is the standard library.
"""
import csv, io, json, os, sys

data, cif, out = sys.argv[1], sys.argv[2], sys.argv[3]
os.makedirs(out, exist_ok=True)


def rd(name):
    with open(os.path.join(data, name), 'rb') as f:
        return f.read()


def repeat_rows(name, times, dst):
    b = rd(name)
    head, _, body = b.partition(b'\n')
    if not body.endswith(b'\n'):
        body += b'\n'
    with open(os.path.join(out, dst), 'wb') as f:
        f.write(head + b'\n')
        for _ in range(times):
            f.write(body)


repeat_rows('owid-co2-data.csv', 8, 'owid8.csv')
repeat_rows('energy_test.csv', 16, 'energy16.csv')

rows = list(csv.reader(io.StringIO(rd('owid-co2-data.csv').decode('utf-8'))))
keys = rows[0]
with open(os.path.join(out, 'owid.jsonl'), 'w', encoding='utf-8') as f:
    lines = []
    for r in rows[1:]:
        o = {}
        for k, v in zip(keys, r):
            if v == '':
                o[k] = None
            else:
                try:
                    o[k] = int(v) if v.lstrip('-').isdigit() else float(v) if any(c.isdigit() for c in v) and v.replace('.', '', 1).lstrip('-').isdigit() else v
                except ValueError:
                    o[k] = v
        lines.append(json.dumps(o, separators=(',', ':')))
    s = '\n'.join(lines) + '\n'
    f.write(s)

# smaller tables for the break-even: prefixes of owid8.csv at line ends; energy_test.csv with its first K columns
b8 = open(os.path.join(out, 'owid8.csv'), 'rb').read()
for mb in (1, 4, 16, 64):
    e = b8.index(b'\n', mb << 20) + 1
    with open(os.path.join(out, 'owid_%dmb.csv' % mb), 'wb') as f:
        f.write(b8[:e])
for k in (8, 16, 32, 64):
    with open(os.path.join(out, 'energy_k%d.csv' % k), 'wb') as f:
        for line in rd('energy_test.csv').split(b'\n'):
            if line:
                f.write(b','.join(line.split(b',')[:k]) + b'\n')

names = sorted(n for n in os.listdir(data) if os.path.isfile(os.path.join(data, n)))
mix = b''.join(rd(n) for n in names)
for mb in (1, 16, 64, 256, 512):
    with open(os.path.join(out, 'mix%d.bin' % mb), 'wb') as f:
        m = mb << 20
        f.write((mix * (m // len(mix) + 1))[:m] if m > len(mix) else mix[:m])

txt = b''.join(rd(n) for n in ('webster', 'mozilla', 'chr22.fa', 'nci', 'ld30.txt', 'samba', 'dickens', 'mr', 'osdb',
                                'reymont', 'xml', 'owid-co2-data.csv', 'owid_arr.json', 'loghub_all.log', 'api.json',
                                'ooffice', 'x-ray', 'sao', 'energy_test.csv', 'wide_test.csv'))
with open(os.path.join(out, 'text256.bin'), 'wb') as f:
    f.write(txt[:256 << 20])

w = rd('llm_bf16_47mb.safetensors')
with open(os.path.join(out, 'bf16x5.bin'), 'wb') as f:
    for _ in range(5):
        f.write(w)

# 4K video: CIF frames upscaled bilinearly (integer weights), plus fixed-seed noise
with open(cif, 'rb') as f:
    y4m = f.read()
hdr, _, rest = y4m.partition(b'\n')
W = int([t for t in hdr.split() if t.startswith(b'W')][0][1:])
H = int([t for t in hdr.split() if t.startswith(b'H')][0][1:])
fs = W * H * 3 // 2
frames = []
p = 0
while p < len(rest) and len(frames) < 16:
    nl = rest.index(b'\n', p)
    frames.append(rest[nl + 1:nl + 1 + fs])
    p = nl + 1 + fs
import numpy as np  # noqa: E402  (run with a venv that has numpy)
rng = np.random.default_rng(20261117)


def upscale(plane, w, h, ow, oh):
    a = np.frombuffer(plane, dtype=np.uint8).reshape(h, w).astype(np.int64)
    fx = (np.arange(ow) * (w - 1) * 256) // max(ow - 1, 1)
    fy = (np.arange(oh) * (h - 1) * 256) // max(oh - 1, 1)
    x0, wx = fx >> 8, fx & 255
    y0, wy = fy >> 8, fy & 255
    x1, y1 = np.minimum(x0 + 1, w - 1), np.minimum(y0 + 1, h - 1)
    top = a[y0][:, x0] * (256 - wx) + a[y0][:, x1] * wx
    bot = a[y1][:, x0] * (256 - wx) + a[y1][:, x1] * wx
    v = (top * (256 - wy)[:, None] + bot * wy[:, None] + 32768) >> 16
    v = v + rng.integers(-2, 3, size=v.shape)
    return np.clip(v, 0, 255).astype(np.uint8).tobytes()


for name, OW, OH in (('v4k.y4m', 3840, 2160), ('v1080.y4m', 1920, 1080)):
    with open(os.path.join(out, name), 'wb') as f:
        f.write(b'YUV4MPEG2 W%d H%d F30:1 Ip A1:1 C420jpeg\n' % (OW, OH))
        for fr in frames:
            y = fr[:W * H]
            u = fr[W * H:W * H + W * H // 4]
            v = fr[W * H + W * H // 4:]
            f.write(b'FRAME\n')
            f.write(upscale(y, W, H, OW, OH))
            f.write(upscale(u, W // 2, H // 2, OW // 2, OH // 2))
            f.write(upscale(v, W // 2, H // 2, OW // 2, OH // 2))
print('done')
