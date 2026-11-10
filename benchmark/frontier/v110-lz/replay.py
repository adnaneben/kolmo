"""Run the pinned v1.1.0 source through the frozen one-worker Silesia harness on macOS."""

import argparse
import csv
import fcntl
import gzip
import hashlib
import io
import json
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SOURCE = "87dfc86b2665cac637c98179fb8afa8db3eeb668"
BOARD = "a667bac80089b3d04d5496772149e632f0758974b0579d743cd8d6e5e0d63eb2"
# Each pair gives the frozen harness setting followed by the official 1.1.0 level.
MODES = {"rt": (0, 0), "fast": (1, 4), "eco": (2, 8), "hc": (3, 9), "max": (4, 10), "ultra": (5, 12)}
RIVALS = {"zstd3": "zstd,3", "lz4": "lz4", "openzl": "openzl_generic"}
STAGES = {
    "short": ["fast", "zstd3", "rt", "lz4", "eco", "openzl"],
    "slow": ["hc", "max"],
    "ultra": ["ultra"],
    "ultra-screen": ["ultra"],
}


def digest(path):
    checksum = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            checksum.update(chunk)
    return checksum.hexdigest()


def check_source(release):
    assert (release / "VERSION").read_text().strip() == "1.1.0"
    entries = subprocess.check_output([
        "git", "ls-tree", "-r", "-z", SOURCE, "Makefile", "VERSION", "src", "vendor",
    ], cwd=ROOT).decode().split("\0")
    for entry in filter(None, entries):
        metadata, name = entry.split("\t", 1)
        _, object_type, expected_hash = metadata.split()
        assert object_type == "blob"
        contents = (release / name).read_bytes()
        git_blob = f"blob {len(contents)}\0".encode() + contents
        if hashlib.sha1(git_blob).hexdigest() != expected_hash:
            raise ValueError(f"Release source differs from GitHub: {name}")


def json_save(path, data):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n")
    temporary.replace(path)


def setup(work, release):
    private = work / "build"
    private.mkdir(exist_ok=True, parents=True)
    check_source(release)
    panel = json.loads((HERE.parent / "panel-manifest.json").read_text())
    frozen = work.parent / "frozen/lzbench"
    assert digest(frozen) == panel["lzbench"]
    library = private / "libkolmo-v110.dylib"
    shutil.copy(frozen, work / "lzbench")
    shutil.copy(release / "build/libkolmo.dylib", library)
    bridge = private / "libkolmo.dylib"
    subprocess.run([
        "cc", "-O3", "-dynamiclib", "-I" + str(release / "src"), HERE / "abi_bridge.c", library,
        "-Wl,-headerpad_max_install_names", "-Wl,-install_name,@loader_path/libkolmo-bridge.dylib",
        "-o", bridge,
    ], check=True)
    subprocess.run([
        "install_name_tool", "-change", "build/libkolmo.dylib", "@loader_path/libkolmo-v110.dylib", bridge,
    ], check=True)
    subprocess.run(["codesign", "--force", "--sign", "-", bridge], check=True, capture_output=True)
    linkage = subprocess.check_output(["otool", "-L", bridge], text=True)
    assert "@loader_path/libkolmo-v110.dylib" in linkage
    manifest = {
        "source": SOURCE,
        "tag_status": "VERSION=1.1.0 on GitHub main; public releases/tags end at v1.0.1 at verification",
        "harness": digest(work / "lzbench"),
        "library": digest(library),
        "bridge": digest(bridge),
        "bridge_source": digest(HERE / "abi_bridge.c"),
        "cli": digest(release / "build/kolmo"),
        "official_mode_levels": {name: levels[1] for name, levels in MODES.items()},
        "frozen_harness_mode_levels": {name: levels[0] for name, levels in MODES.items()},
    }
    manifest_path = work / "manifest.json"
    if any((work / "rows").glob("*.json")) and json.loads(manifest_path.read_text()) != manifest:
        raise ValueError("Existing rows belong to a different build. Choose a fresh --work directory.")
    json_save(manifest_path, manifest)


def row(work, source, kind, counts):
    setting = f"kolmo,{MODES[kind][0]}" if kind in MODES else RIVALS[kind]
    flags = [f"-e{setting}", "-I1", "-T1", "-t0,0", f"-i{counts}", "-p3", "-o4", "-q"]
    started = time.time()
    proc = subprocess.run([work / "lzbench", *flags, source], cwd=work, text=True, capture_output=True)
    elapsed = time.time() - started
    rows = list(csv.DictReader(io.StringIO(proc.stdout)))
    if proc.returncode or len(rows) != 1:
        raise RuntimeError(proc.stderr + proc.stdout)
    result = rows[0]
    original = source.stat().st_size
    size = int(result["Compressed size"])
    assert int(result["Original size"]) == original and size > 0
    assert all(float(result[name]) > 0 for name in ("Compression speed", "Decompression speed"))
    return {
        "codec": kind, "setting": setting, "official_level": MODES[kind][1] if kind in MODES else None,
        "flags": flags, "original": original, "bytes": size, "ratio": original / size,
        "encode": float(result["Compression speed"]), "decode": float(result["Decompression speed"]),
        "valid": True, "started": started, "elapsed_s": elapsed,
        "raw": proc.stdout.replace(str(source), source.name), "stderr": proc.stderr,
    }


def verify(work, release, sample):
    checks = {}
    cli = release / "build/kolmo"
    with tempfile.TemporaryDirectory(dir=work) as temporary:
        folder = Path(temporary)
        fixture = folder / "fixture"
        with sample.open("rb") as stream:
            fixture.write_bytes(stream.read(65536))
        original_hash = digest(fixture)
        for mode in MODES:
            frame = folder / (mode + ".frame")
            restored = folder / (mode + ".out")
            result = row(work, fixture, mode, "1,1")
            subprocess.run([cli, "-T", "1", "-m", mode, fixture, frame], check=True, capture_output=True)
            subprocess.run([cli, "-T", "1", "-d", frame, restored], check=True, capture_output=True)
            assert result["bytes"] == frame.stat().st_size
            restored_hash = digest(restored)
            assert restored_hash == original_hash
            checks[mode] = {
                "test_bytes": fixture.stat().st_size, "source_sha256": original_hash,
                "cli_frame_bytes": frame.stat().st_size, "cli_frame_sha256": digest(frame),
                "harness_bytes": result["bytes"], "verified_decoded_sha256": restored_hash,
            }
    return checks


def publish(work, evidence):
    with (work / "publish.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        paths = sorted((work / "rows").glob("*.json"))
        data = {
            "manifest": json.loads((work / "manifest.json").read_text()), "source_sha256": BOARD,
            "parity": json.loads((work / "parity.json").read_text()),
            "receipts": {path.stem: json.loads(path.read_text()) for path in paths},
        }
        temporary = evidence.with_suffix(".tmp")
        payload = (json.dumps(data, sort_keys=True, indent=2) + "\n").encode()
        temporary.write_bytes(gzip.compress(payload, mtime=0))
        temporary.replace(evidence)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("phase", choices=["prepare", *STAGES])
    parser.add_argument("board", type=Path)
    parser.add_argument("--work", type=Path, default=ROOT / "build/frontier/v110-lz")
    parser.add_argument("--release", type=Path, default=ROOT / "build/frontier/github-110")
    parser.add_argument("--evidence", type=Path, default=HERE / "replay.json.gz")
    args = parser.parse_args()
    work = args.work.resolve()
    release = args.release.resolve()
    source = args.board.resolve()
    assert digest(source) == BOARD
    lockfile = work.parent / "timing.lock"
    if args.phase == "prepare":
        with lockfile.open("a") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            setup(work, release)
            panel = json.loads((HERE.parent / "panel-manifest.json").read_text())
            sample = work.parent / "samples/dickens"
            assert digest(sample) == panel["samples"]["dickens"]["sha256"]
            json_save(work / "parity.json", verify(work, release, sample))
        return
    manifest = json.loads((work / "manifest.json").read_text())
    assert digest(work / "lzbench") == manifest["harness"]
    assert digest(work / "build/libkolmo.dylib") == manifest["bridge"]
    assert digest(work / "build/libkolmo-v110.dylib") == manifest["library"] == digest(release / "build/libkolmo.dylib")
    assert digest(release / "build/kolmo") == manifest["cli"]
    screening = args.phase == "ultra-screen"
    folder = work / "rows"
    folder.mkdir(exist_ok=True)
    for codec in STAGES[args.phase]:
        suffix = "-screen" if screening else ""
        dest = folder / (codec + suffix + ".json")
        if dest.exists():
            continue
        with lockfile.open("a") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            if dest.exists():
                continue
            result = row(work, source, codec, "1,1" if screening else "3,5")
            json_save(dest, result)
        publish(work, args.evidence)
        print(codec, result["bytes"], result["encode"], result["decode"], flush=True)


if __name__ == "__main__":
    main()
