#!/usr/bin/env python3
"""Release-facing documentation check (CI, and RELEASING.md step 1).

The PyPI and crates.io pages of a version are frozen once it is published, so what users read must be right before the
tag. 0.0.1 shipped package READMEs that still described 1.x (bundled liblzma, KOLMO_NO_LZMA, "from 1.0.1"); this check
exists so that cannot happen again.

It fails when:
  1. a release-facing file claims something 0.0.x removed: a bundled or vendored codec library, a build switch or
     feature that no longer exists, XXH3/xxHash checksums, dictionary training as a working example, or a 1.x / 1.2
     version as current. Lines that describe the removal or the history (they contain one of ALLOW) are accepted.
  2. the version labels disagree: VERSION, pyproject.toml, Cargo.toml, the CLI's KOLMO_VERSION_STR, the Homebrew
     formula's url, the top CHANGELOG heading and the README's Status line.

Usage: python3 tests/check_release_docs.py [path/to/kolmo]   (with the binary, its -h and -V are checked too)
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FILES = ["README.md", "bindings/python/README.md", "bindings/rust/README.md", "bindings/python/pyproject.toml",
         "bindings/rust/Cargo.toml", "packaging/homebrew/kolmo.rb", "NOTICE", "RELEASING.md", "benchmark/bench.sh"]
STALE = [
    (r"\bvendor(ed)?\b|\bvendor/", "a vendored library"),
    (r"liblzma|\bLZMA2\b", "liblzma / LZMA2 (0.0.x uses KLZMA)"),
    (r"KOLMO_NO_LZMA|\bNO_LZMA\b|no-lzma", "a build switch that no longer exists"),
    (r"\bXXH3\b|\bXXH64\b|xxHash", "xxHash (0.0.x uses kolmo_khash64)"),
    (r"depends_on \"xz\"", "an xz dependency"),
    (r"-t \S+\.dict|-D \S+\.dict|kolmo train\b", "dictionaries or profiles as a working example"),
    (r"\b1\.0\.[01]\b|\b1\.[12]\.0\b", "a 1.x version"),
    (r"container version (?:[0-9]|1[01])\b", "a container version 0.0.x does not write (it writes 12)"),
    (r"^keywords\s*=.*\b(zstd|brotli|lz4|lzma|xz)\b", "a library name as a package keyword"),
]
ALLOW = re.compile(r"\bno\b|\bnot\b|removed|no longer|were\b|was\b|earlier|history|yanked|1\.x|stays installable|"
                   r"rival|vs\b|than\b|compare|kolmoc==1\.0\.1|older|replace|instead|before", re.I)


def stale_lines():
    bad = []
    for rel in FILES:
        for n, line in enumerate((ROOT / rel).read_text(encoding="utf-8").splitlines(), 1):
            for pat, why in STALE:
                if re.search(pat, line) and not ALLOW.search(line):
                    bad.append(f"{rel}:{n}: {why}: {line.strip()[:140]}")
    return bad


def labels():
    v = (ROOT / "VERSION").read_text().strip()
    got = {
        "pyproject.toml": re.search(r'^version = "(.*)"', (ROOT / "bindings/python/pyproject.toml").read_text(), re.M),
        "Cargo.toml": re.search(r'^version = "(.*)"', (ROOT / "bindings/rust/Cargo.toml").read_text(), re.M),
        "cli.c KOLMO_VERSION_STR": re.search(r'#define KOLMO_VERSION_STR "(.*)"', (ROOT / "src/cli/cli.c").read_text()),
        "formula url": re.search(r'refs/tags/v([^"]*)\.tar\.gz', (ROOT / "packaging/homebrew/kolmo.rb").read_text()),
        "CHANGELOG top": re.search(r"^## (\S+)", (ROOT / "CHANGELOG.md").read_text(), re.M),
        "README Status": re.search(r"^## Status\n\n(\S+?),", (ROOT / "README.md").read_text(), re.M),
    }
    bad = []
    for rel in ("bindings/python/README.md", "bindings/rust/README.md"):   # the package pages: frozen per version
        for n, line in enumerate((ROOT / rel).read_text(encoding="utf-8").splitlines(), 1):
            for x in re.findall(r"\b0\.\d+\.\d+\b", line):
                if x != v and not re.search(r"since " + re.escape(x), line):
                    bad.append(f"{rel}:{n}: names version {x}, VERSION is {v}; a package page must not go stale")
    for k, m in got.items():
        if not m:
            bad.append(f"version label not found: {k}")
        elif m.group(1) != v:
            bad.append(f"version label {k} is {m.group(1)!r}, VERSION is {v!r}")
    return bad, v


def binary(path, v):
    bad = []
    out = subprocess.run([path, "-V"], capture_output=True, text=True).stdout
    if f"kolmo {v} " not in out:
        bad.append(f"{path} -V prints {out.strip()!r}, VERSION is {v!r}")
    help_ = subprocess.run([path, "-h"], capture_output=True, text=True)
    for n, line in enumerate((help_.stdout + help_.stderr).splitlines(), 1):
        for pat, why in STALE:
            if re.search(pat, line) and not ALLOW.search(line):
                bad.append(f"kolmo -h line {n}: {why}: {line.strip()[:140]}")
    return bad


def main():
    bad = stale_lines()
    lb, v = labels()
    bad += lb
    if len(sys.argv) > 1:
        bad += binary(sys.argv[1], v)
    for b in bad:
        print(b)
    print(f"release docs: {'FAIL, ' + str(len(bad)) + ' problem(s)' if bad else 'OK'} (version {v})")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
