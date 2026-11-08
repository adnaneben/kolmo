"""Build safetensors and GGUF files from the float32 initializers of a real ONNX model (magika standard_v3_3)."""
import struct, json, sys
src, D = sys.argv[1], sys.argv[2]
b = open(src, 'rb').read()
def varint(b, i):
    r = 0; s = 0
    while True:
        c = b[i]; i += 1; r |= (c & 127) << s
        if c < 128: return r, i
        s += 7
def fields(b):
    i = 0
    while i < len(b):
        k, i = varint(b, i); f, w = k >> 3, k & 7
        if w == 0: v, i = varint(b, i)
        elif w == 1: v = b[i:i + 8]; i += 8
        elif w == 5: v = b[i:i + 4]; i += 4
        elif w == 2: l, i = varint(b, i); v = b[i:i + l]; i += l
        else: raise ValueError(w)
        yield f, w, v
graph = [v for f, w, v in fields(b) if f == 7][0]
ts = []
for f, w, v in fields(graph):
    if f != 5: continue
    dims = []; dt = None; name = b''; raw = b''
    for ff, ww, vv in fields(v):
        if ff == 1:
            if ww == 0: dims.append(vv)
            else:
                j = 0
                while j < len(vv): x, j = varint(vv, j); dims.append(x)
        elif ff == 2: dt = vv
        elif ff == 8: name = vv
        elif ff == 9: raw = vv
    if dt == 1 and raw: ts.append((name.decode(), dims, raw))
hdr = {}; off = 0; data = b''
for name, dims, raw in ts:
    hdr[name] = {'dtype': 'F32', 'shape': dims, 'data_offsets': [off, off + len(raw)]}; off += len(raw); data += raw
h = json.dumps(hdr).encode(); h += b' ' * ((8 - len(h) % 8) % 8)
open(D + '/magika.safetensors', 'wb').write(struct.pack('<Q', len(h)) + h + data)
def gstr(s): s = s.encode(); return struct.pack('<Q', len(s)) + s
out = b'GGUF' + struct.pack('<IQQ', 3, len(ts), 1) + gstr('general.architecture') + struct.pack('<I', 8) + gstr('magika')
off = 0; infos = b''; datas = []
for name, dims, raw in ts:
    infos += gstr(name) + struct.pack('<I', len(dims)) + b''.join(struct.pack('<Q', d) for d in reversed(dims)) + struct.pack('<IQ', 0, off)
    pad = (-len(raw)) % 32; datas.append(raw + b'\0' * pad); off += len(raw) + pad
out += infos; out += b'\0' * ((-len(out)) % 32); out += b''.join(datas)
open(D + '/magika.gguf', 'wb').write(out)
