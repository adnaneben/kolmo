"""Check FASTA selection and frame compatibility for the opt-in fast encoder."""
import random
import struct
import subprocess
import sys
import tempfile
from pathlib import Path


def run(binary, *args):
    result = subprocess.run([binary, *map(str, args)], capture_output=True)
    assert result.returncode == 0, (binary, args, result.stderr)


def fasta(raw):
    lines = (raw[i:i + 80] for i in range(0, len(raw), 80))
    return b">synthetic\n" + b"\n".join(lines) + b"\n"


def check(stock, frontier, folder):
    src, regular, experimental, restored = [folder / name for name in ("in", "stock", "frontier", "out")]
    # Captured from the v1.1.0 research binary. The 32 packed rows are identical.
    legacy = bytes.fromhex(
        "435a4e310601000000000000280a000000000000010000000000100085379c5d"
        "280a0000a0020000384330bb0800000000000000080000005000000020000000"
        "00000000000102003e76312e312e300a"
    ) + b"\xb4" * 640
    experimental.write_bytes(legacy)
    run(frontier, "-d", experimental, restored)
    assert restored.read_bytes() == b">v1.1.0\n" + (b"ACGT" * 20 + b"\n") * 32

    rng = random.Random(81793)
    seq = bytes(rng.choices(b"ACGT", k=256 * 1024))
    half = len(seq) // 2
    words = [bytes(rng.choices(b"ACGT", k=12)) for _ in range(64)]
    table = b"".join(rng.choice(words) + b"," + rng.choice(words) + b"\n" for _ in range(12000))
    cases = (
        ("clean", fasta(seq), True),
        ("lower", fasta(seq.lower()), True),
        ("masked", fasta(seq[:half] + seq[half:].lower()), True),
        ("ambiguous suffix", fasta(seq[:-65536] + b"N" * 65536), True),
        ("tandem repeat", fasta((seq[:37] * (len(seq) // 37 + 1))[:len(seq)]), False),
        ("DNA-like CSV", table, False),
    )
    for label, data, expect_dna in cases:
        src.write_bytes(data)
        run(stock, "-m", "fast", "-T", 1, src, regular)
        run(frontier, "-d", regular, restored)
        assert restored.read_bytes() == data, (label, "stock frame")
        run(frontier, "-m", "fast", "-T", 1, src, experimental)
        run(frontier, "-d", experimental, restored)
        assert restored.read_bytes() == data, (label, "opt-in frame")
        packed = experimental.read_bytes()
        if expect_dna:
            assert packed[44] in (8, 9, 11), (label, "DNA block codec")
            assert len(packed) < 0.8 * regular.stat().st_size, label
        else:
            assert packed == regular.read_bytes(), (label, "regular frame")
            run(stock, "-d", experimental, restored)
            assert restored.read_bytes() == data, (label, "stock decoder")
            if label == "DNA-like CSV":
                blocks = struct.unpack_from("<I", packed, 20)[0]
                assert packed[32 + 16 * blocks] != 0, "lost table transform"

    # Over 8 MiB exercises the seekable-file path and more than one worker.
    data = fasta(seq[:half] + seq[half:].lower()) * 36
    src.write_bytes(data)
    run(frontier, "-m", "fast", "-T", 1, src, regular)
    run(frontier, "-m", "fast", "-T", 4, src, experimental)
    assert regular.read_bytes() == experimental.read_bytes(), "thread-dependent frame"
    run(frontier, "-d", "-T", 4, experimental, restored)
    assert restored.read_bytes() == data


if __name__ == "__main__":
    stock, frontier = map(Path, sys.argv[1:])
    with tempfile.TemporaryDirectory() as tmp:
        check(stock.resolve(), frontier.resolve(), Path(tmp))
    print("FASTA selection and frame compatibility passed")
