import sys, numpy as np
def H(r):  # order-0 entropy in bytes of residuals mod 256
    c = np.bincount((r.astype(np.int64) & 255).ravel(), minlength=256).astype(float); p = c[c>0]/c.sum()
    return -(c[c>0]*np.log2(p)).sum()/8
def Hc(r, ctx, nctx):  # conditional entropy given small context
    r = (r.astype(np.int64) & 255).ravel(); ctx = ctx.ravel().astype(np.int64)
    c = np.bincount(ctx*256 + r, minlength=nctx*256).reshape(nctx,256).astype(float)
    t = c.sum(1, keepdims=True); 
    with np.errstate(divide='ignore', invalid='ignore'):
        l = np.where(c>0, c*np.log2(c/t), 0)
    return -l.sum()/8
def nb(a):
    p = np.pad(a, ((2,0),(2,1)), mode='constant')
    W = p[2:, 1:-2]; N = p[1:-1, 2:-1]; NW = p[1:-1, 1:-2]; NE = p[1:-1, 3:]; WW = p[2:, :-3]; NN = p[:-2, 2:-1]
    return W, N, NW, NE, WW, NN
def med(W, N, NW):
    g = W + N - NW; return np.where(NW >= np.maximum(W, N), np.minimum(W, N), np.where(NW <= np.minimum(W, N), np.maximum(W, N), g))
def actq(W, N, NW, NE):
    a = np.abs(W-NW) + np.abs(N-NW) + np.abs(N-NE); return np.digitize(a, [3,8,16,32,64,128,256])
for f in sys.argv[1:]:
    raw = open(f,'rb').read(); 
    parts = raw.split(None, 4)[:4]; hl = len(raw) - (int(parts[1])*int(parts[2])*(3 if parts[0]==b'P6' else 1))
    w, h = int(parts[1]), int(parts[2]); c = 3 if parts[0]==b'P6' else 1
    a = np.frombuffer(raw, np.uint8, offset=hl).reshape(h, w, c).astype(np.int32)
    out = [f.split('/')[-1]]
    # (a) per-channel MED, order-0 and act-conditional
    tot0 = tota = 0
    for k in range(c):
        W,N,NW,NE,WW,NN = nb(a[...,k]); r = a[...,k] - med(W,N,NW); tot0 += H(r); tota += Hc(r, actq(W,N,NW,NE), 8)
    out += ['MED/ch H0=%d' % tot0, 'MED/ch H|act=%d' % tota]
    if c == 3:
        R, G, B = a[...,0], a[...,1], a[...,2]
        # (b) file order R,G,B with inter-channel left prediction icp = W + P1 - W1 (in difference domain: left)
        tb = 0; tg = 0; tm = 0
        W,N,NW,NE,WW,NN = nb(R); r0 = R - med(W,N,NW); a0 = actq(W,N,NW,NE); base = Hc(r0, a0, 8)
        for X, P in ((G, R), (B, G)):
            D = X - P; W,N,NW,NE,WW,NN = nb(D); aq = actq(W,N,NW,NE)
            tb += Hc(X - np.clip(P + W, 0, 255), aq, 8)
            tg += Hc(X - np.clip(P + W + N - NW, 0, 255), aq, 8)
            tm += Hc(X - np.clip(P + med(W,N,NW), 0, 255), aq, 8)
        out += ['R med + diff-left=%d' % (base+tb), 'diff-grad=%d' % (base+tg), 'diff-med=%d' % (base+tm)]
        # (c) G first then R-G, B-G med
        t = 0
        for X in (G, R-G, B-G):
            W,N,NW,NE,WW,NN = nb(X); t += Hc(X - med(W,N,NW), actq(W,N,NW,NE), 8)
        out += ['G,R-G,B-G med H|act=%d' % t]
        Co = R - B; tmp = B + (Co >> 1); Cg = G - tmp; Y = tmp + (Cg >> 1); t = 0
        for X in (Y, Co, Cg):
            W,N,NW,NE,WW,NN = nb(X); r = X - med(W,N,NW); 
            # 9-bit residuals for Co/Cg: entropy over full range
            rr = (r - r.min()).ravel(); aq = actq(W,N,NW,NE).ravel(); nv = int(rr.max())+1
            cnt = np.bincount(aq*nv + rr, minlength=8*nv).reshape(8,nv).astype(float); tt = cnt.sum(1, keepdims=True)
            with np.errstate(divide='ignore', invalid='ignore'): l = np.where(cnt>0, cnt*np.log2(cnt/tt), 0)
            t += -l.sum()/8
        out += ['YCoCg med H|act=%d' % t]
    print(' | '.join(out), flush=True)
