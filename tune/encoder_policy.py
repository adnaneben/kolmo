#!/usr/bin/env python3
"""Opt-in local encoder policy for Kolmo's stock v12 CLI."""
import argparse
import bisect
import csv
import hashlib
import json
import os
from pathlib import Path
import platform
import random
import re
import statistics
import struct
import sys
import tempfile
import time

SCRIPT = Path(__file__).resolve()
CATALOGUE = SCRIPT.with_name("format-catalogue.v1.json")
MODEL_HEADER_LIMIT = 64 * 1024
TEXT_PROBE_LIMIT = 64 * 1024
TEXT_MIN_RECORDS = 3
WIRE_HEADER = b"CZN1\x0c"
ANALYSIS = ("none", "minimal", "quick", "full")
METRICS = ("encode_ns", "decode_ns", "encode_rss", "decode_rss")
LIMITS = ("max_encode_time", "max_decode_time", "max_encode_rss", "max_decode_rss")
HEX_DIGEST = re.compile(r"[0-9a-f]{64}\Z")

TEXT_CONTROLS = re.compile(r"[\x00-\x08\x0b-\x1f\x7f-\x9f]")
TABLE_HEADER = re.compile(r"[^\W\d][\w .()/%-]{0,63}\Z")
TABLE_NUMBER = re.compile(r"[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\Z")
FASTQ_SEQUENCE = re.compile(r"[ACGTURYSWKMBDHVN]+\Z", re.IGNORECASE | re.ASCII)
FASTQ_PRINTABLE = re.compile(r"[ -~]+\Z")
FASTQ_QUALITY = re.compile(r"[!-~]+\Z")


def load(path):
    with path.open("rb") as src:
        return json.load(src)


def digest(path):
    sha = hashlib.sha256()
    with path.open("rb") as src:
        for chunk in iter(lambda: src.read(1024 * 1024), b""):
            sha.update(chunk)
    return sha.hexdigest()


def object_digest(value):
    data = json.dumps(value, ensure_ascii=True, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(data.encode()).hexdigest()


def save(path, value):
    # Refuse to overwrite evidence or a previously accepted policy by accident.
    fd, temporary = tempfile.mkstemp(prefix=".encoder-policy-", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as dst:
            json.dump(value, dst, indent=2, sort_keys=True, allow_nan=False)
            dst.write("\n")
        os.link(temporary, path)
    finally:
        os.unlink(temporary)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def soft_model_header(src, size):
    """Identify small, complete SafeTensors headers. Unknown is always safe."""
    raw = src.read(8)
    if len(raw) != 8:
        return False
    length = struct.unpack("<Q", raw)[0]
    if not 2 <= length <= min(MODEL_HEADER_LIMIT, size - 8):
        return False
    try:
        header = json.loads(src.read(length))
    except (UnicodeError, ValueError, RecursionError):
        return False
    if not isinstance(header, dict):
        return False
    intervals = []
    for name, tensor in header.items():
        if name == "__metadata__":
            continue
        if not isinstance(tensor, dict):
            return False
        dtype, shape, offsets = (tensor.get(k) for k in ("dtype", "shape", "data_offsets"))
        if not (isinstance(dtype, str) and dtype and isinstance(shape, list)
                and all(type(n) is int and n >= 0 for n in shape)
                and isinstance(offsets, list) and len(offsets) == 2
                and all(type(n) is int for n in offsets)
                and 0 <= offsets[0] <= offsets[1] <= size - 8 - length):
            return False
        intervals.append(offsets)
    cursor = 0
    for lo, hi in sorted(intervals):
        if lo != cursor:
            return False
        cursor = hi
    return bool(intervals) and cursor == size - 8 - length


def sampled_text(src, size):
    """Read a bounded prefix and return complete UTF-8 lines, without line endings."""
    src.seek(0)
    sample = src.read(TEXT_PROBE_LIMIT)
    truncated = len(sample) < size
    raw_lines = sample.split(b"\n")
    if truncated or raw_lines[-1] == b"":
        raw_lines.pop()
    lines = [line.removesuffix(b"\r").decode("utf-8") for line in raw_lines]
    if any(TEXT_CONTROLS.search(line) for line in lines):
        return [], truncated
    return lines, truncated


def reject_json_constant(value):
    raise ValueError(f"invalid JSON constant: {value}")


def jsonl_text(lines):
    return all(isinstance(json.loads(line, parse_constant=reject_json_constant), dict)
               for line in lines)


def table_text(lines, delimiter):
    rows = list(csv.reader(lines, delimiter=delimiter, strict=True))
    if len(rows) != len(lines) or len(rows) < TEXT_MIN_RECORDS + 1:
        return False
    header = [name.strip() for name in rows[0]]
    width = len(header)
    if (width < 2 or len({name.casefold() for name in header}) != width
            or not all(TABLE_HEADER.fullmatch(name) for name in header)
            or any(len(row) != width for row in rows)):
        return False
    return any(all(TABLE_NUMBER.fullmatch(value.strip()) for value in column)
               for column in zip(*rows[1:]))


def fastq_text(lines, truncated):
    records, tail = divmod(len(lines), 4)
    if records < TEXT_MIN_RECORDS or (tail and not truncated):
        return False
    for i in range(0, records * 4, 4):
        name, sequence, separator, quality = lines[i:i + 4]
        if (not name.startswith("@") or len(name) == 1 or not FASTQ_PRINTABLE.fullmatch(name)
                or separator not in ("+", "+" + name[1:])
                or not FASTQ_SEQUENCE.fullmatch(sequence)
                or len(sequence) != len(quality) or not FASTQ_QUALITY.fullmatch(quality)):
            return False
    return True


def classify_text(src, size, entries):
    """Return a routing hint only when exactly one configured probe matches."""
    probes = [entry for entry in entries if "text_probe" in entry]
    if not probes:
        return "unknown"
    try:
        lines, truncated = sampled_text(src, size)
    except UnicodeError:
        return "unknown"
    if len(lines) < TEXT_MIN_RECORDS:
        return "unknown"
    validators = {
        "jsonl": lambda: jsonl_text(lines),
        "csv": lambda: table_text(lines, ","),
        "tsv": lambda: table_text(lines, "\t"),
        "fastq": lambda: fastq_text(lines, truncated),
    }
    matches = []
    for entry in probes:
        try:
            if validators[entry["text_probe"]]():
                matches.append(entry["id"])
        except (ValueError, RecursionError, csv.Error):
            continue
    return matches[0] if len(matches) == 1 else "unknown"


def classify(path, catalogue):
    with path.open("rb") as src:
        size = os.fstat(src.fileno()).st_size
        head = src.read(16)
        for entry in catalogue["formats"]:
            if entry.get("header") == "safetensors":
                src.seek(0)
                if soft_model_header(src, size):
                    return entry["id"], size
            if "prefix" not in entry or not head.startswith(bytes.fromhex(entry["prefix"])):
                continue
            if "signature" in entry:
                signature = bytes.fromhex(entry["signature"])
                if not head.startswith(signature, entry["offset"]):
                    continue
            if "suffix" in entry:
                suffix = bytes.fromhex(entry["suffix"])
                if size < 2 * len(suffix):
                    continue
                src.seek(-len(suffix), os.SEEK_END)
                if src.read(len(suffix)) != suffix:
                    continue
            return entry["id"], size
        return classify_text(src, size, catalogue["formats"]), size


def bucket(path, catalogue):
    kind, size = classify(path, catalogue)
    return kind, bisect.bisect_right(catalogue["size_cuts"], size)


def read_manifest(path):
    require(path.is_file(), f"missing manifest {path}")
    rows = load(path)
    require(isinstance(rows, list) and rows, "a manifest must be a non-empty JSON array")
    result, seen = [], set()
    for row in rows:
        require(isinstance(row, dict) and set(row) == {"path", "sha256", "origin"},
                "each sample needs path, sha256 and origin")
        name, hash_value, source = (row[k] for k in ("path", "sha256", "origin"))
        require(isinstance(name, str) and name and not Path(name).is_absolute(),
                "sample paths must be relative to the manifest")
        relative = Path(name)
        require(".." not in relative.parts and isinstance(hash_value, str)
                and HEX_DIGEST.fullmatch(hash_value) and isinstance(source, str)
                and source.strip() == source and source, "invalid manifest sample")
        source_path = path.parent / relative
        parent = path.parent.resolve()
        require(parent in source_path.resolve().parents, f"sample outside manifest root {name}")
        require(hash_value not in seen, f"duplicate sample digest {hash_value}")
        require(source_path.is_file() and digest(source_path) == hash_value,
                f"sample content does not match manifest {name}")
        seen.add(hash_value)
        result.append((source_path, hash_value, source))
    return result


def seal(args):
    dev = read_manifest(args.development)
    test = read_manifest(args.holdout)
    for field in (1, 2):
        require(not ({row[field] for row in dev} & {row[field] for row in test}),
                "development and holdout share sample content or an origin")
    save(args.output, {"schema": 1, "kind": "seal", "development": digest(args.development),
                       "holdout": digest(args.holdout)})


def split_samples(args, kind, labelled_seal):
    require(labelled_seal.get("schema") == 1 and labelled_seal.get("kind") == "seal",
            "unsupported split seal")
    require(digest(args.manifest) == labelled_seal[kind],
            "manifest changed after sealing")
    return read_manifest(args.manifest)


def choices(values):
    require(isinstance(values, list) and values, "provide at least one candidate")
    for value in values:
        require(isinstance(value, dict) and value and set(value) <= {"analysis", "block_mib"},
                "unsupported policy choice")
        if "analysis" in value:
            require(value["analysis"] in ANALYSIS, "unsupported analysis mode")
        if "block_mib" in value:
            require(type(value["block_mib"]) is int and 1 <= value["block_mib"] <= 4095,
                    "block_mib must be in 1..4095")
    require(len({object_digest(choice) for choice in values}) == len(values),
            "duplicate policy choices")
    return values


def configuration(path):
    config = load(path)
    require(isinstance(config, dict), "configuration must be an object")
    expected = {"schema", "level", "threads", "repetitions", "min_origins", "min_size_savings",
                "candidates", "max_roundtrip_time", *LIMITS}
    require(set(config) == expected and config["schema"] == 1, "invalid experiment configuration")
    require(config["level"] in [str(i) for i in range(1, 10)] + ["ultra"],
            "level must be a string in 1..9 or ultra")
    for key, lo, hi in (("threads", 1, 1024), ("repetitions", 3, 31), ("min_origins", 2, 10000)):
        require(type(config[key]) is int and lo <= config[key] <= hi, f"{key} outside {lo}..{hi}")
    for key in ("min_size_savings", "max_roundtrip_time", *LIMITS):
        value = config[key]
        require(type(value) in (float, int) and 1.0e-6 <= value <= 100.0,
                f"{key} must be a positive finite ratio at most 100")
    require(config["min_size_savings"] < 1, "minimum size savings must be below 1")
    choices(config["candidates"])
    return config


def environment():
    # Prevent ambient codec settings from changing a measured or applied choice.
    return {k: v for k, v in os.environ.items() if not k.startswith(("KOLMO_", "CZN_"))}


def machine():
    uname = platform.uname()
    host = hashlib.sha256(uname.node.encode()).hexdigest()
    return [uname.system, uname.release, uname.machine, platform.processor(), os.cpu_count(), host]


def encode_command(binary, level, threads, choice, path, output):
    cmd = [str(binary), "--level", level, "-T", str(threads), "-q", "-o", str(output)]
    if "analysis" in choice:
        cmd.extend(("--analysis", choice["analysis"]))
    if "block_mib" in choice:
        cmd.extend(("--block-size", str(choice["block_mib"])))
    return cmd + ["--", str(path)]


def child(command):
    start = time.monotonic_ns()
    pid = os.posix_spawn(command[0], command, environment())
    _, status, usage = os.wait4(pid, 0)
    elapsed = time.monotonic_ns() - start
    require(os.WIFEXITED(status) and os.WEXITSTATUS(status) == 0,
            f"codec process failed {command[0]}")
    # Darwin reports bytes and Linux reports KiB.
    rss = usage.ru_maxrss * (1 if sys.platform == "darwin" else 1024)
    require(rss > 0, "peak process RSS unavailable")
    return elapsed, rss


def measure(binary, decoder, config, candidates, sample, frame, restored, rng):
    path, expected_hash, _ = sample
    runs = [[] for _ in candidates]
    for _ in range(config["repetitions"]):
        for i in rng.sample(range(len(candidates)), len(candidates)):
            encode_ns, encode_rss = child(encode_command(
                binary, config["level"], config["threads"], candidates[i], path, frame))
            with frame.open("rb") as packed:
                require(packed.read(len(WIRE_HEADER)) == WIRE_HEADER, "encoder did not write v12")
            decode_ns, decode_rss = child([
                str(decoder), "-d", "-T", str(config["threads"]), "-q", "-o", str(restored),
                "--", str(frame)
            ])
            require(digest(restored) == expected_hash, f"stock decoder round trip failed {path.name}")
            runs[i].append((frame.stat().st_size, encode_ns, decode_ns, encode_rss, decode_rss, digest(frame)))
            frame.unlink()
            restored.unlink()
    require(digest(path) == expected_hash, f"sample changed during replay {path.name}")
    observations = []
    for series in runs:
        require(len({row[-1] for row in series}) == 1, "encoder output changed across rounds")
        medians = [statistics.median(row[i] for row in series) for i in range(5)]
        observations.append(dict(zip(("bytes", *METRICS), medians)))
    return observations


def passes(baseline, candidate, config):
    if candidate["bytes"] > baseline["bytes"] * (1 - config["min_size_savings"]):
        return False
    total = baseline["encode_ns"] + baseline["decode_ns"]
    if candidate["encode_ns"] + candidate["decode_ns"] > total * config["max_roundtrip_time"]:
        return False
    return all(candidate[field] <= baseline[field] * config[limit]
               for field, limit in zip(METRICS, LIMITS))


def score(args, config, samples, catalogue, candidate_routes):
    # Independent origins are counted inside each eligible format and size bucket.
    groups = {}
    for sample in samples:
        key = bucket(sample[0], catalogue)
        groups.setdefault(key, []).append(sample)
    receipts, routes = [], []
    rng = random.Random(6013409)
    with tempfile.TemporaryDirectory(prefix="kolmo-policy-") as tmp:
        frame, restored = Path(tmp) / "frame.kolmo", Path(tmp) / "restored"
        for key, group in sorted(groups.items()):
            candidates = candidate_routes.get(key)
            origins = len({sample[2] for sample in group})
            if not candidates or key[0] == "unknown" or origins < config["min_origins"]:
                continue
            accepted = [True] * len(candidates)
            totals = [0] * len(candidates)
            for sample in group:
                baseline, *variants = measure(args.kolmo, args.decoder, config, [{}, *candidates],
                                               sample, frame, restored, rng)
                for i, variant in enumerate(variants):
                    improves = passes(baseline, variant, config)
                    accepted[i] &= improves
                    totals[i] += variant["bytes"]
                    receipts.append({"format": key[0], "bucket": key[1],
                                     "sample_sha256": sample[1], "input_bytes": sample[0].stat().st_size,
                                     "choice": candidates[i],
                                     "baseline": baseline, "candidate": variant,
                                     "accepted": improves})
            winners = [i for i in range(len(candidates)) if accepted[i]]
            if winners:
                best = min(winners, key=totals.__getitem__)
                routes.append({"format": key[0], "bucket": key[1], "choice": candidates[best],
                               "samples": len(group), "origins": origins})
    return routes, receipts


def identity(args):
    return {"encoder": digest(args.kolmo), "decoder": digest(args.decoder),
            "catalogue": digest(CATALOGUE), "policy_tool": digest(SCRIPT), "machine": machine()}


def experiment(args, validating):
    require(sys.platform in ("linux", "darwin"), "policy measurement needs Linux or macOS")
    catalogue = load(CATALOGUE)
    labelled_seal = load(args.seal)
    part = "holdout" if validating else "development"
    samples = split_samples(args, part, labelled_seal)
    config = configuration(args.config)
    fingerprints = identity(args)
    seal_hash = digest(args.seal)
    if validating:
        draft = accept_artifact(load(args.draft), "draft")
        require(draft["fingerprints"] == fingerprints and draft["config"] == config
                and draft["seal"] == seal_hash, "draft does not match this scoped experiment")
        candidates = {(r["format"], r["bucket"]): [r["choice"]] for r in draft["routes"]}
    else:
        candidates = {bucket(sample[0], catalogue): config["candidates"] for sample in samples}
    routes, receipts = score(args, config, samples, catalogue, candidates)
    require(identity(args) == fingerprints, "binary, catalogue or machine changed during replay")
    result = {"schema": 1, "kind": "policy" if validating else "draft", "fingerprints": fingerprints,
              "seal": seal_hash, "config": config, "routes": routes,
              "receipts_sha256": object_digest(receipts)}
    if validating:
        result["draft"] = draft["id"]
    result["id"] = object_digest(result)
    save(args.receipts, receipts)
    save(args.output, result)


def accept_artifact(artifact, kind):
    require(isinstance(artifact, dict) and artifact.get("schema") == 1
            and artifact.get("kind") == kind, f"expected a v1 {kind} artifact")
    content = dict(artifact)
    require(content.pop("id", None) == object_digest(content), "artifact identity mismatch")
    for route in artifact["routes"]:
        choices([route["choice"]])
    return artifact


def encode(args):
    choice = {}
    if args.policy:
        policy = accept_artifact(load(args.policy), "policy")
        scope = policy["config"]
        profile = policy["fingerprints"]
        if (scope["level"], scope["threads"]) == (args.level, args.threads) and (
            profile["machine"] == machine() and profile["catalogue"] == digest(CATALOGUE)
            and profile["encoder"] == digest(args.kolmo) and profile["policy_tool"] == digest(SCRIPT)
        ):
            key = bucket(args.input, load(CATALOGUE))
            choice = next((r["choice"] for r in policy["routes"]
                           if (r["format"], r["bucket"]) == key), {})
    argv = encode_command(args.kolmo, args.level, args.threads, choice, args.input, args.output)
    os.execve(argv[0], argv, environment())


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="command", required=True)
    seal_cmd = sub.add_parser("seal")
    for name in ("development", "holdout", "output"):
        seal_cmd.add_argument("--" + name, type=Path, required=True)
    for stage in ("learn", "validate"):
        cmd = sub.add_parser(stage)
        for name in ("kolmo", "decoder", "manifest", "seal", "config", "output", "receipts"):
            cmd.add_argument("--" + name, type=Path, required=True)
        if stage == "validate":
            cmd.add_argument("--draft", type=Path, required=True)
    run = sub.add_parser("encode")
    run.add_argument("--kolmo", type=Path, required=True)
    run.add_argument("--level", default="3", choices=[str(i) for i in range(1, 10)] + ["ultra"])
    run.add_argument("--threads", type=int, default=1)
    run.add_argument("--policy", type=Path)
    run.add_argument("input", type=Path)
    run.add_argument("output", type=Path)
    return p


def main():
    args = parser().parse_args()
    try:
        if args.command == "seal":
            seal(args)
        else:
            args.kolmo = args.kolmo.resolve(strict=True)
            if args.command == "encode":
                require(args.threads >= 1 and args.input.is_file(),
                        "encode requires a positive thread count and a regular input file")
                encode(args)
            else:
                args.decoder = args.decoder.resolve(strict=True)
                experiment(args, args.command == "validate")
    except (OSError, ValueError, TypeError, KeyError, RecursionError) as exc:
        print(f"encoder-policy: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
