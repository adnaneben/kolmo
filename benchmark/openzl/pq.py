import struct, numpy as np, pandas as pd
def uv(x):
    o=bytearray()
    while True:
        b=x&127; x>>=7
        if x: o.append(b|128)
        else: o.append(b); return bytes(o)
def zz(x): return uv((x<<1)^(x>>63))
class S:
    def __init__(s): s.b=bytearray(); s.last=[0]
    def fh(s,fid,t):
        d=fid-s.last[-1]
        if 0<d<16: s.b.append((d<<4)|t)
        else: s.b.append(t); s.b+=zz(fid)
        s.last[-1]=fid
    def i32(s,fid,v): s.fh(fid,5); s.b+=zz(v)
    def i64(s,fid,v): s.fh(fid,6); s.b+=zz(v)
    def bin(s,fid,v): s.fh(fid,8); s.b+=uv(len(v))+v
    def begin(s,fid): s.fh(fid,12); s.last.append(0)
    def end(s): s.b.append(0); s.last.pop()
    def lst(s,fid,et,n): s.fh(fid,9); s.b.append((n<<4)|et if n<15 else 0xF0|et); (s.b.extend(uv(n)) if n>=15 else None)
    def lbegin(s): s.last.append(0)   # struct inside list
def write(df, path):
    out=bytearray(b'PAR1'); cols=[]
    n=len(df)
    for name in df.columns:
        col=df[name]
        if not pd.api.types.is_numeric_dtype(col):
            vals=b''.join(struct.pack('<I',len(x.encode()))+x.encode() for x in col.astype(str)); ty=6
        elif pd.api.types.is_integer_dtype(col): vals=col.to_numpy(np.int32).tobytes(); ty=1
        else: vals=col.to_numpy(np.float64).tobytes(); ty=5
        ph=S(); ph.i32(1,0); ph.i32(2,len(vals)); ph.i32(3,len(vals)); ph.begin(5); ph.i32(1,n); ph.i32(2,0); ph.i32(3,3); ph.i32(4,3); ph.end(); ph.b.append(0)
        off=len(out); out+=ph.b+vals
        cols.append((name,ty,off,len(ph.b)+len(vals)))
    fm=S(); fm.i32(1,1)
    fm.lst(2,12,len(cols)+1)
    fm.lbegin(); fm.bin(4,b'schema'); fm.i32(5,len(cols)); fm.end()
    for name,ty,off,sz in cols:
        fm.lbegin(); fm.i32(1,ty); fm.i32(3,0); fm.bin(4,name.encode()); fm.end()
    fm.i64(3,n)
    fm.lst(4,12,1); fm.lbegin()
    fm.lst(1,12,len(cols))
    for name,ty,off,sz in cols:
        fm.lbegin(); fm.i64(2,off); fm.begin(3)
        fm.i32(1,ty); fm.lst(2,5,1); fm.b+=zz(0); fm.lst(3,8,1); fm.b+=uv(len(name.encode()))+name.encode()
        fm.i32(4,0); fm.i64(5,n); fm.i64(6,sz); fm.i64(7,sz); fm.i64(9,off); fm.end(); fm.end()
    fm.i64(2,sum(c[3] for c in cols)); fm.i64(3,n); fm.end()
    fm.b.append(0)
    out+=fm.b+struct.pack('<I',len(fm.b))+b'PAR1'
    open(path,'wb').write(out)
df=pd.read_csv('/tmp/ozl/e/energy-data-master/owid-energy-data.csv')
df['country']=df['country'].astype(str); df['iso_code']=df['iso_code'].fillna('').astype(str)
write(df,'/tmp/ozp/data/owid_energy.parquet')
co=pd.read_csv('/tmp/czfz/data3/owid-co2-data.csv'); co['country']=co['country'].astype(str); co['iso_code']=co['iso_code'].fillna('').astype(str)
write(co,'/tmp/ozp/data/owid_co2.parquet')
import os; print(os.path.getsize('/tmp/ozp/data/owid_energy.parquet'), os.path.getsize('/tmp/ozp/data/owid_co2.parquet'))
