"""Freeze and benchmark the Portuguese electricity holdout with the prior CLI protocol."""
import argparse
import csv
import json
import platform
import random
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path
from statistics import median

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "electricity"))
from run import commands, digest, measure, save

SOURCE = "https://archive.ics.uci.edu/static/public/321/electricityloaddiagrams20112014.zip"
CODECS = ("candidate", "baseline", "zstd", "openzl")
BASELINE = "27febccfd8a6be7373cee0142777bcd4df371563"


def partitions(archive, work):
    path = work / "portugal.txt"
    with zipfile.ZipFile(archive) as z, z.open("LD2011_2014.txt") as src, path.open("wb") as dst:
        shutil.copyfileobj(src, dst)
    with path.open("rb") as f:
        lines = sum(1 for _ in f)
    with path.open(encoding="utf-8-sig", newline="") as f:
        if sum(1 for _ in csv.reader(f, delimiter=";", strict=True)) != lines:
            raise ValueError("A split would bisect a logical CSV row")
    rows = lines - 1
    train, test = work / "train.csv", work / "test.csv"
    count = 0
    with path.open("rb") as f, train.open("wb") as a, test.open("wb") as b:
        header = next(f)
        a.write(header)
        b.write(header)
        for i, row in enumerate(f):
            if i >= rows // 4:
                b.write(row)
            elif i == count and a.tell() + len(row) <= (4 << 20):
                a.write(row)
                count += 1
    return dict(url=SOURCE, archive_sha256=digest(archive), source_sha256=digest(path), rows=rows,
                train_sha256=digest(train), test_sha256=digest(test), train_rows=count, test_rows=rows-rows//4,
                train_bytes=train.stat().st_size, test_bytes=test.stat().st_size)


def benchmark(source, graph, bins, work, expected, release):
    rng = random.Random(42571)
    trials = []
    with tempfile.TemporaryDirectory(dir=work) as temp:
        frame, output = Path(temp) / "frame", Path(temp) / "decoded"
        for repeat in range(8):
            codecs = list(bins)
            rng.shuffle(codecs)
            for codec in codecs:
                enc, dec = commands(codec, bins[codec], source, frame, output, graph)
                et, dt = measure(enc), measure(dec)
                assert digest(output) == expected
                out_hash = digest(frame)
                size = frame.stat().st_size
                output.unlink()
                if codec == "candidate" and repeat == 0:
                    # Actual installed 1.0.0 decoder, untimed. Check the block recipe too.
                    subprocess.run([release, "-d", "-T", "1", frame, output], check=True)
                    assert digest(output) == expected
                    output.unlink()
                    with frame.open("rb") as f:
                        header = f.read(32)
                        count = int.from_bytes(header[20:24], "little")
                        assert header[:4] == b"CZN1"
                        index = f.read(16 * count)
                        flags = [index[16*i+13] for i in range(count)]
                    info = dict(blocks=count, transpose_blocks=flags.count(2), version=header[4],
                                release_decoded_sha256=expected)
                trials.append(dict(codec=codec, repeat=repeat, compressed=size, encoded_sha256=out_hash,
                                   decoded_sha256=expected, encode=et, decode=dt))
                frame.unlink()
            print("Verified", source.name, "warm-up" if not repeat else f"{repeat}/7", flush=True)
    for codec in bins:
        assert len({t["encoded_sha256"] for t in trials if t["codec"] == codec}) == 1
    return dict(frame=info, trials=trials)


def summarize(result):
    metrics = {}
    for codec in CODECS:
        rows = [t for t in result["trials"] if t["codec"] == codec and t["repeat"]]
        assert sorted(t["repeat"] for t in rows) == list(range(1,8))
        metrics[codec] = dict(size=rows[0]["compressed"], **{
            key+"_"+stage: op(t[stage][key] for t in rows)
            for key,op in (("seconds",median),("peak_rss_bytes",max)) for stage in ("encode","decode")})
    c = metrics["candidate"]
    gates = {r: {k: (5*c[k] <= 4*v if k=="size" else c[k] <= v) for k,v in metrics[r].items()}
             for r in ("zstd","openzl")}
    return dict(metrics=metrics, tests=gates, zstd_pass=all(gates["zstd"].values()),
                both_pass=all(all(flags.values()) for flags in gates.values()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["freeze", "score", "development"])
    parser.add_argument("archive", type=Path)
    parser.add_argument("work", type=Path)
    for key in (*CODECS,"release"):
        parser.add_argument("--"+key, type=Path, required=True)
    parser.add_argument("--dev-input", type=Path)
    args = parser.parse_args()
    work = args.work
    work.mkdir(parents=True, exist_ok=True)
    bins = {k:getattr(args,k).resolve() for k in CODECS}
    hashes = {k:digest(p) for k,p in {**bins,"release":args.release}.items()}
    protocol = digest(HERE / "protocol.md")
    freeze_path = HERE / "freeze.json"
    if args.action == "freeze":
        if freeze_path.exists():
            raise FileExistsError("A scored experiment cannot be re-frozen")
        pins = json.loads((HERE.parent / "electricity/freeze.json").read_text())["binaries"]
        assert hashes["baseline"] == pins["candidate"]
        assert all(hashes[k] == pins[k] for k in ("zstd","openzl"))
        save(freeze_path, dict(inputs=partitions(args.archive,work), binaries=hashes, protocol_sha256=protocol,
                               runner_sha256=digest(Path(__file__)), system=platform.platform(), baseline_commit=BASELINE,
                               candidate_commit=subprocess.check_output(["git","rev-parse","HEAD"],text=True).strip()))
        return
    frozen = json.loads(freeze_path.read_text())
    assert hashes == frozen["binaries"] and protocol == frozen["protocol_sha256"]
    assert digest(Path(__file__)) == frozen["runner_sha256"]
    if args.action == "development":
        base = HERE.parent / "electricity"
        ref = json.loads((base / "freeze.json").read_text())["inputs"]
        graph = base / "opsd.zlc"
        assert digest(args.dev_input) == ref["test_sha256"]
        assert digest(graph) == json.loads((base / "trials.json").read_text())["training"]["sha256"]
        result = benchmark(args.dev_input, graph, bins, work, ref["test_sha256"], args.release)
        save(HERE / "development.json", {**result, **summarize(result)})
        return
    inputs = frozen["inputs"]
    if not (work/"train.csv").exists() or not (work/"test.csv").exists():
        assert partitions(args.archive,work) == inputs
    for part in ("train","test"):
        assert digest(work/(part+".csv")) == inputs[part+"_sha256"]
    with tempfile.TemporaryDirectory(dir=work) as tmp:
        graph = Path(tmp) / "portugal.zlc"
        training = measure([bins["openzl"],"train","--profile","csv","--profile-arg",";","--threads","4",
                            "--max-time-secs","10",work/"train.csv","--output",graph])
        print("OpenZL training complete", training, flush=True)
        result = benchmark(work/"test.csv", graph, bins, work, inputs["test_sha256"], args.release)
        save(HERE / "trials.json", dict(training={**training,"sha256":digest(graph),"bytes":graph.stat().st_size},**result))
        shutil.copyfile(graph, HERE / "portugal.zlc")
    save(HERE / "decision.json", summarize(result))


if __name__ == "__main__":
    main()
