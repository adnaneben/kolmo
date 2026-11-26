"""Verify the files actually shipped in a wheel, sdist or crate."""
import argparse
from pathlib import Path, PurePosixPath
import tarfile
import zipfile


ROOT = Path(__file__).resolve().parents[1]
NOTICES = {
    "LICENSE": ROOT / "LICENSE",
    "NOTICE": ROOT / "NOTICE",
    "LICENSE-libsais": ROOT / "src/algo/sais/LICENSE",
}


def check(artifact):
    if artifact.suffix == ".whl":
        with zipfile.ZipFile(artifact) as archive:
            paths = [PurePosixPath(name) for name in archive.namelist()]
            for path in paths:
                if path.suffix in {".o", ".obj"} or "_kolmo_build" in path.parts:
                    raise ValueError(f"{artifact.name} ships a compiler intermediate: {path}")
            for name, original in NOTICES.items():
                matches = [p for p in paths if p.name == name and p.parts[0].endswith(".dist-info")]
                if len(matches) != 1 or archive.read(str(matches[0])) != original.read_bytes():
                    raise ValueError(f"{artifact.name} has incorrect or missing {name}")
        return
    if artifact.suffix != ".crate" and not artifact.name.endswith(".tar.gz"):
        raise ValueError(f"unrecognized package: {artifact}")
    with tarfile.open(artifact) as archive:
        members = [p for p in archive.getmembers() if p.isfile()]
        roots = {PurePosixPath(p.name).parts[0] for p in members}
        if len(roots) != 1:
            raise ValueError(f"{artifact.name} has no single package root")
        root = roots.pop()
        for name, original in NOTICES.items():
            rel = "native/src/algo/sais/LICENSE" if name == "LICENSE-libsais" else name
            with archive.extractfile(f"{root}/{rel}") as member:
                if member.read() != original.read_bytes():
                    raise ValueError(f"{artifact.name} has incorrect {rel}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("artifacts", nargs="+", type=Path)
    for artifact in parser.parse_args().artifacts:
        check(artifact)
        print(f"{artifact.name}: package notices and contents passed")
