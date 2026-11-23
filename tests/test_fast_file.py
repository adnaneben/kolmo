"""Exercise the seekable CLI path and publication of checked output."""
import hashlib
import os
import subprocess
import tempfile
from pathlib import Path

CLI = Path(__file__).resolve().parents[1] / "build/kolmo"


def run(*args, success=True):
    result = subprocess.run([CLI, *args], capture_output=True)
    assert (result.returncode == 0) == success, (args, result.stderr)


with tempfile.TemporaryDirectory() as directory:
    root = Path(directory)
    source, frame, output, bad, alias = [root / f for f in ("source", "frame", "out", "bad", "link")]
    data = (b"date,load,output\n" + b"2018-01-01,123.05,321.5\n" * 410000)
    source.write_bytes(data)
    # dictionaries (-D) were removed in 0.0.2: the CLI refuses them with exit status 2 and a hint
    dictionary = root / "dict"
    dictionary.write_bytes(data[:4096])
    r = subprocess.run([CLI, "-3", "-D", dictionary, source, "-o", frame], capture_output=True)
    assert r.returncode == 2 and b"removed in 0.0.2" in r.stderr, r
    assert not frame.exists()
    run("-3", "-T", "1", source, "-o", frame)
    serial = hashlib.sha256(frame.read_bytes()).digest()
    run("-3", "-f", "-T", "4", source, "-o", frame)
    assert hashlib.sha256(frame.read_bytes()).digest() == serial

    run("-d", "-T", "4", frame, "-o", output)
    assert output.read_bytes() == data
    # Preserve existing filesystem aliases and mode when writing through them.
    output.write_bytes(b"old contents")
    output.chmod(0o600)
    os.link(output, alias)
    symlink = root / "symlink"
    symlink.symlink_to(alias)
    run("-d", "-f", frame, "-o", symlink)
    assert symlink.is_symlink() and output.samefile(alias)
    assert output.read_bytes() == data and output.stat().st_mode & 0o777 == 0o600

    corrupt = bytearray(frame.read_bytes())
    corrupt[-1] ^= 0x80
    bad.write_bytes(corrupt)
    output.write_bytes(b"keep")
    run("-d", "-f", bad, "-o", symlink, success=False)
    assert output.read_bytes() == b"keep" and not list(root.glob(".kolmo-*"))
    # a frame of an earlier version (byte 4 below 12) is refused with KOLMO_ERR_LEGACY's message, output untouched
    legacy = bytearray(frame.read_bytes())
    legacy[4] = 11
    bad.write_bytes(legacy)
    r = subprocess.run([CLI, "-d", "-f", bad, "-o", symlink], capture_output=True)
    assert r.returncode != 0 and b"written by an earlier Kolmo" in r.stderr, r
    assert output.read_bytes() == b"keep" and not list(root.glob(".kolmo-*"))
    # The output is never the input file, in any path, even with -f (0.0.2): refused, the input untouched.
    before = frame.read_bytes()
    run("-d", "-f", frame, "-o", frame, success=False)
    run("-f", "--stream", source, "-o", source, success=False)
    assert frame.read_bytes() == before and source.read_bytes() == data

    # The version-3 transpose packs 255 fields separately and the full suffix.
    # Exercise partial/ragged rows, embedded separators, boundary-spanning
    # fields, paths with and without file descriptors, and cross-thread decode.
    row = b';'.join([b'12345'] * 270 + [b'"a;b"', b'z' * 80]) + b'\r\n'
    wide = row * 5200 + b'only;two\n' + row * 90 + b'trailing;partial'
    source.write_bytes(wide)
    run("-f", "-T", "1", source, "-o", frame)
    encoded = frame.read_bytes()
    assert encoded[:5] == b'CZN1\x0c' and encoded[45] == 2   # version 12
    run("-f", "-T", "4", source, "-o", frame)
    assert frame.read_bytes() == encoded
    run("-d", "-f", "-T", "4", frame, "-o", output)
    assert output.read_bytes() == wide
    run("--range", "2097000:500", "-f", frame, "-o", output)
    assert output.read_bytes() == wide[2097000:2097500]
    run("-3", "-f", "-T", "1", source, "-o", source, success=False)
    assert source.read_bytes() == wide
