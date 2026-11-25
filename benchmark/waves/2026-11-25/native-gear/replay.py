"""Compare normal CLI archive creation and full restore on one stable archive member."""
import argparse
import csv
import hashlib
import os
from pathlib import Path
import random
import subprocess
import tempfile
import time

p = argparse.ArgumentParser(description=__doc__)
for key in ("main", "asm", "paired-c"):
    p.add_argument("--" + key, type=Path, required=True)
p.add_argument("--input", type=Path, required=True, action="append")
p.add_argument("--threads", type=int, nargs="+", default=[1, 4])
p.add_argument("--rounds", type=int, default=7)
p.add_argument("--out", type=Path, required=True)
args = p.parse_args()
programs = {k: str(v.resolve()) for k, v in {
    "main": args.main, "main-AA": args.main, "asm": args.asm, "paired-C": args.paired_c
}.items()}
rng = random.Random(20261125173)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def run(cmd, cwd):
    start = time.monotonic_ns()
    with subprocess.Popen(cmd, cwd=cwd, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL) as child:
        _, status, usage = os.wait4(child.pid, 0)
        child.returncode = os.waitstatus_to_exitcode(status)
        if child.returncode:
            raise RuntimeError((cmd, child.returncode))
    return ((time.monotonic_ns()-start)/1e6, (usage.ru_stime+usage.ru_utime)*1000,
            usage.ru_maxrss)


with tempfile.TemporaryDirectory() as tmp, args.out.open("w") as raw:
    work = Path(tmp)
    restored = work / "restored"
    restored.mkdir()
    fields = "source size sha256 threads arm round archive_bytes archive_sha256 encode_ms encode_cpu_ms encode_maxrss decode_ms decode_cpu_ms decode_maxrss".split()
    w = csv.writer(raw)
    w.writerow(fields)
    for source in args.input:
        data = source.read_bytes()
        member = work / source.name
        member.write_bytes(data)
        os.chmod(member, 0o644)
        os.utime(member, (1600000000, 1600000000))
        for threads in args.threads:
            expected = None
            for rep in range(-1, args.rounds):
                arms = list(programs)
                rng.shuffle(arms)
                for arm in arms:
                    frame = work / "archive.kmo"
                    exe = programs[arm]
                    enc = run([exe, "archive", "-1", "-T", str(threads), frame, member.name], work)
                    dec = run([exe, "extract", "-T", str(threads), "-C", restored, frame], work)
                    payload = frame.read_bytes()
                    if (restored/member.name).read_bytes() != data:
                        raise AssertionError((source, threads, rep, arm, "restore"))
                    if expected is None:
                        expected = digest(payload)
                    if digest(payload) != expected:
                        raise AssertionError((source, threads, rep, arm, "frame"))
                    w.writerow([member.name, len(data), digest(data), threads, arm, rep,
                                len(payload), expected, *enc, *dec])
                    raw.flush()
                    frame.unlink()
                    (restored/member.name).unlink()
        member.unlink()
