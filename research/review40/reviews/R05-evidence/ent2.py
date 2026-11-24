import sys, numpy as np
def Hc(r, ctx, nctx):
    r = (r.astype(np.int64) & 255).ravel(); ctx = ctx.ravel().astype(np.int64)
    c = np.bincount(ctx*256 + r, minlength=nctx*256).reshape(nctx,256).astype(float); t = c.sum(1, keepdims=True)
    with np.errstate(divide='ignore', invalid='ignore'): l = np.where(c>0, c*np.log2(c/t), 0)
    return -l.sum()/8
def sh(a, dy, dx):  # value at (y-dy, x-dx), zero outside
    h, w = a.shape; o = np.zeros_like(a)
    ys = slice(dy, h) ; yd = slice(0, h-dy)
    if dx >= 0: o[dy:, dx:] = a[:h-dy, :w-dx]
    else: o[dy:, :w+dx] = a[:h-dy, -dx:]
    return o
def med(W, N, NW):
    g = W + N - NW; return np.where(NW >= np.maximum(W, N), np.minimum(W, N), np.where(NW <= np.minimum(W, N), np.maximum(W, N), g))
tot = [0, 0, 0, 0]
for f in sys.argv[1:]:
    raw = open(f,'rb').read(); parts = raw.split(None, 4)[:4]; w, h = int(parts[1]), int(parts[2]); c = 3 if parts[0]==b'P6' else 1; hl = len(raw) - w*h*c
    a = np.frombuffer(raw, np.uint8, offset=hl).reshape(h, w, c).astype(np.int32)
    if c == 3: planes = [a[...,1], a[...,0]-a[...,1], a[...,2]-a[...,1]]
    else: planes = [a[...,0]]
    h0 = ha = he = he2 = 0
    for X in planes:
        W, N, NW, NE = sh(X,0,1), sh(X,1,0), sh(X,1,1), sh(X,1,-1)
        r = X - med(W, N, NW); r8 = ((r + 128) & 255) - 128   # residual as int8
        z = np.abs(r8)
        act = np.abs(W-NW) + np.abs(N-NW) + np.abs(N-NE)
        e = sh(z,0,1) + sh(z,1,0) + sh(z,1,1) + sh(z,1,-1)       # residual magnitudes of W, N, NW, NE (decoder has them before reconstruction)
        e2 = 2*sh(z,0,1) + 2*sh(z,1,0) + sh(z,1,1) + sh(z,1,-1) + sh(z,0,2) + sh(z,2,0)
        q = [1,2,3,4,6,8,11,15,20,27,36,48,64,90,128]
        h0 += Hc(r, np.zeros_like(r), 1); ha += Hc(r, np.digitize(act, q), 16); he += Hc(r, np.digitize(e, q), 16); he2 += Hc(r, np.digitize(e2 // 2, q), 16)
    print('%-26s H0=%8d  H|grad-activity16=%8d (%+.1f%%)  H|resid-energy16=%8d (%+.1f%%)  H|resid-energy6nb=%8d (%+.1f%%)' % (f.split('/')[-1], h0, ha, 100*(ha/h0-1), he, 100*(he/h0-1), he2, 100*(he2/h0-1)), flush=True)
    for i, v in enumerate((h0, ha, he, he2)): tot[i] += v
print('TOTAL H0=%d act=%d (%+.1f%%) energy=%d (%+.1f%%) energy6=%d (%+.1f%%)' % (tot[0], tot[1], 100*(tot[1]/tot[0]-1), tot[2], 100*(tot[2]/tot[0]-1), tot[3], 100*(tot[3]/tot[0]-1)))
