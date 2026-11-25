"""Exercise buffered, fd, regular file, pipe and CZNS stream entry points on valid modules."""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("library")
parser.add_argument("candidate")
parser.add_argument("reader")
parser.add_argument("onig", type=Path)
parser.add_argument("output", type=Path)
a = parser.parse_args()
lib = C.CDLL(a.library)
ptr = C.c_void_p
class Params(C.Structure):
    _fields_ = [("mode",C.c_int),("level",C.c_int),("threads",C.c_int),
                ("block",C.c_uint32),("transforms",C.c_int),("mem_limit",C.c_uint64)]

P = C.POINTER(Params)
for symbol, ret, arguments in [
    ("kolmo_params_for_level",C.c_int,[C.c_int,P]),
    ("kolmo_bound",C.c_size_t,[C.c_size_t,P]),
    ("kolmo_compress",C.c_int64,[ptr,C.c_size_t,ptr,C.c_size_t,P]),
    ("kolmo_compress_from_fd",C.c_int64,[C.c_int,C.c_size_t,ptr,C.c_size_t,P,ptr,C.c_size_t]),
    ("kolmo_compress_file",C.c_int64,[C.c_int,C.c_int,C.c_size_t,P,ptr,C.c_size_t]),
]:
    fn = getattr(lib,symbol)
    fn.argtypes, fn.restype = arguments, ret


def uleb(n):
    b = bytearray()
    while n >= 128:
        b.append(n & 127 | 128)
        n >>= 7
    return bytes(b + bytes([n]))


def sha(data):
    return hashlib.sha256(data).hexdigest()


with tempfile.TemporaryDirectory() as tmp,a.output.open("x") as receipt:
    root = Path(tmp)
    original = a.onig.read_bytes()
    # An ordinary named custom section, carrying 9 MiB of dense module bytes.
    payload = b"\x04data" + (original * (1 + (9 << 20) // len(original)))[:9 << 20]
    dense = original + b"\x00" + uleb(len(payload)) + payload
    for name, data in [("onig",original),("dense",dense)]:
        src = root/"input"
        frame = root/"output"
        src.write_bytes(data)
        for level in [1,3]:
            for threads in [1,4]:
                params = Params()
                assert lib.kolmo_params_for_level(level,C.byref(params)) == 0
                params.threads = threads
                params.transforms |= 128
                cap = lib.kolmo_bound(len(data),C.byref(params))
                out = C.create_string_buffer(cap)
                opts = [a.candidate, f"-{level}", f"-T{threads}", "--wasm-check-shuffle"]
                observed = {}
                n = lib.kolmo_compress(data,len(data),out,cap,C.byref(params))
                assert n > 0
                observed["buffer"] = out.raw[:n]
                with src.open("rb") as infile,frame.open("wb") as outfile:
                    n = lib.kolmo_compress_from_fd(infile.fileno(),len(data),out,cap,C.byref(params),None,0)
                    assert n > 0
                    observed["fd-to-buffer"] = out.raw[:n]
                    n = lib.kolmo_compress_file(infile.fileno(),outfile.fileno(),len(data),C.byref(params),None,0)
                    assert n > 0
                observed["fd-to-fd"] = frame.read_bytes()
                subprocess.run([*opts,"-f","-o",frame,src], check=True)
                observed["regular-file"] = frame.read_bytes()
                observed["pipe"] = subprocess.run([*opts,"-c"],input=data,check=True,capture_output=True).stdout
                assert len({sha(x) for x in observed.values()}) == 1
                observed["stream-file"] = subprocess.run([*opts,"--stream=1","-c",src],capture_output=True,check=True).stdout
                observed["stream-pipe"] = subprocess.run([*opts,"--stream=1","-c"],input=data,capture_output=True,check=True).stdout
                assert observed["stream-file"] == observed["stream-pipe"]
                for route,archive in observed.items():
                    decoded = subprocess.run([a.reader,"-dc"],input=archive,capture_output=True,check=True).stdout
                    assert decoded==data
                    row = dict(file=name,input_bytes=len(data),sha256=sha(data),level=level,threads=threads,
                               route=route,frame_bytes=len(archive),frame_sha256=sha(archive),readback_sha256=sha(decoded))
                    receipt.write(json.dumps(row)+"\n")
                    receipt.flush()
