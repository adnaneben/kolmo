#!/usr/bin/env python3
"""Print what a Kolmo frame chose per block: codec, tflag, plan prefix (t, a, b, c). Summarised by bytes.
usage: frameinfo.py FRAME [--blocks]"""
import struct, sys, collections
CODEC = {0:'store',1:'lz4',2:'lz4hc',3:'zstd',4:'brotli',5:'bsc',6:'lzma2',7:'cm',12:'graph',16:'fqfam',17:'cmimg',
         18:'bwt',19:'aud',20:'vid',25:'cmpar',26:'gr6'}
PLAN = {0:'none',1:'tp',2:'shuf',3:'tbl',4:'log',5:'img16',6:'rec',7:'x86',8:'word',9:'kg',10:'lines'}
def kg_kind(b):
    if b in (64, 66): return 'dec64tab'
    if b == 65: return 'dec64'
    if b & 32: return 'img8'
    if b & 16: return 'float%d' % (b & 15)
    return 'int%d' % b
def info(path, blocks=False):
    d = open(path, 'rb').read()
    if d[:4] != b'CZN1': return {'?': len(d)}, None
    ver, mode = d[4], d[5]; nb = struct.unpack_from('<I', d, 20)[0]
    off = 32 + 16 * nb; agg = collections.Counter(); rows = []
    for i in range(nb):
        raw, comp, _, codec, tflag = struct.unpack_from('<IIIBB', d, 32 + 16 * i)
        t, a, b, c = d[off:off + 4] if comp >= 4 else (0, 0, 0, 0)
        name = CODEC.get(codec, str(codec))
        if codec in (19, 20, 26, 16): key = name
        else:
            p = PLAN.get(t, str(t))
            if t == 9: p = 'kg:' + kg_kind(b)
            if t == 4 and a: p = 'log/json%d' % a
            key = '%s+%s/tf%d' % (p, name, tflag) if p != 'none' or tflag else name
        agg[key] += raw; rows.append((raw, comp, key)); off += comp
    return agg, (ver, mode, rows)
if __name__ == '__main__':
    agg, meta = info(sys.argv[1], '--blocks' in sys.argv)
    tot = sum(agg.values()) or 1
    print('v%d' % meta[0] if meta else '', ' '.join('%s:%.0f%%' % (k, 100 * v / tot) for k, v in agg.most_common()))
