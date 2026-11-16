import random, struct, sys
R=random.Random(5); O=sys.argv[1]
def row(i):
    return [str(i), str(1700000000+i*3), str(R.randint(0,500)), '%.2f'%R.uniform(0,100), R.choice(['aa','bb','cc'])]
L=['id,ts,q,p,s']
for i in range(40000):
    r=row(i)
    if i%97==5: r=r[:3]
    elif i%89==3: r=r+['extra']
    L.append(','.join(r))
open(O+'/ragged_1m.csv','w').write('\n'.join(L)+'\n')
L=['id,name,p,q']
for i in range(40000):
    L.append('%d,"%s, %s",%.2f,%d'%(i,R.choice(['Smith','Jones']),R.choice(['A','B']),R.uniform(0,100),R.randint(0,99)))
open(O+'/quotedc_1m.csv','w').write('\n'.join(L)+'\n')
L=[]
for i in range(400):
    L.append(','.join(''.join(R.choice('abcdefghij') for _ in range(R.randint(1,6))) for j in range(300)))
open(O+'/widetext_600k.csv','w').write('\n'.join(L)+'\n')
L=[]
for i in range(300):
    L.append('\t'.join(R.choice(['on','off','n/a','x%d'%R.randint(0,9)]) for j in range(270)))
open(O+'/widetext_tab.tsv','w').write('\n'.join(L)+'\n')
def arr(fmt, gen, n): return b''.join(struct.pack(fmt, gen(i)) for i in range(n))
v=[0]
def walk(step, lo, hi):
    def g(i):
        v[0]=max(lo,min(hi,v[0]+R.randint(-step,step))); return v[0]
    return g
v[0]=0; open(O+'/i32_s1.bin','wb').write(arr('<i', walk(60,-2**31,2**31-1), 300000))
v[0]=0; open(O+'/i32_s2.bin','wb').write(arr('<i', walk(20000,-2**31,2**31-1), 300000))
v[0]=0; open(O+'/i32_s4.bin','wb').write(arr('<i', lambda i: R.randint(-2**30,2**30), 300000))
v[0]=10**6; open(O+'/u32_s1.bin','wb').write(arr('<I', walk(60,0,2**32-1), 300000))
v[0]=0; open(O+'/i64_s1.bin','wb').write(arr('<q', walk(60,-2**62,2**62), 150000))
v[0]=0; open(O+'/i64_s2.bin','wb').write(arr('<q', walk(20000,-2**62,2**62), 150000))
v[0]=0; open(O+'/i64_s4.bin','wb').write(arr('<q', walk(2**30,-2**62,2**62), 150000))
open(O+'/i64_s8.bin','wb').write(arr('<q', lambda i: R.randint(-2**62,2**62), 150000))
open(O+'/u64_ts.bin','wb').write(arr('<Q', lambda i: 1700000000000000+i*1000+R.randint(0,50), 150000))
v[0]=0; open(O+'/i16_s1.bin','wb').write(arr('<h', walk(60,-32768,32767), 400000))
v[0]=0; open(O+'/u16_s2.bin','wb').write(arr('<H', walk(3000,0,65535), 400000))
v[0]=0; open(O+'/i32_be.bin','wb').write(arr('>i', walk(60,-2**31,2**31-1), 300000))
