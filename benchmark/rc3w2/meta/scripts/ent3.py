import sys, numpy as np
def H(x, k):
    c = np.bincount(x, minlength=k).astype(np.float64); c = c[c>0]; p = c/c.sum(); return -(p*np.log2(p)).sum()*len(x)/8
def Hc(x, ctx, k):
    j = ctx.astype(np.int64)*k + x; c = np.bincount(j).astype(np.float64); cc = np.bincount(ctx.astype(np.int64)).astype(np.float64)
    m = c>0; ci = np.nonzero(m)[0]//k
    return -(c[m]*np.log2(c[m]/cc[ci])).sum()/8
x = np.frombuffer(open(sys.argv[1],'rb').read(), '<u2').astype(np.int64); C = int(sys.argv[2])
e = (x>>7)&255; s = x>>15; m = x&127
print('n', len(x), 'H(e)', int(H(e,256)), 'H(e|col)', int(Hc(e, np.tile(np.arange(C), len(x)//C), 256)), 'H(e|prev)', int(Hc(e, np.concatenate([[0],e[:-1]]),256)),
 'H(s)', int(H(s,2)), 'H(m)', int(H(m,128)), 'H(m|e)', int(Hc(m,e,128)), 'H(s|e)', int(Hc(s,e,2)))
