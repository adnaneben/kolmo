"""Compare full RGB arrays and exact lossless specialist native Python APIs."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import random
from statistics import median
import sys
import time

import imagecodecs
import numpy as np
from panel import sha, write_json, MODES

HERE=Path(__file__).resolve().parent

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("work",type=Path)
    a=p.parse_args()
    work=a.work.resolve()
    lib=work/"m1/build/libkolmo.dylib"
    pins=json.loads((HERE/"m1-freeze.json").read_text())["binaries"]
    assert sha(lib)==pins["candidate_library"]
    os.environ["KOLMO_LIB"]=str(lib)
    sys.path.insert(0,str(HERE.parents[1]/"bindings/python"))
    import kolmo
    assert Path(kolmo._lib._name).resolve()==lib
    source=work/"holdouts/coffee.rgb"
    digest=sha(source)
    meta=json.loads((HERE/"holdout-inputs.json").read_text())["coffee"]
    assert digest==meta["target_sha256"]
    data=source.read_bytes()
    pixels=np.frombuffer(data,dtype=np.uint8).reshape((meta["height"],meta["width"],3))
    def native_decode(fn, **opts):
        def decode(blob):
            arr=fn(blob,**opts)
            assert arr.shape==pixels.shape and arr.dtype==np.uint8
            return arr.tobytes()
        return decode
    codecs={
        "jpegxl-e7":(lambda:imagecodecs.jpegxl_encode(pixels,effort=7,lossless=True,numthreads=1),
                     native_decode(imagecodecs.jpegxl_decode,numthreads=1),imagecodecs.jpegxl_version()),
        "webp-m4":(lambda:imagecodecs.webp_encode(pixels,lossless=True,level=75,method=4,numthreads=1),
                   native_decode(imagecodecs.webp_decode),imagecodecs.webp_version()),
        "png-6":(lambda:imagecodecs.png_encode(pixels,level=6),
                 native_decode(imagecodecs.png_decode),imagecodecs.png_version()),
    }
    for mode in MODES:
        codecs["kolmo_"+mode]=(lambda m=mode:kolmo.compress(data,mode=m,threads=1),
                lambda blob:kolmo.decompress(blob,threads=1),pins["candidate_library"])
    rng=random.Random(61960)
    trials=[]
    out=work/"rgb-confirm.json"
    for repeat in range(8):
        order=list(codecs);rng.shuffle(order)
        for k in order:
            enc,dec,_=codecs[k]
            with (work/"timing.lock").open("a") as lock:
                fcntl.flock(lock,fcntl.LOCK_EX)
                t=time.perf_counter_ns();blob=enc();u=time.perf_counter_ns()
                raw=dec(blob);v=time.perf_counter_ns()
                assert raw==data
            trials.append(dict(codec=k,repeat=repeat,bytes=len(blob),encode_s=(u-t)/1e9,
                               decode_s=(v-u)/1e9,decoded_sha256=hashlib.sha256(raw).hexdigest()))
        result=dict(input="coffee",source_sha256=digest,source_bytes=len(data),
                    geometry=[meta["width"],meta["height"],"rgb24"],trials=trials,
                    packages=dict(imagecodecs=imagecodecs.__version__,numpy=np.__version__),
                    versions={k:x[2] for k,x in codecs.items()})
        write_json(out,result)
    metrics={}
    for k in codecs:
        rows=[r for r in trials if r["codec"]==k and r["repeat"]]
        assert len({r["bytes"] for r in rows})==1
        metrics[k]=dict(bytes=rows[0]["bytes"],encode=len(data)/median(r["encode_s"] for r in rows)/1e6,
                        decode=len(data)/median(r["decode_s"] for r in rows)/1e6)
    result["metrics"]=metrics
    write_json(out,result)
    print(metrics,flush=True)

if __name__=="__main__":
    main()
