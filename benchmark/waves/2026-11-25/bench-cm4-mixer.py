"""Alternate dispatch modes in one process queue and save per-trial resource usage."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import sys
import tempfile
import time


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for data in iter(lambda: stream.read(65536), b""):
            result.update(data)
    return result.hexdigest()


def run(binary, isa, action, source, destination, raw_bytes):
    args = [binary, action, str(source), str(destination), str(raw_bytes)]
    env = dict(os.environ, KOLMO_CPU=isa)
    start = time.perf_counter_ns()
    pid = os.posix_spawn(binary, args, env)
    _, status, usage = os.wait4(pid, 0)
    elapsed = time.perf_counter_ns() - start
    if status:
        raise RuntimeError(f"{args}: exit {os.waitstatus_to_exitcode(status)} ({isa})")
    return {
        "wall_ns": elapsed,
        "cpu_s": usage.ru_utime + usage.ru_stime,
        "peak_rss_bytes": usage.ru_maxrss * (1 if sys.platform == "darwin" else 1024),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("samples", nargs="+", type=Path)
    parser.add_argument("--binary", type=Path, default=Path("build/bench-cm4-mixer"))
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--prefix-bytes", type=int, default=262144)
    args = parser.parse_args()
    if args.repeats < 1 or args.prefix_bytes < 64:
        parser.error("need at least one repeat and a 64-byte or longer prefix")
    binary = str(args.binary.resolve(strict=True))
    rng = random.Random(342775)

    with tempfile.TemporaryDirectory(prefix="kolmo-cm4-neon-") as scratch:
        here = Path(scratch)
        source = here / "input"
        output = here / "output"
        shared = here / "frame-c"
        for sample in args.samples:
            with sample.open("rb") as stream:
                source.write_bytes(stream.read(args.prefix_bytes))
            source_sha256 = digest(source)
            raw_bytes = source.stat().st_size
            reference_digest = None
            for trial in range(-1, args.repeats):
                order = ["c", "neon"]
                rng.shuffle(order)
                for isa in order:
                    frame = here / f"frame-{isa}"
                    encode = run(binary, isa, "encode", source, frame, raw_bytes)
                    frame_sha256 = digest(frame)
                    if reference_digest is None:
                        reference_digest = frame_sha256
                    if frame_sha256 != reference_digest:
                        raise RuntimeError(f"frame mismatch for {sample} ({isa})")
                    if trial == -1 and isa == "c":
                        shared.write_bytes(frame.read_bytes())
                    if trial >= 0 and not shared.exists():
                        raise RuntimeError(f"missing C reference for {sample}")
                    decode = run(
                        binary, isa, "decode", shared if trial >= 0 else frame, output, raw_bytes
                    )
                    if digest(output) != source_sha256:
                        raise RuntimeError(f"decoded SHA-256 mismatch for {sample} ({isa})")
                    if trial >= 0:
                        print(json.dumps({
                            "sample": sample.name,
                            "source_sha256": source_sha256,
                            "frame_sha256": frame_sha256,
                            "raw_bytes": raw_bytes,
                            "frame_bytes": frame.stat().st_size,
                            "isa": isa,
                            "repeat": trial,
                            "encode": encode,
                            "decode": decode,
                        }), flush=True)
            shared.unlink()


if __name__ == "__main__":
    main()
