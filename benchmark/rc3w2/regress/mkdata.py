import sys, os, csv, random, numpy as np, pandas as pd
D=sys.argv[1]; S=os.path.join(D,'set'); os.makedirs(S,exist_ok=True)
import shutil
for f in ['dickens','nci','webster','osdb','mr','x-ray','sao']: shutil.copy(f'{D}/silesia/{f}', f'{S}/{f}')
shutil.copy(f'{D}/owid-co2-data.csv', S); shutil.copy(f'{D}/countries-aggregated.csv', S)
def tail_rows(src, dst, frac):
    L=open(src).read().splitlines(True); n=len(L)-1; k=int(n*frac)
    open(dst,'w').write(L[0]+''.join(L[1+k:]))
tail_rows(f'{D}/owid-co2-data.csv', f'{S}/owid_test.csv', 0.25)
tail_rows(f'{D}/owid-energy-data.csv', f'{S}/energy_test.csv', 0.25)
e=pd.read_csv(f'{D}/owid-energy-data.csv')
num=e.drop(columns=['country','iso_code'])
m=num.to_numpy(dtype='<f8'); np.save(f'{S}/owid_energy_f64.npy', m)
num.astype('float64').to_pickle(f'{S}/owid_energy.pkl')
vars_=[c for c in e.columns if c not in ('country','iso_code','year')]
rows=[]
w=e.melt(id_vars=['country','year'], value_vars=vars_, var_name='variable').pivot_table(index=['variable','year'], columns='country', values='value', aggfunc='first')
w.to_csv(f'{S}/wide_test.csv', sep=';')
r=list(csv.DictReader(open(D+'/owid-co2-data.csv')))
y=np.array([int(x['year']) for x in r],dtype='<i4'); y.tofile(S+'/year.i32'); np.save(S+'/year_i32.npy',y)
p=np.array([int(float(x['population'])) for x in r if x['population']],dtype='<i8'); p.tofile(S+'/population.i64'); np.save(S+'/population_i64.npy',p)
rnd=random.Random(7); t=1700000000000; ts=[]
for i in range(8043):
    t+=rnd.choice([1,1,2,3,5,8,13,250,1000,1000,1001,999,60000]); ts.append(t)
np.array(ts,dtype='<i8').tofile(S+'/log_timestamps_ms.i64'); np.save(S+'/log_timestamps_ms_i64.npy', np.array(ts,dtype='<i8'))
b=np.frombuffer(open(D+'/mitdb100.dat','rb').read(),dtype=np.uint8).reshape(-1,3).astype(np.int32)
a=b[:,0]|((b[:,1]&15)<<8); c=b[:,2]|((b[:,1]>>4)<<8)
a=np.where(a>2047,a-4096,a); c=np.where(c>2047,c-4096,c)
np.stack([a,c],1).astype('<i2').tofile(S+'/mitdb-100.i16')
