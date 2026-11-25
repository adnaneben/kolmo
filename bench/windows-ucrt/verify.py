"""Run the ordinary memory API and five release-file readbacks in a fresh native Windows process."""
import csv
import ctypes as c
import hashlib
import json
import os
from pathlib import Path
import platform
import sys

ROOT = Path(__file__).resolve().parents[2]
FRAMES = Path(__file__).with_name("frames.csv")


def digest(data):
    return hashlib.sha256(data).hexdigest()


class Params(c.Structure):
    _fields_ = [("mode", c.c_int), ("level", c.c_int), ("threads", c.c_int), ("block", c.c_uint32),
                ("transforms", c.c_int), ("mem_limit", c.c_uint64)]


def check_api(lib, out):
    specs = {
        "kolmo_params_for_level": (c.c_int, [c.c_int, c.POINTER(Params)]),
        "kolmo_bound": (c.c_size_t, [c.c_size_t, c.POINTER(Params)]),
        "kolmo_compress": (c.c_int64, [c.c_void_p, c.c_size_t, c.c_void_p, c.c_size_t, c.POINTER(Params)]),
        "kolmo_decompress": (c.c_int64, [c.c_void_p, c.c_size_t, c.c_void_p, c.c_size_t, c.c_int]),
    }
    for name, (result, arguments) in specs.items():
        method = getattr(lib, name)
        method.restype, method.argtypes = result, arguments
    with FRAMES.open() as source:
        cases = list(csv.DictReader(source))
    if len(cases) != 5 or len({row["frame"] for row in cases}) != 5:
        raise ValueError("Expected the five pinned release frames")
    with (out / "checks.csv").open("w", newline="") as results:
        writer = csv.writer(results, lineterminator="\n")
        writer.writerow(["case", "threads", "stored_bytes", "stored_sha256", "decoded_bytes", "decoded_sha256"])
        plain = (ROOT / "tests/v12/r0.0.2/in/text.txt").read_bytes()
        if digest(plain) != next(row["original_sha256"] for row in cases if row["frame"].endswith("/text.txt.L3.kolmo")):
            raise ValueError("Unexpected ordinary round-trip source")
        for level in (1, 3, 8):
            params = Params(threads=2)
            if lib.kolmo_params_for_level(level, c.byref(params)) != 0:
                raise RuntimeError(f"Unsupported level {level}")
            capacity = lib.kolmo_bound(len(plain), c.byref(params))
            if not capacity:
                raise RuntimeError("No compression bound")
            encoded = c.create_string_buffer(capacity)
            size = lib.kolmo_compress(plain, len(plain), encoded, capacity, c.byref(params))
            if not 0 < size <= capacity:
                raise RuntimeError(f"Compression failed at level {level}: {size}")
            restored = c.create_string_buffer(len(plain))
            count = lib.kolmo_decompress(encoded, size, restored, len(plain), 2)
            if count != len(plain) or restored.raw != plain:
                raise ValueError(f"Ordinary round-trip mismatch at level {level}")
            writer.writerow([f"encode-decode-L{level}", 2, size, digest(encoded.raw[:size]), count, digest(restored.raw)])
            results.flush()
        for row in cases:
            payload = (ROOT / row["frame"]).read_bytes()
            if digest(payload) != row["frame_sha256"] or len(payload) != int(row["frame_bytes"]):
                raise ValueError(f"Frozen input differs: {row['frame']}")
            length = int(row["original_bytes"])
            for threads in (1, 4):
                restored = c.create_string_buffer(length)
                count = lib.kolmo_decompress(payload, len(payload), restored, length, threads)
                if count != length or digest(restored.raw) != row["original_sha256"]:
                    raise ValueError(f"Frozen output differs: {row['frame']}, T{threads}")
                writer.writerow([row["frame"], threads, len(payload), row["frame_sha256"], count, digest(restored.raw)])
                results.flush()


def main():
    dll, out = map(lambda a: Path(a).resolve(strict=True), sys.argv[1:])
    if sys.platform != "win32" or c.sizeof(c.c_void_p) != 8 or platform.machine().lower() not in ("amd64", "x86_64"):
        raise SystemExit("A fresh native Windows x86-64 process is required")
    with os.add_dll_directory(str(dll.parent)):
        lib = c.CDLL(str(dll))
        name = c.windll.kernel32.GetModuleFileNameW
        name.argtypes = [c.c_void_p, c.c_wchar_p, c.c_uint32]
        name.restype = c.c_uint32
        buffer = c.create_unicode_buffer(32768)
        n = name(lib._handle, buffer, len(buffer))
        if not 0 < n < len(buffer) or not os.path.samefile(buffer.value, dll):
            raise RuntimeError("Windows loader did not load the staged DLL")
        identity = {"pid": os.getpid(), "dll": buffer.value, "dll_sha256": digest(dll.read_bytes()),
                    "python": sys.version, "machine": platform.machine()}
        (out / "loader.json").write_text(json.dumps(identity, indent=2) + "\n")
        check_api(lib, out)
    print("Passed three ordinary memory-API round trips and ten frozen readbacks from the installed DLL.")


if __name__ == "__main__":
    main()
