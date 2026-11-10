"""Prepare the EIA or FASTA holdout from the public dataset."""
import argparse
from collections import Counter
import csv
from datetime import datetime
from functools import lru_cache
import gzip
import json
from pathlib import Path
import shutil
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
from panel import sha


@lru_cache(maxsize=8192)
def timestamp(value):
    return datetime.strptime(value, "%m/%d/%Y %I:%M:%S %p")


def eia(source, target, train):
    field = "UTC Time at End of Hour"
    with source.open(encoding="utf-8-sig", newline="") as raw:
        hours = Counter(timestamp(row[field]) for row in csv.DictReader(raw, strict=True))
    split = sorted(hours)[len(hours) // 4]
    with source.open("rb") as raw, target.open("xb") as test, train.open("xb") as early:
        header = raw.readline()
        index = next(csv.reader([header.decode("utf-8-sig")])).index(field)
        test.write(header)
        early.write(header)
        for line in raw:
            row = next(csv.reader([line.decode("utf-8")], strict=True))
            if timestamp(row[index]) >= split:
                test.write(line)
            elif early.tell() + len(line) <= (4 << 20):
                early.write(line)


def fasta(source, target, header):
    with gzip.open(source, "rb") as raw, target.open("xb") as out:
        if header is None:
            shutil.copyfileobj(raw, out)
            return
        found = False
        for line in raw:
            if line.startswith(b">"):
                if found:
                    break
                found = line.rstrip(b"\r\n") == header
            if found:
                out.write(line)
        if not found:
            raise ValueError(f"Missing FASTA entry {header!r}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("case", choices=["eia", "eia2022", "ce11", "danrer25", "pao1", "bsub", "yeast", "chr22"])
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path, help="new output directory")
    args = parser.parse_args()
    spec = json.loads((HERE / "inputs.json").read_text())[args.case]
    if sha(args.source) != spec["artifact_sha256"]:
        parser.error(f"source digest differs; see {spec['url']}")
    args.output.mkdir(exist_ok=False, parents=True)
    target = args.output / ("test.csv" if args.case in ("eia", "eia2022") else args.case + ".fa")
    if args.case in ("eia", "eia2022"):
        train = args.output / "train.csv"
        eia(args.source, target, train)
        if sha(train) != spec["train_sha256"]:
            raise ValueError("training bytes differ")
    else:
        header = {"ce11": b">chrI", "danrer25": b">chr25"}.get(args.case)
        fasta(args.source, target, header)
    if sha(target) != spec["target_sha256"]:
        raise ValueError("prepared input differs from the frozen benchmark")
    print(target)


if __name__ == "__main__":
    main()
