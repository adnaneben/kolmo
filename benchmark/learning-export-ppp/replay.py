#!/usr/bin/env python3
"""Replay the preregistered native export and wrapper on whole executables."""
import argparse
from pathlib import Path
import random
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tune"))
import encoder_policy as policy


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("inputs", "stock", "decoder", "exported", "output"):
        p.add_argument("--" + name, type=Path, required=True)
    args = p.parse_args()
    here = Path(__file__).resolve().parent
    frozen = policy.load(here / "freeze.json")
    model = ROOT / "benchmark/learning-speed-ppp/accepted.json"
    snapshot = policy.accept_artifact(policy.load(model), "policy")
    assert snapshot["id"] == frozen["policy_id"] and snapshot["routes"] == []
    encoder = args.stock.resolve(strict=True)
    reader = args.decoder.resolve(strict=True)
    native = args.exported.resolve(strict=True)
    assert snapshot["fingerprints"]["decoder"] == policy.digest(reader)
    assert snapshot["fingerprints"]["encoder"] == policy.digest(encoder) == policy.digest(native)
    result = {"policy_id": snapshot["id"], "encoder_sha256": policy.digest(native),
              "reader_sha256": policy.digest(reader), "rows": []}
    rng = random.Random(frozen["seed"])
    level, threads = str(frozen["level"]), str(frozen["threads"])
    with tempfile.TemporaryDirectory(prefix="kolmo-export-evaluation-") as tmp:
        frame, restored = Path(tmp) / "frame", Path(tmp) / "restored"
        for item in frozen["inputs"]:
            src = (args.inputs / item["name"]).resolve(strict=True)
            assert src.stat().st_size == item["bytes"] and policy.digest(src) == item["sha256"]
            reference = None
            for trial in range(1 + frozen["rounds"]):
                for order, arm in enumerate(rng.sample(frozen["arms"], len(frozen["arms"]))):
                    if arm == "wrapper":
                        cmd = [sys.executable, str(policy.SCRIPT), "encode", "--policy", str(model),
                               "--kolmo", str(encoder), "--level", level, "--threads", threads,
                               "--", str(src), str(frame)]
                    else:
                        cmd = policy.encode_command(encoder if arm == "stock" else native, level, threads,
                                                    {}, src, frame)
                    # No measurement fd: the wrapper wall timing uses its normal operator entry.
                    enc, _ = policy.child(cmd)
                    dec, _ = policy.child([str(reader), "-d", "-T", threads, "-q", "-o", str(restored),
                                           "--", str(frame)])
                    frame_hash = policy.digest(frame)
                    if reference is None:
                        reference = frame_hash
                    assert frame_hash == reference and policy.digest(restored) == item["sha256"]
                    result["rows"].append(dict(name=item["name"], trial=trial, order=order, arm=arm,
                                               bytes=frame.stat().st_size, frame_sha256=frame_hash,
                                               encode_ns=enc, decode_ns=dec))
                    frame.unlink()
                    restored.unlink()
    policy.save(args.output, result)


if __name__ == "__main__":
    main()
