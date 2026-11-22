"""Tests for the kolmo Python binding. Run after `make lib` (and `make` for the CLI cross-check):
    python3 bindings/python/test_kolmo.py"""
import os, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kolmo

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLI = os.environ.get("KOLMO_CLI", os.path.join(ROOT, "build", "kolmo"))
LEVELS = [1, 2, 3, 4, 5, 6, 7, 8, 9, "ultra"]

def sample():
    return b"country,year,co2\n" + b"".join(b"Utopia,%d,%d.5\n" % (2000 + i % 20, i) for i in range(40000))

def main():
    samples = [b"", b"hello", os.urandom(1000), sample()]
    fails = 0
    def fail(msg):
        nonlocal fails
        fails += 1; print("FAIL", msg)
    for data in samples:
        for lv in LEVELS:
            blob = kolmo.compress(data, level=lv, threads=1)
            if kolmo.decompress(blob) != data: fail(f"level {lv} len {len(data)}: round trip")
            if blob != kolmo.compress(data, level=lv, threads=4): fail(f"level {lv}: threads change the output")
        if data:
            r = kolmo.compress(data)
            if kolmo.original_size(r) != len(data): fail("original_size")
            mid = len(data) // 2
            n = min(64, len(data) - mid)
            if kolmo.read_range(r, mid, n) != data[mid:mid + n]: fail("read_range")
            if kolmo.read_range(r, mid, 10**12) != data[mid:]: fail("read_range past the end is clamped")
        print(f"len {len(data):>7}: round trips ok at every level")
    data = samples[3]
    # names and numbers are the same levels; the default is 3 (light)
    for name, num in kolmo.LEVEL_NAMES.items():
        if kolmo.compress(data, level=name) != kolmo.compress(data, level=num if num < 10 else "ultra"): fail(f"name {name}")
    if kolmo.compress(data) != kolmo.compress(data, level="light"): fail("default is not light")
    for bad in (0, 10, -1, "fast", "speed", "ratio", None, 3.0, True):
        try:
            kolmo.compress(b"abc", level=bad); fail(f"level {bad!r} accepted")
        except ValueError:
            pass
    # the 0.0.x keywords are gone
    for kw in ({"mode": "speed"}, {"dict": b"x"}, {"transforms": False}):
        try:
            kolmo.compress(b"abc", **kw); fail(f"{kw} accepted")
        except TypeError:
            pass
    for name in ("train_dict",):
        if hasattr(kolmo, name): fail(f"kolmo.{name} still exists")
    # memory limit
    if kolmo.decompress(kolmo.compress(data, level=9, memlimit="4G"), memlimit="max") != data: fail("memlimit round trip")
    try:
        kolmo.compress(data, memlimit="lots"); fail("memlimit 'lots' accepted")
    except ValueError:
        pass
    # container version 12 only: a blob relabelled as version 11 is refused as legacy
    blob = bytearray(kolmo.compress(data))
    if blob[:5] != b"CZN1\x0c": fail(f"version byte {blob[:5]!r}")
    blob[4] = 11
    try:
        kolmo.decompress(bytes(blob)); fail("version 11 blob accepted")
    except ValueError as e:
        if "earlier Kolmo" not in str(e): fail(f"legacy message {e}")
    # a hostile 32-byte header claiming 40 GiB: refused before any allocation
    hostile = bytearray(32); hostile[:5] = b"CZN1\x0c"; hostile[12:20] = (40 << 30).to_bytes(8, "little")
    for call in (lambda: kolmo.decompress(bytes(hostile)), lambda: kolmo.read_range(bytes(hostile), 0, 1 << 30)):
        try:
            call(); fail("hostile header accepted")
        except ValueError:
            pass
    print("legacy and hostile frames refused as expected")
    # the same bytes as the command line at every level
    if os.path.exists(CLI):
        with tempfile.TemporaryDirectory() as t:
            src = os.path.join(t, "in.csv"); open(src, "wb").write(data)
            for lv in LEVELS:
                flag = "-U" if lv == "ultra" else f"-{lv}"
                cli = subprocess.run([CLI, flag, "-c", src], capture_output=True, check=True).stdout
                if cli != kolmo.compress(data, level=lv): fail(f"CLI {flag} and Python level={lv!r} differ")
        print("CLI and Python give the same bytes at every level")
    else:
        print("CLI not built: cross-check skipped")
    print("ALL PASSED" if not fails else f"{fails} FAILURES")
    sys.exit(1 if fails else 0)

if __name__ == "__main__":
    main()
