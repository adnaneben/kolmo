"""Split OWID tables by country identity and train one bounded OpenZL CSV model."""
import argparse
import csv
import hashlib
import json
import time
from pathlib import Path
import subprocess

from competitor_support import digest, tables


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("corpus", type=Path)
parser.add_argument("out", type=Path)
parser.add_argument("zli")
parser.add_argument("dataset", choices=tables)
args = parser.parse_args()
base = args.out / args.dataset
train = base / "train"
train.mkdir(parents=True, exist_ok=False)
source = args.corpus / tables[args.dataset]
test = base / "test.csv"
partitions = [bytearray() for _ in range(5)]
countries = [set() for _ in range(5)]
counts = [0] * 5
with source.open("rb") as raw:
    header = raw.readline()
    assert next(csv.reader([header.decode()]))[0] == "country"
    for part in partitions:
        part.extend(header)
    for line in raw:
        cells = next(csv.reader([line.decode()], strict=True))
        key = cells[0]
        bucket = int.from_bytes(hashlib.sha256(key.encode()).digest()[:8], "big")
        index = (bucket // 4) % 4 if bucket % 4 == 0 else 4
        partitions[index].extend(line)
        countries[index].add(key)
        counts[index] += 1
for i, group in enumerate(countries):
    assert all(group.isdisjoint(other) for other in countries[i + 1:])
files = [train / f"train-{i}.csv" for i in range(4)] + [test]
records = []
for i, path in enumerate(files):
    path.write_bytes(partitions[i])
    records.append(dict(file=str(path.relative_to(base)), bytes=path.stat().st_size, sha256=digest(path),
                        countries=len(countries[i]), rows=counts[i]))
graph = base / "graph.zlc"
command = [args.zli, "train", "-p", "csv", train, "-o", graph, "--threads", 1, "--max-time-secs", 120]
start = time.perf_counter()
with (base / "train.log").open("wb") as log:
    subprocess.run([str(arg) for arg in command], stdout=log, stderr=log, check=True)
receipt = dict(source_sha256=digest(source), files=records, train_seconds=time.perf_counter()-start,
               train_threads=1, budget_seconds=120, graph_sha256=digest(graph), graph_bytes=graph.stat().st_size)
(base / "training.json").write_text(json.dumps(receipt, indent=2) + "\n")
