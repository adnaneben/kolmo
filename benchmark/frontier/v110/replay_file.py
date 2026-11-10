"""Replay a published comparison on local binaries and verified input bytes."""
import argparse
import gzip
import json
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
from file_trials import run
from panel import sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--case", help="for example release/eia or sampling/ce11")
    parser.add_argument("--source", type=Path, help="exact, uncompressed benchmark file")
    parser.add_argument("--codec", action="append", help="select a codec, repeat to compare several")
    parser.add_argument("--bin", metavar="NAME=PATH", action="append", default=[])
    parser.add_argument("--strict-binaries", action="store_true", help="require the archived binary digests")
    parser.add_argument("--output", type=Path, help="directory for the timing lock and resumable receipt")
    args = parser.parse_args()
    with gzip.open(HERE / "file-trials.json.gz", "rt") as stream:
        cases = json.load(stream)["cases"]
    if args.list:
        for case, row in cases.items():
            print(case, ", ".join(row["commands"]))
        return
    if args.case not in cases or not args.source or not args.output:
        parser.error("choose a case from --list and supply --source and --output")
    case = cases[args.case]
    source = args.source.resolve()
    if sha(source) != case["source_sha256"]:
        parser.error("input differs from the recorded file")
    binaries = {}
    for assignment in args.bin:
        name, sep, path = assignment.partition("=")
        if not sep or not name or not path:
            parser.error("--bin takes NAME=PATH")
        binaries["@" + name] = Path(path).resolve(strict=True)
    commands = {}
    for codec in args.codec or case["commands"]:
        if codec not in case["commands"]:
            parser.error(f"{codec} is unavailable for {args.case}")
        commands[codec] = {}
        for phase, argv in case["commands"][codec].items():
            exe = argv[0]
            if exe not in binaries:
                parser.error(f"{codec} requires --bin {exe[1:]}=PATH")
            if args.strict_binaries and sha(binaries[exe]) != case["binary_hashes"][exe]:
                parser.error(f"binary differs from archived build: {exe[1:]}")
            local = [str(binaries[exe])]
            for arg in argv[1:]:
                if arg == "@graph":
                    graph = HERE / case["graph"]["file"]
                    if sha(graph) != case["graph"]["sha256"]:
                        parser.error("trained graph differs from archived bytes")
                    local.append(str(graph))
                else:
                    local.append(arg)
            commands[codec][phase] = local
    work = args.output.resolve()
    work.mkdir(parents=True, exist_ok=True)
    result = work / (args.case.replace("/", "-") + ".json")
    run(source, commands, work, result, rounds=case["rounds"])
    if set(json.loads(result.read_text())["metrics"]) != set(commands):
        raise SystemExit("round-trip verification failed; inspect the receipt")


if __name__ == "__main__":
    main()
