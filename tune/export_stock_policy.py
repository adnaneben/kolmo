#!/usr/bin/env python3
"""Export the stock native encoder when an accepted policy has no routes."""
import argparse
import json
import os
from pathlib import Path
import shutil
import tempfile

from encoder_policy import accept_artifact, digest, identity, load, object_digest, require


def export(args):
    directory = args.experiment
    policy = accept_artifact(load(directory / "accepted.json"), "policy")
    require(policy["routes"] == [], "nonempty policy requires the routing wrapper")

    draft = accept_artifact(load(directory / "draft.json"), "draft")
    require(policy["draft"] == draft["id"] and draft["config"] == policy["config"],
            "accepted policy differs from its validation draft")
    fingerprints = identity(args)
    require(draft["fingerprints"] == policy["fingerprints"] == fingerprints,
            "encoder, decoder, catalogue, policy tool or machine differs from validation")

    seal_path = directory / "seal.json"
    seal = load(seal_path)
    require(seal["schema"] == 1 and seal["kind"] == "seal"
            and policy["seal"] == draft["seal"] == digest(seal_path),
            "policy or draft differs from the split seal")
    for split, artifact in (("development", draft), ("holdout", policy)):
        manifest = directory / (split + ".json")
        receipts = load(directory / (split + "-receipts.json"))
        require(digest(manifest) == seal[split] and object_digest(receipts) == artifact["receipts_sha256"],
                f"{split} manifest or receipts differ from validated evidence")
        if split == "holdout":
            expected = {(r["sha256"], r["origin"]) for r in load(manifest)}
            seen = {(r["sample_sha256"], r["origin"]) for r in receipts}
            require(expected and seen == expected and len(receipts) == len(expected)
                    and all(r.get("dispatch_confirmed") is True for r in receipts),
                    "every holdout needs one verified deployment dispatch")
    require(args.kolmo.is_file() and os.access(args.kolmo, os.X_OK), "encoder must be executable")
    require(args.decoder.is_file() and os.access(args.decoder, os.X_OK), "reader must be executable")
    # Publish only a complete verified image; concurrent exports or existing files win.
    fd, temporary = tempfile.mkstemp(prefix=".kolmo-export-", dir=args.output.parent)
    try:
        with os.fdopen(fd, "wb") as dst, args.kolmo.open("rb") as src:
            shutil.copyfileobj(src, dst)
            dst.flush()
            os.fchmod(dst.fileno(), os.fstat(src.fileno()).st_mode & 0o755)
            os.fsync(dst.fileno())
        require(digest(Path(temporary)) == fingerprints["encoder"],
                "encoder changed during export")
        os.link(temporary, args.output)
    finally:
        os.unlink(temporary)

    return {"policy_id": policy["id"], "encoder_sha256": fingerprints["encoder"],
            "decoder_sha256": fingerprints["decoder"], "output": str(args.output),
            "level": policy["config"]["level"], "threads": policy["config"]["threads"]}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("experiment", "kolmo", "decoder", "output"):
        p.add_argument("--" + name, type=Path, required=True)
    try:
        print(json.dumps(export(p.parse_args()), indent=2))
    except (OSError, ValueError, TypeError, KeyError) as error:
        p.exit(1, f"export-stock-policy: {error}\n")


if __name__ == "__main__":
    main()
