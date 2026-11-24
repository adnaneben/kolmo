#!/usr/bin/env python3
"""Every file under tests/v12 is pinned: frozen frames, their originals and manifests are never rewritten.

usage: python3 tests/check_v12_pinned.py [--write] [--base OLD_PIN_FILE]

tests/v12.sha256 lists the SHA-256 of every file under tests/v12 (sha256sum format, paths relative to tests/v12).
Fails when a listed file is missing or its hash differs, or when a file is not listed. --write adds the files that
are not yet listed (a new release directory) and never changes an existing line; it refuses when a listed file
changed. --base OLD (CI passes the base branch's tests/v12.sha256): fails unless every line of OLD is still present
unchanged, so a PR cannot rewrite a frozen frame and its pin together. Complements tests/check_v12_frames.py and
test-v12-corpus, which decode the frames; this check also covers the 0.0.1 frames at the top of tests/v12, their
MANIFEST and the r*/in/ originals the size ledger (benchmark/ledger) compresses.
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
V12 = os.path.join(HERE, "v12")
PINS = os.path.join(HERE, "v12.sha256")


def read(path):
    pins = {}
    for line in open(path):
        if line.strip():
            h, name = line.rstrip("\n").split("  ", 1)
            pins[name] = h
    return pins


def tree():
    out = {}
    for d, _, files in os.walk(V12):
        for f in files:
            if f == ".DS_Store":
                continue
            p = os.path.join(d, f)
            out[os.path.relpath(p, V12).replace(os.sep, "/")] = hashlib.sha256(open(p, "rb").read()).hexdigest()
    return out


def main():
    args = sys.argv[1:]
    pins = read(PINS) if os.path.exists(PINS) else {}
    now = tree()
    bad = []
    for name, h in sorted(pins.items()):
        if name not in now:
            bad.append("%s: pinned but missing (frozen files are never removed)" % name)
        elif now[name] != h:
            bad.append("%s: changed (frozen files are never rewritten)" % name)
    new = sorted(set(now) - set(pins))
    if "--write" in args:
        if bad:
            print("\n".join(bad))
            sys.exit("v12 pins: refusing to write while pinned files changed")
        pins.update({n: now[n] for n in new})
        with open(PINS, "w") as f:
            for name in sorted(pins):
                f.write("%s  %s\n" % (pins[name], name))
        print("v12 pins: %d files pinned (%d added)" % (len(pins), len(new)))
        return
    bad += ["%s: not pinned (add it with tests/check_v12_pinned.py --write)" % n for n in new]
    if "--base" in args:
        base = read(args[args.index("--base") + 1])
        for name, h in sorted(base.items()):
            if pins.get(name) != h:
                bad.append("%s: its pin in tests/v12.sha256 was changed or removed (pins are append-only)" % name)
    for b in bad:
        print(b)
    if bad:
        sys.exit("V12 PINS FAILED (%d problems)" % len(bad))
    print("V12 PINS PASSED (%d files under tests/v12 unchanged)" % len(pins))


if __name__ == "__main__":
    main()
