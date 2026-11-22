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
  3. (0.1.0) a release-facing file or `kolmo -h` shows an option removed in 0.1.0 as current usage (-m MODE, -x, -s,
     -S, -r, -l N, -B MiB, -D, -P, kolmo train/profile, kolmo a/x/l, the two-positional `kolmo IN OUT`, the bindings'
     mode=/Mode::). Lines that describe the removal (ALLOW) and the sections on upgrading from 0.0.x are accepted.
  4. the level table of docs/LEVELS.md (levels, names, letters, long options) and the one `kolmo -h` prints disagree.

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
    (r"^keywords\s*=.*\b(zstd|brotli|lz4|lzma|xz)\b", "a library name as a package keyword"),
]
REMOVED = [
    (r"(?<![\w-])-m\s*(speed|balanced|ratio|rt|fast|eco|hc|max|ultra)\b", "-m MODE (removed in 0.1.0; use a level)"),
    (r"(?<![\w-])-x\b(?!-)", "-x (removed in 0.1.0; use -U)"),
    (r"(?<![\w-])-[sS](\s+\d+)?\s+[\w./]+\.\w+", "-s/-S (removed in 0.1.0; use --stream)"),
    (r"(?<![\w-])-r\s*\d+:\d+", "-r OFF:LEN (removed in 0.1.0; use --range)"),
    (r"(?<![\w-])-l\s*\d", "-l LEVEL (removed in 0.1.0; use --codec-level)"),
    (r"(?<![\w-])-B\s*\d", "-B MiB (removed in 0.1.0; use --block-size)"),
    (r"(?<![\w-])-[DP]\s+\S", "-D/-P (removed)"),
    (r"\bkolmo\s+(train|profile)\b", "kolmo train/profile (removed)"),
    (r"\bkolmo\s+(a|x|l)\s", "kolmo a/x/l (renamed archive/extract/contents in 0.1.0)"),
    (r"\bkolmo(\s+-[\w-]+)*\s+(?!-)[\w./]+\.\w+\s+(?!-)[\w./]+\.\w+(\s|`|$)", "the two-positional form kolmo IN OUT (removed in 0.1.0; use -o)"),
    (r"\bmode\s*=\s*[\"']|Mode::", "the bindings' mode (replaced by level in 0.1.0)"),
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


def removed_usage():
    """Removed 0.1.0 options shown as current usage in a user-facing document (sections about upgrading are skipped)."""
    bad = []
    for rel in ["README.md", "bindings/python/README.md", "bindings/rust/README.md", "RELEASING.md", "packaging/homebrew/kolmo.rb"]:
        skip = False
        for n, line in enumerate((ROOT / rel).read_text(encoding="utf-8").splitlines(), 1):
            if line.startswith("#") and not rel.endswith(".rb"):
                skip = bool(re.search(r"upgrad|0\.0\.x|removed|mapping|history", line, re.I))
            if skip:
                continue
            for pat, why in REMOVED:
                if re.search(pat, line) and not ALLOW.search(line):
                    bad.append(f"{rel}:{n}: {why}: {line.strip()[:140]}")
    return bad


def level_table(help_text):
    """docs/LEVELS.md section 1 against kolmo -h: every level with its name, letter and long option."""
    bad, rows = [], []
    for line in (ROOT / "docs/LEVELS.md").read_text(encoding="utf-8").splitlines():
        m = re.match(r"^\|\s*(\d|—)\s*\|\s*(\w*)\s*\|\s*`?(-\w)?`?\s*\|\s*`?(--\w+)?`?\s*\|", line)
        if m:
            rows.append(m.groups())
    if len(rows) != 10:
        return [f"docs/LEVELS.md: level table has {len(rows)} rows, expected 10 (1-9 and ultra)"]
    lines = help_text.splitlines()
    for num, name, letter, long_ in rows:
        if num == "—":
            pat = rf"^\s*{re.escape(letter)}\s+{re.escape(long_)}\b"
        elif name:
            pat = rf"^\s*-{num}\s+{re.escape(letter)}\s+{re.escape(long_)}\b"
        else:
            pat = rf"^\s*-{num}\s*$"
        if not any(re.search(pat, l) for l in lines):
            bad.append(f"kolmo -h: no line for level {num} {name} {letter or ''} {long_ or ''} as docs/LEVELS.md has it")
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
    if help_.returncode != 0:
        bad.append(f"{path} -h exits with {help_.returncode}")
    text = help_.stdout + help_.stderr
    for n, line in enumerate(text.splitlines(), 1):
        for pat, why in STALE + REMOVED:
            if re.search(pat, line) and not ALLOW.search(line):
                bad.append(f"kolmo -h line {n}: {why}: {line.strip()[:140]}")
    bad += level_table(text)
    return bad


def main():
    bad = stale_lines() + removed_usage()
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
