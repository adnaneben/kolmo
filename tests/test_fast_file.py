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
    dictionary = root / "dict"
    dictionary.write_bytes(data[:4096])
    run("-m", "fast", "-D", dictionary, "-T", "1", source, frame)
    serial = hashlib.sha256(frame.read_bytes()).digest()
    run("-m", "fast", "-D", dictionary, "-T", "4", source, frame)
    assert hashlib.sha256(frame.read_bytes()).digest() == serial
    run("-d", frame, output, success=False)
    assert not output.exists()

    run("-d", "-D", dictionary, "-T", "4", frame, output)
    assert output.read_bytes() == data
    # Preserve existing filesystem aliases and mode when writing through them.
    output.write_bytes(b"old contents")
    output.chmod(0o600)
    os.link(output, alias)
    symlink = root / "symlink"
    symlink.symlink_to(alias)
    run("-d", "-D", dictionary, frame, symlink)
    assert symlink.is_symlink() and output.samefile(alias)
    assert output.read_bytes() == data and output.stat().st_mode & 0o777 == 0o600

    corrupt = bytearray(frame.read_bytes())
    corrupt[-1] ^= 0x80
    bad.write_bytes(corrupt)
    output.write_bytes(b"keep")
    run("-d", "-D", dictionary, bad, symlink, success=False)
    assert output.read_bytes() == b"keep" and not list(root.glob(".kolmo-*"))
    # Same input and output path is safe once the entire frame is verified.
    run("-d", "-D", dictionary, frame, frame)
    assert frame.read_bytes() == data

    # The version-3 transpose packs 255 fields separately and the full suffix.
    # Exercise partial/ragged rows, embedded separators, boundary-spanning
    # fields, paths with and without file descriptors, and cross-thread decode.
    row = b';'.join([b'12345'] * 270 + [b'"a;b"', b'z' * 80]) + b'\r\n'
    wide = row * 5200 + b'only;two\n' + row * 90 + b'trailing;partial'
    source.write_bytes(wide)
    run("-T", "1", source, frame)
    encoded = frame.read_bytes()
    assert encoded[:5] == b'CZN1\x03' and encoded[45] == 2
    run("-T", "4", source, frame)
    assert frame.read_bytes() == encoded
    run("-d", "-T", "4", frame, output)
    assert output.read_bytes() == wide
    run("-r", "2097000:500", frame, output)
    assert output.read_bytes() == wide[2097000:2097500]
    run("-m", "fast", "-T", "1", source, source)
    assert source.read_bytes() == encoded
