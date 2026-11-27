import numpy as np,glob,math,sys
def adaptive(ctx,xs,alpha=0.4,A=256):
    # KT-like: p = (c[x]+alpha)/(n+alpha*A) per context, adaptive
    cnt={}; tot={}; bits=0.0
    for c,x in zip(ctx,xs):
        d=cnt.setdefault(c,{}); n=tot.get(c,0); k=d.get(x,0)
        bits-=math.log2((k+alpha)/(n+alpha*A)); d[x]=k+1; tot[c]=n+1
    return bits/8
oz={'14739_PRCP':13423,'14739_TMAX':26169,'14739_TMIN':21937,'23174_PRCP':4151,'23174_TMAX':15836,'23174_TMIN':13451,'94728_PRCP':26170,'94728_TMAX':38645,'94728_TMIN':37357}
for f in sorted(glob.glob('*.npy')):
    a=np.load(f).astype(np.int64); v=(a-a.min()).tolist(); A=max(v)+1
    key=f[17:27]
    r1=adaptive([0]+v[:-1],v,0.05,A)            # value | previous value (exact), adaptive
    r2=adaptive([0]+[x>>2 for x in v[:-1]],v,0.05,A)
    r0=adaptive([0]*len(v),v,0.05,A)
    print(key,'oz %d  adapt order0 %.0f  order1(prev) %.0f  order1(prev>>2) %.0f'%(oz[key],r0,r1,r2))
