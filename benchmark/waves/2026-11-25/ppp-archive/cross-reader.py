"""Build identical archives on two revisions and verify both readers on both writers."""
import argparse
import csv
import hashlib
import itertools
import os
from pathlib import Path
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("before", type=Path)
parser.add_argument("after", type=Path)
args = parser.parse_args()
tools = [args.before.resolve(), args.after.resolve()]
repo = Path(__file__).resolve().parents[4]

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def run(*args, cwd):
    subprocess.run(args, cwd=cwd, stdout=subprocess.DEVNULL, check=True)

with tempfile.TemporaryDirectory() as td:
    work = Path(td)
    source = work / "input"
    source.mkdir()
    files = {"prose": (repo / "SPEC.md").read_bytes(), "code": (repo / "src/core/extension.c").read_bytes()}
    files["copy-of-code"] = files["code"]
    files["empty"] = b""
    for name, data in files.items():
        f = source / name
        f.write_bytes(data)
        f.chmod(0o640)
        os.utime(f, (1700000000, 1700000000))
    writer = csv.writer(sys.stdout, lineterminator="\n")
    writer.writerow(["level", "threads", "chunk_KiB", "original_bytes", "archive_bytes", "archive_sha256", "cross_restores"])
    expected = {name: digest(source / name) for name in files}
    for name, sha in expected.items():
        print(f"{name}: {len(files[name])} bytes sha256={sha}", file=sys.stderr)
    for level, threads, chunk in itertools.product((1, 3, 9), (1, 4), (1, 4)):
        archives = [work / "before.kol", work / "after.kol"]
        for tool, arc in zip(tools, archives):
            run(tool, "archive", f"-{level}", "-f", "-T", str(threads), "--chunk", str(chunk), arc, *files, cwd=source)
        if archives[0].read_bytes() != archives[1].read_bytes():
            raise ValueError(f"archive mismatch: {level=} {threads=} {chunk=}")
        for arc, reader in itertools.product(archives, tools):
            with tempfile.TemporaryDirectory(dir=work) as dst:
                run(reader, "extract", "-T", str(threads), "-C", dst, arc, cwd=source)
                paths = list(Path(dst).iterdir())
                if {p.name: digest(p) for p in paths} != expected:
                    raise ValueError("member content mismatch")
                if any((p.stat().st_mode & 0o777) != 0o640 or int(p.stat().st_mtime) != 1700000000 for p in paths):
                    raise ValueError("member metadata mismatch")
        writer.writerow([level, threads, chunk, sum(map(len, files.values())), archives[0].stat().st_size, digest(archives[0]), 4])
