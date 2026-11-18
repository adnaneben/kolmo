import json, struct, sys, numpy as np
def H(x, k):
    c = np.bincount(x, minlength=k).astype(np.float64); c = c[c>0]; p = c/c.sum(); return -(p*np.log2(p)).sum()*len(x)/8
def Hc(x, ctx, k):  # conditional entropy H(x|ctx), contexts quantised
    j = ctx.astype(np.int64)*k + x; c = np.bincount(j).astype(np.float64); cc = np.bincount(ctx.astype(np.int64)).astype(np.float64)
    m = c>0; ci = np.nonzero(m)[0]//k
    return -(c[m]*np.log2(c[m]/cc[ci])).sum()/8
p = sys.argv[1]; d = open(p,'rb').read(); n = struct.unpack('<Q', d[:8])[0]; h = json.loads(d[8:8+n]); h.pop('__metadata__',None); base=8+n
t = {'H':0,'prev':0,'up':0,'row':0,'col':0,'sm':0, 'smctx':0}
for k,v in h.items():
    if v['dtype'] not in ('BF16','F16') or len(v['shape'])!=2: continue
    a,b = v['data_offsets']; x = np.frombuffer(d[base+a:base+b], '<u2').astype(np.int64); R,C = v['shape']
    if v['dtype']=='BF16': e = (x>>7)&255; sm = ((x>>15)<<7)|(x&127)
    else: e = (x>>10)&31; sm = ((x>>15)<<10)|(x&1023)
    t['H'] += H(e,256); t['sm'] += H(sm, 2048)
    prev = np.concatenate([[0], e[:-1]]); t['prev'] += Hc(e, prev, 256)
    E2 = e.reshape(R,C)
    # context: mean exponent of the row so far? use row id quantised by median exponent of row
    rowm = np.median(E2, axis=1).astype(np.int64); t['row'] += Hc(e, np.repeat(rowm, C), 256)
    colm = np.median(E2, axis=0).astype(np.int64); t['col'] += Hc(e, np.tile(colm, R), 256)
    up = np.concatenate([np.zeros(C,np.int64), e[:-C]]); t['up'] += Hc(e, up, 256)
    t['smctx'] += Hc(sm & 127, e, 256)  # mantissa given exponent
print(p, {k:int(v) for k,v in t.items()})
