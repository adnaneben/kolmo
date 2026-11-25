"""Measure whether marker-only or grouping-only changes explain the frame-size effect."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def sha(data):
    return hashlib.sha256(data).hexdigest()


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("observations", type=Path)
parser.add_argument("kolmo")
parser.add_argument("transform")
parser.add_argument("output", type=Path)
args = parser.parse_args()
rows = list(map(json.loads, args.observations.read_text().splitlines()))
sources = {r["file"]: Path(r["source_path"]) for r in rows}
with tempfile.TemporaryDirectory() as tmp, args.output.open("x") as output:
    root = Path(tmp)
    framed, restored, ablated = root/"grouped", root/"restored", root/"ablated"
    archive, decoded = root/"frame", root/"decoded"
    for file, source in sources.items():
        orig = source.read_bytes()
        subprocess.run([args.transform, "group", source, framed], check=True)
        transformed = framed.read_bytes()
        assert transformed[:4] == b"KWS1"
        for variant, data in [("marker-only", b"KWS1" + orig), ("group-only", transformed[4:])]:
            for level in [1, 3]:
                ablated.write_bytes(data)
                subprocess.run([args.kolmo, f"-{level}", "-T", "1", "-f", "-o", archive, ablated], check=True)
                subprocess.run([args.kolmo, "-d", "-T", "1", "-f", "-o", decoded, archive], check=True)
                actual = decoded.read_bytes()
                assert data == actual
                if variant == "marker-only":
                    result = actual[4:]
                else:
                    framed.write_bytes(b"KWS1" + actual)
                    subprocess.run([args.transform, "restore", framed, restored], check=True)
                    result = restored.read_bytes()
                assert sha(result) == sha(orig)
                r = dict(file=file, level=level, variant=variant, input_sha256=sha(orig),
                         readback_sha256=sha(result), payload_sha256=sha(data), frame_bytes=archive.stat().st_size,
                         frame_sha256=sha(archive.read_bytes()))
                output.write(json.dumps(r) + "\n")
