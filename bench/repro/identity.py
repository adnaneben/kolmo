"""Bind a resumable benchmark to its source, protocol, executables and data."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def tree_identity(root, paths):
    return {p.relative_to(root).as_posix(): digest(p)
            for p in sorted(set(paths)) if p.is_file()}


def tracked_ref(root, ref):
    return subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "--verify", "--end-of-options", ref + "^{commit}"],
        text=True).strip()


def worktree_source(root):
    paths = [root / p for p in ("Makefile", "experimental.mk", "VERSION", "LICENSE", "NOTICE")]
    paths.extend((root / "src").rglob("*"))
    paths.extend((root / "vendor").rglob("*"))
    return tree_identity(root, paths)


def arguments(root, ref, quick):
    protocol = [root / "site/build.py"]
    for directory in (root / "bench/repro", root / "bench/lzbench"):
        protocol.extend(p for p in directory.iterdir()
                        if p.suffix in (".py", ".sh", ".json", ".mk", ".cpp", ".txt"))
    environment_names = ("KOLMO_CPU", "KOLMO_MEMLIMIT", "KOLMO_NO_HUGEPAGE",
                         "KOLMO_MEM_FORCE_BUDGET", "KOLMO_BENCH_TIMED", "CC", "CXX",
                         "CFLAGS", "CPPFLAGS", "CXXFLAGS", "LDFLAGS", "SDKROOT",
                         "MACOSX_DEPLOYMENT_TARGET", "MAKEFLAGS", "LZBENCH_SRC", "OPENZL_SRC")
    settings = json.dumps({name: os.environ.get(name) for name in environment_names}, sort_keys=True)
    stage = {
        "schema": 1,
        "ref": tracked_ref(root, "HEAD" if ref == "WORKTREE" else ref),
        "worktree_source": worktree_source(root) if ref == "WORKTREE" else None,
        "quick": quick,
        "protocol": tree_identity(root, protocol),
        "machine": list(platform.uname()),
        "cores": os.cpu_count(),
        "python": digest(Path(sys.executable)),
        "system_python": digest(Path(shutil.which("python3"))),
        "environment_sha256": hashlib.sha256(settings.encode()).hexdigest(),
    }
    return stage


def binaries(results):
    listing = results / "tools.json"
    tools = json.loads(listing.read_text())
    paths = {name: tools[name]["path"] for name in ("kolmo", "lzbench", "openzl")}
    paths.update({name: spec["path"] or shutil.which(name) for name, spec in tools["cli"].items()})
    return {"manifest": digest(listing),
            "executables": {name: digest(Path(path)) if path else None
                            for name, path in sorted(paths.items())}}


def inputs(work, results):
    data = work / "data"
    # The runners consume derived corpora, holdouts and model files. Exclude
    # download cache and mutable transient fetch reports.
    paths = [entry for group in ("corpus", "holdout", "weights")
             for entry in (data / group).rglob("*")]
    # Retain the report displayed by the benchmark site as well.
    fetched = results / "FETCHED.json"
    return {"files": tree_identity(data, paths),
            "fetched": digest(fetched) if fetched.is_file() else None}


def check_receipt(path, actual, create=False):
    if path.exists():
        if json.loads(path.read_text()) != actual:
            raise ValueError(f"{path.name} changed. Use a new WORK directory for this run.")
        return
    if create:
        tmp = path.with_suffix(".part")
        tmp.write_text(json.dumps(actual, sort_keys=True, indent=2) + "\n")
        tmp.replace(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("prepare", "tools", "inputs"))
    parser.add_argument("root", type=Path)
    parser.add_argument("work", type=Path)
    parser.add_argument("ref")
    parser.add_argument("mode", choices=("quick", "full"))
    args = parser.parse_args()
    results = args.work / "results"
    run_receipt = results / "run-identity.json"
    tool_receipt = results / "tool-identity.json"
    input_receipt = results / "input-identity.json"
    if args.operation == "prepare":
        if not run_receipt.exists() and any(p.name != ".run.lock" for p in results.iterdir()):
            raise ValueError("results have no run identity. Use a new WORK directory to preserve those results.")
        identity = arguments(args.root, args.ref, args.mode == "quick")
        check_receipt(run_receipt, identity, create=True)
        if tool_receipt.exists():
            check_receipt(tool_receipt, binaries(results))
        if input_receipt.exists():
            check_receipt(input_receipt, inputs(args.work, results))
        print("WORKTREE" if args.ref == "WORKTREE" else identity["ref"])
    elif args.operation == "tools" and (results / "tools.json").exists() and not tool_receipt.exists():
        # A WORKTREE build must still correspond to the source captured before compilation.
        if args.ref == "WORKTREE":
            check_receipt(run_receipt, arguments(args.root, args.ref, args.mode == "quick"))
        check_receipt(tool_receipt, binaries(results), create=True)
    elif args.operation == "inputs" and not input_receipt.exists():
        check_receipt(input_receipt, inputs(args.work, results), create=True)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        sys.exit(f"benchmark identity: {error}")
