"""Verify that built Python distributions include exact copies of the project notices."""
import sys
import tarfile
import zipfile
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[2]


def check(archive):
    expected = {name: (PROJECT / name).read_bytes() for name in ("LICENSE", "NOTICE")}
    if archive.name.endswith(".whl"):
        with zipfile.ZipFile(archive) as wheel:
            for name, contents in expected.items():
                # Modern setuptools uses dist-info/licenses, older versions use dist-info.
                paths = [p for p in wheel.namelist() if p.endswith((".dist-info/licenses/" + name,
                                                                   ".dist-info/" + name))]
                if len(paths) != 1 or wheel.read(paths[0]) != contents:
                    raise ValueError(f"{archive.name} has missing or different {name}")
    else:
        with tarfile.open(archive, "r:gz") as sdist:
            for name, contents in expected.items():
                paths = [p for p in sdist if p.isfile() and p.name.split("/")[1:] == [name]]
                if len(paths) != 1 or sdist.extractfile(paths[0]).read() != contents:
                    raise ValueError(f"{archive.name} has missing or different {name}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit("Usage: check_license_files.py ARCHIVE ...")
    for filename in sys.argv[1:]:
        check(Path(filename))
