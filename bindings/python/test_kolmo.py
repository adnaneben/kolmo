"""Tests for the kolmo Python binding. Run after `make lib` (and `make` for the CLI cross-check):
    python3 bindings/python/test_kolmo.py"""
import ctypes, os, resource, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import kolmo

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CLI = os.environ.get("KOLMO_CLI", os.path.join(ROOT, "build", "kolmo"))
LEVELS = [1, 2, 3, 4, 5, 6, 7, 8, 9, "ultra"]

def sample():
    return b"country,year,co2\n" + b"".join(b"Utopia,%d,%d.5\n" % (2000 + i % 20, i) for i in range(40000))

def czns(frames, seg=1 << 20):
    """A CZNS stream (FORMAT.md section 10) wrapping the given frames."""
    return (b"CZNS\x01\x00\x00\x00" + seg.to_bytes(4, "little")
            + b"".join(len(f).to_bytes(8, "little") + f for f in frames) + bytes(8))

def forge(nb, raw, kmem):
    """A frame whose header and index are valid (CRC included) and whose nb blocks each claim `raw` bytes from a
    5-byte payload, as review R23's forge.c: the claim is consistent, the payloads are garbage."""
    hdr = bytearray(32); hdr[:4] = b"CZN1"; hdr[4] = 12; hdr[5] = 1; hdr[11] = kmem
    hdr[12:20] = (raw * nb).to_bytes(8, "little"); hdr[20:24] = nb.to_bytes(4, "little"); hdr[24:28] = raw.to_bytes(4, "little")
    idx = b"".join(raw.to_bytes(4, "little") + (5).to_bytes(4, "little") + bytes(4) + b"\x20" + bytes(3) for _ in range(nb))
    crc = kolmo._lib.kolmo_crc32c
    crc.restype, crc.argtypes = ctypes.c_uint32, [ctypes.c_uint32, ctypes.c_char_p, ctypes.c_size_t]
    hdr[28:32] = crc(crc(0, bytes(hdr[:28]), 28), idx, len(idx)).to_bytes(4, "little")
    return bytes(hdr) + idx + b"".join(bytes(4) + bytes([(i * 131 + 7) & 255]) for i in range(nb))

def peak_rss_mib():
    r = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    return r / 2**20 if sys.platform == "darwin" else r / 2**10

def multi_frame(fail):
    """Concatenated frames and CZNS streams (bug 11 of the review40 synthesis): what the CLI writes for --stream and
    for pipes over 256 MiB decodes here too."""
    data = sample()
    a, b = data[:300000], data[300000:]
    fa, fb = kolmo.compress(a, level=1), kolmo.compress(b, level=5)
    cases = {"two frames": (fa + fb, data), "stream": (czns([fa, fb]), data),
             "stream then frame": (czns([fa]) + fb, data), "two streams": (czns([fa]) + czns([fb]), data),
             "empty stream": (czns([]), b""), "empty stream then frame": (czns([]) + fa, a)}
    for name, (blob, want) in cases.items():
        if kolmo.decompress(blob) != want: fail(f"{name}: decompress")
        if kolmo.original_size(blob) != len(want): fail(f"{name}: original_size")
        if want:
            for off, n in ((0, 10), (299990, 20), (len(want) - 5, 100), (12345, 400000)):
                if kolmo.read_range(blob, off, n) != want[off:off + n]: fail(f"{name}: read_range({off}, {n})")
        if kolmo.decompress(blob, max_output_size=len(want)) != want: fail(f"{name}: max_output_size equal")
        if want:
            try:
                kolmo.decompress(blob, max_output_size=len(want) - 1); fail(f"{name}: max_output_size exceeded but accepted")
            except ValueError as e:
                if "limit" not in str(e): fail(f"{name}: limit message {e}")
    bad = {"trailing byte": fa + b"x", "trailing zeros": fa + bytes(8), "truncated frame": (fa + fb)[:-1],
           "stream without terminator": czns([fa])[:-8], "stream cut in a length": czns([fa])[:-4],
           "stream length past the end": czns([fa])[:-9] + b"\x00" * 8,
           "two frames in one segment": czns([fa + fb]), "stream version 2": b"CZNS\x02" + czns([fa])[5:],
           "short stream header": b"CZNS\x01\x00", "empty": b""}
    for name, blob in bad.items():
        for call in (kolmo.decompress, kolmo.original_size, lambda x: kolmo.read_range(x, 0, 1)):
            try:
                call(blob); fail(f"{name}: accepted by {getattr(call, '__name__', 'read_range')}")
            except ValueError:
                pass
    try:
        kolmo.read_range(fa + fb, len(data) + 1, 1); fail("read_range past the end accepted")
    except ValueError as e:
        if "range" not in str(e): fail(f"read_range past the end: {e}")
    if kolmo.read_range(fa + fb, len(data), 5) != b"": fail("read_range at the end")
    print("concatenated frames and CZNS streams decode; damaged ones are refused")

def cli_streams(fail):
    """Files the command line writes: --stream (CZNS), concatenated outputs, a stream followed by a frame."""
    if not os.path.exists(CLI):
        print("CLI not built: CLI stream files skipped"); return
    data = (sample() * 4)[:3 << 20]          # 3 MiB: three 1 MiB segments
    with tempfile.TemporaryDirectory() as t:
        src = os.path.join(t, "in.csv"); open(src, "wb").write(data)
        run = lambda *a: subprocess.run([CLI, *a], capture_output=True, check=True).stdout
        st = run("-3", "-T", "1", "--stream=1", "-c", src)
        fr = run("-1", "-c", src)
        if st[:4] != b"CZNS": fail(f"CLI --stream wrote {st[:4]!r}")
        for name, blob, want in (("--stream", st, data), ("frame + frame", fr + fr, data + data),
                                 ("--stream + frame", st + fr, data + data)):
            if kolmo.decompress(blob, threads=2) != want: fail(f"CLI {name}: decompress")
            if kolmo.original_size(blob) != len(want): fail(f"CLI {name}: original_size")
            off = (1 << 20) - 7                   # across the first segment boundary
            if kolmo.read_range(blob, off, 100) != want[off:off + 100]: fail(f"CLI {name}: read_range")
            p = os.path.join(t, "x.kz"); open(p, "wb").write(blob)
            if name == "--stream" and run("-d", "-c", p) != want: fail(f"CLI {name}: the CLI's own decode differs")
    print("files written by the CLI (--stream, concatenated) decode")

def forged_frames(fail):
    """Bug 5 of the review40 synthesis: a 74-byte frame claiming 2 GiB made decompress zero-fill 2 GiB before refusing.
    The output now holds only the pages the decoder writes, and a claim the system cannot map is an error."""
    base = peak_rss_mib()
    for nb, raw, kmem in ((2, 1 << 30, 0x59), (16, 0xFFFFFFFF, 0x69), (65536, 0xFFFFFFFF, 0x69)):
        f = forge(nb, raw, kmem)
        if kolmo.original_size(f) != nb * raw: fail(f"forged frame {nb}x{raw}: the claim is not read")
        for call in (lambda: kolmo.decompress(f, threads=1), lambda: kolmo.read_range(f, 0, nb * raw, threads=1)):
            try:
                call(); fail(f"forged frame {nb}x{raw} accepted")
            except ValueError:
                pass
    grew = peak_rss_mib() - base
    if grew > 256: fail(f"forged frames raised peak RSS by {grew:.0f} MiB")
    print(f"forged frames claiming 2 GiB to 256 TiB refused; peak RSS grew {max(grew, 0):.0f} MiB")

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
    # the 0.0.1 keywords are gone
    for kw in ({"mode": "speed"}, {"dict": b"x"}, {"transforms": False}):
        try:
            kolmo.compress(b"abc", **kw); fail(f"{kw} accepted")
        except TypeError:
            pass
    for name in ("train_dict",):
        if hasattr(kolmo, name): fail(f"kolmo.{name} still exists")
    # output bound: a frame that claims more than max_output_size is refused before allocation
    blob = kolmo.compress(data)
    if kolmo.decompress(blob, max_output_size=len(data)) != data: fail("max_output_size equal to the size")
    try:
        kolmo.decompress(blob, max_output_size=len(data) - 1); fail("max_output_size exceeded but accepted")
    except ValueError as e:
        if "limit" not in str(e): fail(f"max_output_size message {e}")
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
    forged_frames(fail)
    multi_frame(fail)
    cli_streams(fail)
    # the same bytes as the command line at every level
    if os.path.exists(CLI):
        with tempfile.TemporaryDirectory() as t:
            src = os.path.join(t, "in.csv"); open(src, "wb").write(data)
            for lv in LEVELS:
                flag = "-U" if lv == "ultra" else f"-{lv}"
                cli = subprocess.run([CLI, flag, "-c", src], capture_output=True, check=True).stdout
                if cli != kolmo.compress(data, level=lv): fail(f"CLI {flag} and Python level={lv!r} differ")
        print("CLI and Python give the same bytes at every level")
        # a `kolmo archive` file is refused (its frame is the internal chunk store, not a file), never returned
        with tempfile.TemporaryDirectory() as t:
            for nm in ("a", "b"):
                open(os.path.join(t, nm), "wb").write(data)
            subprocess.run([CLI, "archive", "arc.kol", "a", "b"], cwd=t, capture_output=True, check=True)
            arc = open(os.path.join(t, "arc.kol"), "rb").read()
            for call in (lambda: kolmo.decompress(arc), lambda: kolmo.read_range(arc, 0, 10)):
                try:
                    call(); fail("a kolmo archive file was decompressed")
                except ValueError as e:
                    if "kolmo extract" not in str(e): fail(f"archive message {e}")
        print("kolmo archive files refused as expected")
    else:
        print("CLI not built: cross-check skipped")
    print("ALL PASSED" if not fails else f"{fails} FAILURES")
    sys.exit(1 if fails else 0)

if __name__ == "__main__":
    main()
