"""Assemble the two synthetic tar regression cases using fixed USTAR headers."""
import argparse
from pathlib import Path
import tarfile

GROUPS = {
    "structured": ("bsub.fa", "test.csv", "events.jsonl"),
    "binary": ("chinook.sqlite", "akiyo.y4m", "saut_page.docx"),
}


def prepare(name, sources, target):
    if len(sources) != len(GROUPS[name]):
        raise ValueError(f"{name} needs the files {GROUPS[name]} in that order")
    with target.open("xb") as stream, tarfile.open(fileobj=stream, mode="w", format=tarfile.USTAR_FORMAT) as tar:
        for member, path in zip(GROUPS[name], sources):
            info = tarfile.TarInfo(member)
            info.size = path.stat().st_size
            info.mode = 0o644
            with path.open("rb") as source:
                tar.addfile(info, source)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("case", choices=GROUPS)
    parser.add_argument("output", type=Path)
    parser.add_argument("input", type=Path, nargs=3)
    args = parser.parse_args()
    prepare(args.case, args.input, args.output)
