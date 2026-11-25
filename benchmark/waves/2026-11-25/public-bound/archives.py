"""Positive DDP1 bound and cross-reader verification, including a shared chunk and empty file."""
import csv, ctypes as c, gzip, hashlib, random, sys
from pathlib import Path
class Params(c.Structure):
    _fields_=[("mode",c.c_int),("level",c.c_int),("threads",c.c_int),("block",c.c_uint32),
              ("transforms",c.c_int),("limit",c.c_uint64)]
class File(c.Structure):
    _fields_=[("name",c.c_char_p),("data",c.c_char_p),("size",c.c_size_t),("mode",c.c_uint32),("mtime",c.c_int64)]
class Archive(c.Structure):
    _fields_=[("avg",c.c_uint32),("lines",c.c_int)]
libs=[]
for path in sys.argv[1:3]:
    l=c.CDLL(str(Path(path).resolve()))
    for name,restype,at in [
        ("kolmo_archive_bound",c.c_size_t,[c.POINTER(File),c.c_size_t,c.POINTER(Params)]),
        ("kolmo_bound",c.c_size_t,[c.c_size_t,c.POINTER(Params)]),
        ("kolmo_archive_create",c.c_int64,[c.POINTER(File),c.c_size_t,c.c_void_p,c.c_size_t,c.POINTER(Params),c.POINTER(Archive),c.c_void_p]),
        ("kolmo_archive_extract",c.c_int64,[c.c_void_p,c.c_size_t,c.c_uint64,c.c_void_p,c.c_size_t,c.c_int]),
        ("kolmo_params_for_level",c.c_int,[c.c_int,c.POINTER(Params)])]:
        f=getattr(l,name);f.restype=restype;f.argtypes=at
    libs.append(l)
r=random.Random(567)
member=gzip.compress(bytes(r.getrandbits(8) for _ in range(96<<10)),mtime=0)
data=[member,member,b"",b"millisecond,value\n"+b"0,500\n"*700]
names=[b"one.gz",b"repeat.gz",b"empty",b"data.csv"]
files=(File*4)(*[File(n,v,len(v),0o644,1600000000) for n,v in zip(names,data)])
with open(sys.argv[3],"w") as fp:
    w=csv.writer(fp);w.writerow("level block threads avg line_cuts old_bound new_bound archive_bytes archive_sha256 original_sha256".split())
    for level in (1,3,7):
        for block,avg in ((0,4096),(1,256),(65537,1<<22)):
            for lines in (0,1):
                p=Params(threads=2,block=block)
                assert libs[0].kolmo_params_for_level(level,c.byref(p))==0
                a=Archive(avg,lines)
                frames=[];sizes=[]
                for l in libs:
                    cap=l.kolmo_archive_bound(files,len(data),c.byref(p))
                    out=c.create_string_buffer(cap)
                    z=l.kolmo_archive_create(files,len(data),out,cap,c.byref(p),c.byref(a),None)
                    assert 0<z<=cap
                    frames.append(out.raw[:z]);sizes.append(cap)
                assert frames[0]==frames[1]
                for l in libs:
                    for i,v in enumerate(data):
                        out=c.create_string_buffer(max(1,len(v)))
                        assert l.kolmo_archive_extract(frames[0],z,i,out,len(v),2)==len(v)
                        assert out.raw[:len(v)]==v
                w.writerow([level,block,2,avg,lines,*sizes,z,hashlib.sha256(frames[0]).hexdigest(),
                            "|".join(hashlib.sha256(v).hexdigest() for v in data)]);fp.flush()
