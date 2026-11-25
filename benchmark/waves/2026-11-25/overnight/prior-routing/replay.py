"""Run exact main and RC L8 stock / impact comparisons on the two published complete cuts."""
import argparse
import csv
import hashlib
import importlib.util
import json
import os
import platform
import random
import statistics
import subprocess
import tempfile
import time
from pathlib import Path

REVISIONS = {
    "main": "a1d39559686a9ec17c8614b9154a64d7b5b98672",
    "rc": "a2e430fed2fbc6dc357a78651cadf0e1fed611f2",
}
INPUTS = {
    "holdout/tsv__bb-rows": (4194123, "ce1f46c812afed1ec508438e80cd248939065e176251f6a6d672d76a228190b6"),
    "holdout/log__bgl": (4194269, "fba140ef27632a08d8304148c42a7f8ee9b09b724cd62d97e3d2d0cb4dd4eb61"),
}
SEED = 202611252145


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def measure(argv, env, output):
    with open(output, "wb") as out, tempfile.TemporaryFile() as err:
        actions = [(os.POSIX_SPAWN_DUP2, out.fileno(), 1), (os.POSIX_SPAWN_DUP2, err.fileno(), 2)]
        start = time.perf_counter_ns()
        pid = os.posix_spawn(argv[0], argv, env, file_actions=actions)
        _, status, usage = os.wait4(pid, 0)
        wall = (time.perf_counter_ns() - start) / 1e9
        err.seek(0)
        return status, wall, usage.ru_utime + usage.ru_stime, usage.ru_maxrss, err.read().decode(errors="replace")[-400:]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build_root", type=Path)
    parser.add_argument("corpus", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    for key in list(os.environ):
        if key.startswith("KOLMO_"):
            del os.environ[key]
    out = args.output.resolve()
    out.mkdir(exist_ok=True, parents=True)
    manifest = {"revisions": REVISIONS, "inputs": INPUTS, "seed": SEED, "level": 8, "threads": 1,
                "warmups": 1, "scored_rounds": 7, "host": platform.platform(), "compiler": subprocess.check_output(
                    ["cc", "--version"], text=True).splitlines()[0], "binaries": {}, "harnesses": {}}
    modules = {}
    builds = {}
    for revision in REVISIONS:
        project = args.build_root.resolve() / revision
        builds[revision] = {name: project / "build" / name for name in ("kolmo", "kolmo-impact")}
        runner = project / "benchmark/impact/run.py"
        manifest["binaries"][revision] = {name: digest(p) for name, p in builds[revision].items()}
        manifest["harnesses"][revision] = digest(runner)
        spec = importlib.util.spec_from_file_location("impact_" + revision, runner)
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        mod.timel = measure
        mod.CORPUS = str(args.corpus.resolve())
        mod.BIN_ID = REVISIONS[revision]
        mod.STOCK_BIN = str(builds[revision]["kolmo"])
        modules[revision] = mod
    assert len(set(manifest["harnesses"].values())) == 1
    records = {m["file"]: m for m in modules["main"].manifest()}
    for name, (n, sha) in INPUTS.items():
        p = args.corpus / name
        assert p.stat().st_size == n == records[name]["bytes"] and digest(p) == sha == records[name]["sha256"]
    manifest["trial_environment"] = modules["main"].F["stack-klzma"][0]
    manifest["timing"] = "wait4 child wall, user+system CPU, max RSS in bytes on macOS; historical cell rounds seconds to four decimals"
    (out / "exact-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    rng = random.Random(SEED)
    order = [(revision, source, arm) for revision in REVISIONS for source in INPUTS
             for arm in ("stock", "harness-base", "stack-klzma")]
    rows, decoder_checks = [], []
    with tempfile.TemporaryDirectory() as scratch, (out / "exact-raw.jsonl").open("w", buffering=1) as log:
        for number in range(8):
            rng.shuffle(order)
            for position, (revision, source, arm) in enumerate(order):
                mod = modules[revision]
                mod.WORK = scratch
                mod.IMPACT_BIN = str(builds[revision]["kolmo" if arm == "stock" else "kolmo-impact"])
                feature = "stack-klzma" if arm == "stack-klzma" else "base"
                row = mod.cell(feature, records[source], 8, 1)
                assert row.get("ok"), row
                assert row["decoder"] == "stock"
                row.update({"revision": revision, "arm": arm, "round": number, "position": position,
                            "source_sha256": INPUTS[source][1]})
                if number == 0:
                    for decoder, binaries in builds.items():
                        restored = subprocess.check_output([str(binaries["kolmo"]), "-d", "-T", "1", "-c",
                                                            str(Path(scratch) / "c.kolmo")])
                        assert hashlib.sha256(restored).hexdigest() == INPUTS[source][1]
                        decoder_checks.append(dict(source=source, encoder=revision, arm=arm, reader=decoder,
                                                   frame_sha256=row["frame_sha256"], recovered_sha256=INPUTS[source][1]))
                log.write(json.dumps(row) + "\n")
                rows.append(row)
    scores = []
    for revision, source, arm in sorted(set(order)):
        group = [r for r in rows if (r["revision"],r["file"],r["arm"]) == (revision,source,arm) and r["round"]]
        assert len(group) == 7 and len({r["frame_sha256"] for r in group}) == 1
        size = INPUTS[source][0]
        median = lambda key: statistics.median(r[key] for r in group)
        combined = statistics.median(r["c_wall"] + r["d_wall"] for r in group)
        scores.append(dict(revision=revision, source=source, arm=arm, input_bytes=size, frame_bytes=group[0]["size"],
                           frame_sha256=group[0]["frame_sha256"], enc_s=median("c_wall"), dec_s=median("d_wall"),
                           paired_combined_s=combined, enc_MBps=size/1e6/median("c_wall"),
                           dec_MBps=size/1e6/median("d_wall"), combined_MBps=size/1e6/combined,
                           enc_cpu_s=median("c_cpu"), dec_cpu_s=median("d_cpu"),
                           enc_peak_rss_bytes=median("c_rss"), dec_peak_rss_bytes=median("d_rss")))
    for revision in REVISIONS:
        for source in INPUTS:
            zero = [r["frame_sha256"] for r in scores if r["revision"]==revision and r["source"]==source and r["arm"]!="stack-klzma"]
            assert len(zero)==2 and len(set(zero))==1
    for filename, data in (("exact-score.csv", scores), ("exact-cross-read.csv", decoder_checks)):
        with (out/filename).open("w", newline="") as f:
            writer = csv.DictWriter(f, fieldnames=list(data[0]))
            writer.writeheader()
            writer.writerows(data)


if __name__ == "__main__":
    main()
