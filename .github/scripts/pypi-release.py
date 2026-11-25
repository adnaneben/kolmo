#!/usr/bin/env python3
"""Check automated PyPI packages and stage only absent, digest-compatible files."""

import argparse
import hashlib
import json
import re
import shutil
import tarfile
import time
import urllib.error
import urllib.request
import zipfile
from email.parser import BytesParser
from pathlib import Path

ARCHES = ("x86_64", "aarch64")
DISTRIBUTIONS = ("sdist", *ARCHES)


def distribution(filename, version):
    base = f"kolmoc-{version}"
    if filename == base + ".tar.gz":
        return "sdist"
    match = re.fullmatch(re.escape(base) + r"-py3-none-([\w.]+)\.whl", filename, re.ASCII)
    if match:
        platforms = match[1].split(".")
        for arch in ARCHES:
            if all(tag.startswith("manylinux") and tag.endswith("_" + arch) for tag in platforms):
                return arch
    return None


def check_metadata(path, version, kind):
    base = f"kolmoc-{version}"
    if kind == "sdist":
        with tarfile.open(path, "r:gz") as archive:
            member, = [entry for entry in archive.getmembers() if entry.name == base + "/PKG-INFO"]
            if not member.isfile() or member.size > 1 << 20:
                raise ValueError(f"Invalid sdist metadata in {path.name}")
            content = archive.extractfile(member).read()
    else:
        with zipfile.ZipFile(path) as archive:
            member, = [entry for entry in archive.infolist() if entry.filename == base + ".dist-info/METADATA"]
            if member.file_size > 1 << 20:
                raise ValueError(f"Invalid wheel metadata in {path.name}")
            content = archive.read(member)
    metadata = BytesParser().parsebytes(content, headersonly=True)
    if metadata.get_all("Name") != ["kolmoc"] or metadata.get_all("Version") != [version]:
        raise ValueError(f"Package metadata disagrees with kolmoc {version} in {path.name}")


def check_packages(directory, version, expected):
    packages = {}
    for path in sorted(directory.iterdir()):
        kind = distribution(path.name, version)
        if not path.is_file() or path.is_symlink() or kind not in expected or kind in packages:
            raise ValueError(f"Unexpected or duplicate distribution: {path.name}")
        check_metadata(path, version, kind)
        packages[kind] = path
    if set(packages) != set(expected):
        raise ValueError(f"Missing distributions: {', '.join(sorted(set(expected) - packages.keys()))}")
    return packages


def published_digests(version):
    url = f"https://pypi.org/pypi/kolmoc/{version}/json"
    request = urllib.request.Request(url, headers={"Cache-Control": "no-cache"})
    try:
        with urllib.request.urlopen(request, timeout=30) as reply:
            published = json.load(reply)["urls"]
    except urllib.error.HTTPError as error:
        if error.code == 404:
            return {}
        raise
    existing = {}
    for entry in published:
        name = entry["filename"]
        kind = distribution(name, version)
        if kind is None:
            continue  # manual macOS wheels are outside this workflow
        if kind in existing:
            raise ValueError(f"Multiple published files for {kind}")
        existing[kind] = (name, entry["digests"]["sha256"])
    return existing


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def missing_packages(packages, existing):
    missing = []
    for kind, path in packages.items():
        if kind not in existing:
            missing.append(path)
        elif existing[kind] != (path.name, sha256(path)):
            raise ValueError(f"Published {kind} disagrees with this build: {path.name}")
    return missing


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("check", "prepare", "verify"))
    parser.add_argument("directory", type=Path)
    parser.add_argument("version")
    parser.add_argument("--kind", choices=DISTRIBUTIONS)
    parser.add_argument("--upload", type=Path)
    args = parser.parse_args()
    if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", args.version):
        parser.error("Expected release version X.Y.Z")
    if args.command != "check" and args.kind:
        parser.error("--kind applies only to check")
    packages = check_packages(args.directory, args.version, (args.kind,) if args.kind else DISTRIBUTIONS)
    if args.command == "prepare":
        if args.upload is None:
            parser.error("prepare requires --upload")
        missing = missing_packages(packages, published_digests(args.version))
        args.upload.mkdir()
        for path in missing:
            shutil.copyfile(path, args.upload / path.name)
    elif args.command == "verify":
        for attempt in range(6):
            missing = missing_packages(packages, published_digests(args.version))
            if not missing:
                return
            if attempt < 5:
                time.sleep(10)
        raise ValueError(f"PyPI does not list the complete set: {', '.join(path.name for path in missing)}")


if __name__ == "__main__":
    main()
