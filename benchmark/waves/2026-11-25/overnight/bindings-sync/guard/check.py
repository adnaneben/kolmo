"""Check installed public consumers, the trial on/off switch, and an independent release reader."""
import argparse
import csv
import hashlib
import os
from pathlib import Path
import struct
import subprocess
import tempfile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def check(language, args):
    rows = []
    for label, path, baseline, allowed_hash in (
        ("repetitive", args.repeat, args.repeat_stock, args.repeat_allowed),
        ("positive-control", args.positive, args.positive_stock, args.positive_allowed),
    ):
        original = path.read_bytes()
        reference = baseline.read_bytes()
        for setting in ("unset", "0", "1"):
            env = os.environ.copy()
            env.pop("KOLMO_BENCH_READ_CUTS", None)
            env.pop("KOLMO_LIBRARY", None)
            if setting != "unset":
                env["KOLMO_BENCH_READ_CUTS"] = setting
            with tempfile.TemporaryDirectory(dir=args.work) as td:
                frame = Path(td) / "frame"
                restored = Path(td) / "restored"
                if language == "python":
                    env["PYTHONPATH"] = str(args.site.resolve())
                    command = [args.python, args.consumer_python, path, frame]
                elif language == "rust":
                    command = [args.consumer_rust, path, frame]
                else:
                    command = [args.cli, "-q", "-U", "-T1", "-M", "256M", "-o", frame, path]
                subprocess.run(command, env=env, check=True, stdout=subprocess.DEVNULL)
                data = frame.read_bytes()
                actual = digest(data)
                assert actual == (allowed_hash if setting == "1" else digest(reference)), (label, language, setting, actual)
                subprocess.run([args.reader, "-q", "-T2", "-d", "-o", restored, frame], check=True)
                assert restored.read_bytes() == original
                blocks = struct.unpack_from("<I", data, 20)[0]
                entries = [struct.unpack_from("<IIIBxxx", data, 32+16*i) for i in range(blocks)]
                rows.append(dict(case=label, consumer=language, setting=setting, input_bytes=len(original),
                                 input_sha256=digest(original), frame_bytes=len(data), frame_sha256=actual,
                                 first_cut=entries[0][0], codecs="|".join(str(e[3]) for e in entries),
                                 release_restore_sha256=digest(restored.read_bytes())))
                print(label, language, setting, len(data), actual, flush=True)
    with args.out.open("w", newline="") as out:
        writer = csv.DictWriter(out, fieldnames=rows[0].keys(), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ("repeat", "repeat-stock", "positive", "positive-stock", "python", "site",
                 "consumer-python", "consumer-rust", "cli", "reader", "work", "out"):
        ap.add_argument("--"+name, type=Path, required=True)
    for name in ("repeat-allowed", "positive-allowed"):
        ap.add_argument("--"+name, required=True)
    ap.add_argument("consumer", choices=("cli", "python", "rust"))
    opt = ap.parse_args()
    check(opt.consumer, opt)
