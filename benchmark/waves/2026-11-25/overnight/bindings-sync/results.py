"""Hash the source distributions and outputs, verify frame metadata and decode through an independent stock CLI."""
import csv
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import tarfile
import tempfile

repo, builds, cli, out = [Path(v).resolve() for v in sys.argv[1:]]
data = (builds / "opaque-records").read_bytes()
def sha(data):
    return hashlib.sha256(data).hexdigest()
def filehash(path):
    return sha(path.read_bytes())

lines = data.splitlines(keepends=True)
assert len(lines) % 4 == 0
ends, total = set(), 0
for i in range(0, len(lines), 4):
    total += sum(map(len, lines[i:i+4]))
    ends.add(total)
assert total == len(data)
results = []
with tempfile.TemporaryDirectory() as td:
    for mode in ("stock", "opt"):
        for lang in ("python", "rust"):
            for toggle in ("off", "on"):
                frame = builds / mode / f"{lang}-runtime-{toggle}.frame"
                b = frame.read_bytes()
                dest = Path(td) / "restored"
                subprocess.run([cli, "-q", "-T2", "-d", "-o", dest, frame], check=True)
                assert dest.read_bytes() == data
                dest.unlink()
                n = struct.unpack_from("<I", b, 20)[0]
                entries = [struct.unpack_from("<IIIBxxx", b, 32+16*i) for i in range(n)]
                assert sum(e[0] for e in entries) == len(data)
                assert n == 2
                a, z = entries
                results.append(dict(language=lang, build=mode, runtime=toggle, input_bytes=len(data),
                                    input_sha256=sha(data), frame_bytes=len(b), frame_sha256=sha(b), blocks=n,
                                    first_cut=a[0], first_cut_on_record=a[0] in ends,
                                    first_codec=a[3], first_compressed_bytes=a[1], second_codec=z[3],
                                    second_compressed_bytes=z[1], stock_cli_t2_readback="exact"))
for row in results:
    opt = row["build"] == "opt" and row["runtime"] == "on"
    assert row["frame_sha256"] == ("9da97e1bcb76ee3b1dad4bb9a8800730850bb7d165f671fde4fad9b86d5f2074" if opt else "ca1d1847c171c12b0cc180d9815900335f73adf50d0afb37d92c3c4193057da8")
with (out / "matrix.csv").open("w") as f:
    w = csv.DictWriter(f, fieldnames=list(results[0]), lineterminator="\n")
    w.writeheader()
    w.writerows(results)
package_hashes = {}
for lang, path in (("python", next(builds.glob("sdist/*.tar.gz"))), ("rust", next(builds.glob("package/package/*.crate")))):
    files = subprocess.check_output(["git", "-C", str(repo), "ls-files", "-z", f"bindings/{lang}/native"]).decode().split("\0")[:-1]
    missing, mismatched = [], []
    with tarfile.open(path) as archive:
        contents = {item.name.split("/",1)[1]: item for item in archive if item.isfile() and "/" in item.name}
        for filename in files:
            relative = filename[len(f"bindings/{lang}/"):]
            if relative not in contents:
                missing.append(relative)
            elif archive.extractfile(contents[relative]).read() != (repo / filename).read_bytes():
                mismatched.append(relative)
    package_hashes[lang] = dict(source_package=path.name, sha256=filehash(path), tracked=len(files),
                                missing=missing, different=mismatched)
binary_hashes = []
for mode in ("stock", "opt"):
    root = builds/mode
    for kind,path in [("wheel", next(root.glob("wheels/*.whl"))), ("python_dylib",root/"site/libkolmo.dylib"),
                      ("rust_native_static",next(root.glob("target/debug/build/kolmo-*/out/libkolmo_native.a"))),
                      ("rust_consumer",root/"consumer")]:
        info = dict(build=mode, artifact=kind, filename=path.name, sha256=filehash(path))
        if kind in ("python_dylib", "rust_native_static"):
            info["runtime_gate_literal_in_binary"] = b"KOLMO_BENCH_READ_CUTS" in path.read_bytes()
            assert info["runtime_gate_literal_in_binary"] == (mode == "opt")
        binary_hashes.append(info)
(out/"packages.json").write_text(json.dumps(dict(sources=package_hashes, binaries=binary_hashes,
    stock_cli_sha256=filehash(cli)), indent=2)+"\n")
