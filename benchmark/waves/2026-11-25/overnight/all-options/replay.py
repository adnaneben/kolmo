"""Replay historical main, qualified main twice and the opt-in integrated binary."""
import argparse
import csv
import hashlib
import json
import os
import platform
import random
import subprocess
import sys
import tempfile
import time
from collections import Counter
from pathlib import Path

REFERENCE = Path(__file__).with_name("manifest.json")
SEED = 2026112508
PROFILES = {"enwik8.zip": ["1"], "owid-co2.csv": ["1", "2"],
            "prose-64k": ["3"], "pydub.wav": ["7", "ultra"]}


def digest(data):
    return hashlib.sha256(data).hexdigest()


def timed(argv, cwd, env):
    args = [str(arg) for arg in argv]
    with tempfile.TemporaryFile() as errors:
        start = time.monotonic_ns()
        with subprocess.Popen(args, cwd=cwd, env=env, stdout=subprocess.DEVNULL, stderr=errors) as child:
            _, status, usage = os.wait4(child.pid, 0)
            wall = time.monotonic_ns() - start
            child.returncode = os.waitstatus_to_exitcode(status)
            if child.returncode:
                errors.seek(0)
                message = errors.read(16000).decode(errors="replace")
                raise RuntimeError(f"{args} exited {child.returncode}: {message}")
    rss = usage.ru_maxrss * (1024 if sys.platform.startswith("linux") else 1)
    return wall, usage.ru_utime + usage.ru_stime, rss


def check_restore(path, data, archive):
    if path.read_bytes() != data:
        raise ValueError(f"Restoration mismatch: {path.name}")
    if archive:
        info = path.stat()
        if (info.st_mode & 0o777, int(info.st_mtime)) != (0o644, 1600000000):
            raise ValueError("Restored archive member mode or mtime differs")
    path.unlink()


def run_cell(directory, restore, source, level, threads, exe, reader, data, env):
    frame = directory / "frame"
    archive = source == "enwik8.zip"
    output = restore / (source if archive else "out")
    if archive:
        enc = [exe, "archive", "-1", "-T", threads, frame, source]
        dec = [exe, "extract", "-T", threads, "-C", restore, frame]
    else:
        enc = [exe, "--level", level, "-T", threads, "-q", "-f", source, "-o", frame]
        dec = [exe, "-d", "-T", threads, "-q", "-f", frame, "-o", output]
    encode = timed(enc, directory, env)
    decode = timed(dec, directory, env)
    check_restore(output, data, archive)
    if reader is not None:
        timed([reader, *dec[1:]], directory, env)
        check_restore(output, data, archive)
    encoded = frame.read_bytes()
    frame.unlink()
    counts = {}
    if not archive:
        blocks = int.from_bytes(encoded[20:24], "little")
        if len(encoded) < 44 or blocks > (len(encoded) - 44) // 16:
            raise ValueError("Incomplete frame header or block table")
        counts = dict(Counter(encoded[44 + 16*i] & 63 for i in range(blocks)))
    return encoded, counts, encode, decode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("before", "main", "combined", "archive", "csv", "prose", "audio", "out"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=7)
    parser.add_argument("--reference", type=Path, default=REFERENCE, help="Published manifest for this exact comparison")
    parser.add_argument("--verify-reference", action="store_true",
                        help="Require original binary and data hashes from the published manifest")
    args = parser.parse_args()
    if args.rounds < 1 or sys.platform not in ("darwin", "linux"):
        parser.error("positive rounds and Darwin or Linux are required")
    executables = {"pre": args.before.resolve(strict=True), "main": args.main.resolve(strict=True),
                   "main-AA": args.main.resolve(strict=True), "combined": args.combined.resolve(strict=True)}
    paths = dict(zip(PROFILES, (args.archive, args.csv, args.prose, args.audio)))
    data, sources = {}, {}
    for name, path in paths.items():
        whole = path.read_bytes()
        data[name] = whole[:65536] if name == "prose-64k" else whole
        sources[name] = dict(source_sha256=digest(whole), source_bytes=len(whole),
                             input_sha256=digest(data[name]), input_bytes=len(data[name]))
    binaries = {k: digest(p.read_bytes()) for k, p in executables.items()}
    reference = json.loads(args.reference.read_text()) if args.reference.is_file() else {}
    if args.verify_reference and (binaries != reference.get("binaries") or sources != reference.get("sources")):
        parser.error("the executable or source/input digests differ from the published experiment")

    settings = [(s, l, t) for s, levels in PROFILES.items() for l in levels for t in (1, 4)]
    env = dict(os.environ, KOLMO_AUDIO_SCREEN="1")
    provenance = {k: reference["build_provenance"][k] for k, h in binaries.items()
                  if reference.get("binaries", {}).get(k) == h and k in reference.get("build_provenance", {})}
    manifest = dict(binaries=binaries, sources=sources, randomization_seed=SEED, rounds=args.rounds,
                    settings=settings, archive_mode_and_mtime=[420, 1600000000],
                    cross_reader="warmup frames decoded by qualified main, excluded from phase scores",
                    build_provenance=provenance,
                    build_provenance_evidence={"classification": "supplied by published manifest for matching digests"},
                    runtime={"platform": platform.platform(), "rss_unit": "bytes",
                             "KOLMO_AUDIO_SCREEN": env["KOLMO_AUDIO_SCREEN"],
                             "effective_worker_count": "not instrumented"})
    args.out.mkdir(parents=True, exist_ok=True)
    if any((args.out / n).exists() for n in ("raw.csv", "manifest.json")):
        parser.error("use a fresh output directory to preserve earlier measurements")
    rng = random.Random(SEED)
    with (args.out / "manifest.json").open("x") as metadata:
        json.dump(manifest, metadata, indent=2)
        metadata.write("\n")
    with tempfile.TemporaryDirectory() as tmp, (args.out / "raw.csv").open("x", newline="", buffering=1) as stream:
        directory = Path(tmp)
        restore = directory / "restore"
        restore.mkdir()
        for name, buf in data.items():
            path = directory / name
            path.write_bytes(buf)
            path.chmod(0o644)
            os.utime(path, (1600000000, 1600000000))
        writer = None
        for repeat in range(args.rounds + 1):
            cells = list(settings)
            rng.shuffle(cells)
            for source, level, threads in cells:
                labels = list(executables)
                rng.shuffle(labels)
                for order, arm in enumerate(labels):
                    reader = executables["main"] if repeat == 0 else None
                    packed, counts, enc, dec = run_cell(directory, restore, source, level, threads,
                                                        executables[arm], reader, data[source], env)
                    row = dict(source=source, level=level, threads=threads, repeat=repeat, order=order,
                               arm=arm, input_bytes=len(data[source]), sha256=sources[source]["input_sha256"],
                               stored_bytes=len(packed), frame_sha256=digest(packed), codecs=json.dumps(counts),
                               encode_ns=enc[0], decode_ns=dec[0], encode_cpu_s=enc[1], decode_cpu_s=dec[1],
                               encode_rss_B=enc[2], decode_rss_B=dec[2], load1=os.getloadavg()[0])
                    if writer is None:
                        writer = csv.DictWriter(stream, fieldnames=row)
                        writer.writeheader()
                    writer.writerow(row)
            print("pass", repeat, "complete", flush=True)


if __name__ == "__main__":
    main()
