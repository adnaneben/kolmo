"""Shared whole-file timing and hashing for the two competitor follow-ups."""
import hashlib
import os
import subprocess
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def phase(command, *, quiet=True):
    start = time.perf_counter_ns()
    child = subprocess.Popen(
        [str(arg) for arg in command],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL if quiet else None,
    )
    _, status, usage = os.wait4(child.pid, 0)
    elapsed = (time.perf_counter_ns() - start) / 1e9
    child.returncode = os.waitstatus_to_exitcode(status)
    if child.returncode:
        raise subprocess.CalledProcessError(child.returncode, command)
    return elapsed, usage.ru_utime + usage.ru_stime, usage.ru_maxrss


def observe(source, frame, readback, expected, encode, decode, *, quiet=True):
    enc, enc_cpu, enc_rss = phase(encode, quiet=quiet)
    dec, dec_cpu, dec_rss = phase(decode, quiet=quiet)
    restored = digest(readback)
    if restored != expected:
        raise ValueError(f"Readback SHA-256 mismatch for {source}")
    return {
        "n": source.stat().st_size,
        "bytes": frame.stat().st_size,
        "frame_hash": digest(frame),
        "enc": enc, "dec": dec,
        "encode_cpu": enc_cpu, "decode_cpu": dec_cpu,
        "encode_rss": enc_rss, "decode_rss": dec_rss,
    }


tables = {"energy": "corpus/energy_test.csv", "co2": "corpus-holdout/owid-co2-data.csv"}
