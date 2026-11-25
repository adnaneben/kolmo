"""Time the whole enwik8.zip archive at L1/T4 against the identical-flag #100 binary."""
import argparse
import csv
import hashlib
import os
from pathlib import Path
import random
import subprocess
import tempfile
import time


def timed(command, directory):
    start = time.monotonic_ns()
    with subprocess.Popen(command, cwd=directory, stdout=subprocess.DEVNULL,
                          stderr=subprocess.DEVNULL) as child:
        _, status, usage = os.wait4(child.pid, 0)
        elapsed = time.monotonic_ns() - start
        child.returncode = os.waitstatus_to_exitcode(status)
        if child.returncode:
            raise RuntimeError((command, child.returncode))
    return elapsed, usage.ru_stime + usage.ru_utime, usage.ru_maxrss


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for option in ("base", "stack", "input", "output"):
        p.add_argument("--" + option, type=Path, required=True)
    args = p.parse_args()
    programs = {"base": args.base.resolve(), "base-AA": args.base.resolve(),
                "stack": args.stack.resolve()}
    data = args.input.read_bytes()
    original = hashlib.sha256(data).hexdigest()
    assert original == "547994d9980ebed1288380d652999f38a14fe291a6247c157c3d33d4932534bc"
    rng = random.Random(2026112554)
    with tempfile.TemporaryDirectory() as tmp, args.output.open("x") as output:
        directory = Path(tmp)
        source = directory / "enwik8.zip"
        source.write_bytes(data)
        source.chmod(0o644)
        os.utime(source, (1600000000, 1600000000))
        target = directory / "restore"
        target.mkdir()
        restored = target / source.name
        frame = directory / "frame"
        expectations = None
        writer = csv.writer(output, lineterminator="\n")
        writer.writerow(["repeat", "order", "arm", "input_bytes", "input_sha256", "frame_bytes",
                         "frame_sha256", "encode_ns", "encode_cpu_s", "encode_rss_bytes",
                         "decode_ns", "decode_cpu_s", "decode_rss_bytes", "load1"])
        for rep in range(22):
            labels = list(programs)
            rng.shuffle(labels)
            for order, label in enumerate(labels):
                encode = [programs[label], "archive", "-1", "-T", "4", frame, source.name]
                decode = [programs[label], "extract", "-T", "4", "-C", target, frame]
                start = timed(encode, directory)
                finish = timed(decode, directory)
                assert restored.read_bytes() == data
                st = restored.stat()
                assert (st.st_mode & 0o777, int(st.st_mtime)) == (0o644, 1600000000)
                restored.unlink()
                if rep == 0:
                    timed([programs["base"], *decode[1:]], directory)
                    assert restored.read_bytes() == data
                    restored.unlink()
                frame_hash = hashlib.sha256(frame.read_bytes()).hexdigest()
                if expectations is None:
                    expectations = frame_hash
                assert frame_hash == expectations
                writer.writerow([rep, order, label, len(data), original, frame.stat().st_size,
                                 frame_hash, *start, *finish, os.getloadavg()[0]])
                output.flush()
                frame.unlink()


if __name__ == "__main__":
    main()
