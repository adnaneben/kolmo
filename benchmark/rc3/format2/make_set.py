# make_set.py DATA : builds DATA/set/ (typed arrays, ECG, OWID float arrays, sensor-like and sparse arrays) from the files
# fetch.sh downloaded into DATA (Silesia unzipped into DATA/silesia). The CSVs, the MiniLM weights (as minilm_f32.safetensors)
# and Silesia dickens, osdb, nci, samba, xml and sao are copied into DATA/set/ as they are.
import numpy as np, csv, sys, os, random
D=sys.argv[1]; S=os.path.join(D,'set')
def npy(name, a): np.save(os.path.join(S,name), a)
mr=np.frombuffer(open(D+'/silesia/mr','rb').read()[:9970564//2*2],dtype='<u2'); npy('mr_u16.npy',mr)
xr=np.frombuffer(open(D+'/silesia/x-ray','rb').read(),dtype='<u2'); npy('xray_u16.npy',xr)
def mit(f):
    b=np.frombuffer(open(f,'rb').read(),dtype=np.uint8).reshape(-1,3).astype(np.int32)
    a=b[:,0]|((b[:,1]&15)<<8); c=b[:,2]|((b[:,1]>>4)<<8)
    a=np.where(a>2047,a-4096,a); c=np.where(c>2047,c-4096,c)
    return np.stack([a,c],1)
e=mit(D+'/mitdb100.dat')
e.astype('<i2').tofile(S+'/mitdb-100.i16'); e[:,0].astype('<i2').tofile(S+'/mitdb-100-ch0.i16'); e[:,1].astype('<i2').tofile(S+'/mitdb-100-ch1.i16'); e[:,0].astype('<i4').tofile(S+'/mitdb-100-ch0.i32')
npy('ecg_2ch_i16.npy', e.astype('<i2')); npy('ecg101_ch0_i16.npy', mit(D+'/mitdb101.dat')[:,0].astype('<i2'))
r=list(csv.DictReader(open(D+'/owid-co2-data.csv')))
y=np.array([int(x['year']) for x in r],dtype='<i4'); y.tofile(S+'/year.i32'); npy('year_i32.npy',y)
p=np.array([int(float(x['population'])) for x in r if x['population']],dtype='<i8'); p.tofile(S+'/population.i64'); npy('population_i64.npy',p)
rnd=random.Random(7); t=1700000000000; ts=[]
for i in range(8043):
    t+=rnd.choice([1,1,2,3,5,8,13,250,1000,1000,1001,999,60000]); ts.append(t)
np.array(ts,dtype='<i8').tofile(S+'/log_timestamps_ms.i64')
co2=np.array([float(x['co2']) if x['co2'] else np.nan for x in r]); npy('owid_co2_f64.npy',co2)
er=list(csv.DictReader(open(D+'/owid-energy-data.csv'))); keys=[k for k in er[0] if k not in('country','iso_code','year')][:16]
m=np.array([[float(x[k]) if x[k] else np.nan for k in keys] for x in er]); npy('owid_energy_f64.npy',m)
# sensor-like: temperature in centi-degrees with slow drift (i32), and sparse counter (i64)
t=np.cumsum(np.random.default_rng(1).integers(-3,4,200000)).astype('<i4')+2000; npy('sensor_temp_i32.npy',t)
sp=np.zeros(300000,dtype='<i64' if False else '<i8'); idx=np.random.default_rng(2).choice(300000,3000,replace=False); sp[idx]=np.random.default_rng(3).integers(1,1<<40,3000); npy('sparse_i64.npy',sp)
