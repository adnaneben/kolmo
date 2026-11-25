"""Run only valid inputs. Compare exact-capacity new output to stock output and both readers."""
import argparse, csv, ctypes as c, gzip, hashlib, io, random, struct, tarfile
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument("stock")
p.add_argument("candidate")
p.add_argument("output")
args = p.parse_args()
class Params(c.Structure):
    _fields_ = [("mode",c.c_int), ("level",c.c_int), ("threads",c.c_int), ("block",c.c_uint32),
                ("transforms",c.c_int), ("mem",c.c_uint64)]
class Index(c.Structure):
    _fields_ = [("kind",c.c_int),("delim",c.c_uint8),("filter",c.c_int),("bits",c.c_int)]
def load(path):
    lib=c.CDLL(str(Path(path).resolve()))
    specs=[
        ("kolmo_params_for_level",c.c_int,[c.c_int,c.POINTER(Params)]),
        ("kolmo_mem_floor",c.c_uint64,[c.c_int]),
        ("kolmo_bound",c.c_size_t,[c.c_size_t,c.POINTER(Params)]),
        ("kolmo_indexed_bound",c.c_size_t,[c.c_size_t,c.POINTER(Params)]),
        ("kolmo_compress",c.c_int64,[c.c_void_p,c.c_size_t,c.c_void_p,c.c_size_t,c.POINTER(Params)]),
        ("kolmo_compress_indexed",c.c_int64,[c.c_void_p,c.c_size_t,c.c_void_p,c.c_size_t,c.POINTER(Params),c.POINTER(Index)]),
        ("kolmo_decompress",c.c_int64,[c.c_void_p,c.c_size_t,c.c_void_p,c.c_size_t,c.c_int])]
    for name,restype,at in specs:
        fun=getattr(lib,name);fun.restype=restype;fun.argtypes=at
    return lib
libs = [load(args.stock),load(args.candidate)]
rng = random.Random(7509)
def noise(n): return bytes(rng.getrandbits(8) for _ in range(n))
samples={"empty":b"", "one":b"x", "4095":noise(4095),"4096":noise(4096),"4097":noise(4097)}
samples["table"]=b"id,name,temp\n"+b"".join(("%d,reading,%d\n"%(i,i%173)).encode() for i in range(6601))
samples["gzip"]=gzip.compress(noise(256<<10),mtime=0)
samples["large-gzip"]=gzip.compress(noise(4<<20),mtime=0)
sh=("{'descr': '<i4', 'fortran_order': False, 'shape': (8192,), }").ljust(69)+"\n"
samples["npy"]=b"\x93NUMPY\x01\x00"+struct.pack("<H",len(sh))+sh.encode()+b"".join(struct.pack("<i",i*i) for i in range(8192))
f=io.BytesIO()
with tarfile.open(fileobj=f, mode="w", format=tarfile.USTAR_FORMAT) as tar:
    for i in range(8):
        member=gzip.compress(noise(80<<10),mtime=0)
        t=tarfile.TarInfo("member%02d.gz"%i);t.size=len(member)
        tar.addfile(t,io.BytesIO(member))
samples["ustar-8-gzip"]=f.getvalue()
digest=lambda data:hashlib.sha256(data).hexdigest()
with open(args.output,"w") as out:
    w=csv.writer(out,lineterminator="\n")
    w.writerow("input bytes sha256 level threads requested_block transforms mem_limit api old_bound new_bound frame_bytes frame_sha256 blocks".split())
    def check(name,lvl,block=0,threads=1,opts=None,capmode=0,indexed=False):
        data=samples[name]
        pp=Params(threads=threads,block=block)
        assert libs[0].kolmo_params_for_level(lvl,c.byref(pp))==0
        if opts is not None: pp.transforms=opts
        if capmode: pp.mem=2<<30 if capmode==1 else libs[1].kolmo_mem_floor(pp.mode)
        if lvl==10 and name=="large-gzip" and capmode==2: pp.mem=max(pp.mem,256<<20)
        iopts=Index(0,0,1,16)
        sizes=[]; frames=[]
        for j,l in enumerate(libs):
            fn=l.kolmo_indexed_bound if indexed else l.kolmo_bound
            cap=fn(len(data),c.byref(pp));sizes.append(cap)
            assert cap>0
            dst=c.create_string_buffer(cap)
            if indexed:
                got=l.kolmo_compress_indexed(data,len(data),dst,cap,c.byref(pp),c.byref(iopts))
            else:got=l.kolmo_compress(data,len(data),dst,cap,c.byref(pp))
            if not 0<got<=cap:raise ValueError((name,lvl,block,threads,opts,capmode,indexed,j,got,cap))
            frames.append(dst.raw[:got])
        assert frames[0]==frames[1],(name,lvl,block)
        for l in libs:
            back=c.create_string_buffer(max(1,len(data)))
            assert l.kolmo_decompress(frames[1],len(frames[1]),back,len(data),threads)==len(data)
            assert back.raw[:len(data)]==data
        frame=frames[1]
        w.writerow([name,len(data),digest(data),lvl,threads,block,pp.transforms,pp.mem,
                    "indexed" if indexed else "plain",*sizes,len(frame),digest(frame),
                    struct.unpack_from("<I",frame,20)[0]])
        out.flush()
    for lvl in range(1,11):
        for name in ["empty","one","4095","4096","4097","npy","gzip"]:
            check(name,lvl)
        check("ustar-8-gzip",lvl,0,4)
        check("large-gzip",lvl,0,4,capmode=2)
    for block in [1,4095,4096,4097,65535,65536,1<<20,1<<27]:
        for lv in [1,3,7]:
            check("table",lv,block,4)
        check("gzip",3,block,1)
    for opts in [0,1,2,3,1|16,1|32,1|64]:
        for name in ["npy","table"]:
            check(name,3,0,1,opts,1)
    for block in [0,1,4096,65537]:
        check("table",3,block,2,indexed=True)
        check("ustar-8-gzip",1,block,2,indexed=True)
