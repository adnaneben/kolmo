import sys, csv, collections
def load(f):
    d={}
    for r in csv.reader(open(f)):
        d[(r[0],r[1])]=(int(r[2]),float(r[3]),float(r[4]),r[5])
    return d
a=load(sys.argv[1]); b=load(sys.argv[2]); show=len(sys.argv)<4
tot=collections.defaultdict(lambda:[0,0,0.0,0.0,0.0,0.0,0,0])
for k in sorted(a):
    if k not in b: continue
    x,y=a[k],b[k]; t=tot[k[1]]; t[0]+=x[0]; t[1]+=y[0]; t[2]+=x[1]; t[3]+=y[1]; t[4]+=x[2]; t[5]+=y[2]; t[6]+=y[0]<x[0]; t[7]+=y[0]>x[0]
    if show and x[0]!=y[0]: print('%-26s %-12s %11d %11d %+7.3f%%  enc %.2f->%.2fs dec %.2f->%.2fs'%(k[0],k[1],x[0],y[0],100*(y[0]-x[0])/x[0],x[1],y[1],x[2],y[2]))
for m,t in tot.items(): print('TOTAL %-12s %12d %12d %+7.3f%%  smaller %d larger %d  enc %.1f->%.1fs dec %.1f->%.1fs'%(m,t[0],t[1],100*(t[1]-t[0])/t[0],t[6],t[7],t[2],t[3],t[4],t[5]))
