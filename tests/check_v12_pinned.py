#!/usr/bin/env python3
"""Every file under tests/v12 is pinned: frozen frames, their originals and manifests are never rewritten.

usage: python3 tests/check_v12_pinned.py [--write] [--base OLD_PIN_FILE]

tests/v12.sha256 lists the SHA-256 of every file under tests/v12 (sha256sum format, paths relative to tests/v12).
Fails when a listed file is missing or its hash differs, or when a file is not listed. --write adds the files that
are not yet listed (a new release directory) and never changes an existing line; it refuses when a listed file
changed.

tests/v12/MANIFEST is the one file that grows: tests/make_v12_frames.py appends each release's "frame original" lines
to it. It is pinned as append-only: the line "MANIFEST" and each later line "MANIFEST#<lines>" hold the SHA-256 of the
file as it was after a release, and every one of them must be the hash of the first lines of today's file, the newest
of the whole file. So lines can be added at the end and no earlier line can change. --write adds the "MANIFEST#<lines>"
pin when the file has grown. (Other files named MANIFEST#... cannot exist under tests/v12: the name is reserved.) --base OLD (CI passes the base branch's tests/v12.sha256): fails unless every line of OLD is still present
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


MANIFEST = "MANIFEST"


def manifest_pin(n):
    return MANIFEST if n is None else "%s#%d" % (MANIFEST, n)


def is_manifest_pin(name):
    return name == MANIFEST or name.startswith(MANIFEST + "#")


def manifest_prefixes():
    """SHA-256 of every whole-line prefix of tests/v12/MANIFEST -> its number of lines"""
    out, h, n = {}, hashlib.sha256(), 0
    for line in open(os.path.join(V12, MANIFEST), "rb"):
        h.update(line); n += 1
        out[h.hexdigest()] = n
    return out, n


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
    whole = now.pop(MANIFEST, None)                    # append-only, see the module comment
    prefixes, lines = manifest_prefixes() if whole is not None else ({}, 0)
    bad = []
    for name, h in sorted(pins.items()):
        if is_manifest_pin(name):
            if whole is None:
                bad.append("%s: pinned but missing (frozen files are never removed)" % name)
            elif h not in prefixes:
                bad.append("%s: MANIFEST no longer starts with the lines pinned here (lines are only added at its end)" % name)
        elif name not in now:
            bad.append("%s: pinned but missing (frozen files are never removed)" % name)
        elif now[name] != h:
            bad.append("%s: changed (frozen files are never rewritten)" % name)
    new = sorted(set(now) - set(pins))
    grown = whole is not None and whole not in [h for n, h in pins.items() if is_manifest_pin(n)]
    if "--write" in args:
        if bad:
            print("\n".join(bad))
            sys.exit("v12 pins: refusing to write while pinned files changed")
        pins.update({n: now[n] for n in new})
        if grown:
            pins[manifest_pin(lines if MANIFEST in pins else None)] = whole; new.append(MANIFEST)
        with open(PINS, "w") as f:
            for name in sorted(pins):
                f.write("%s  %s\n" % (pins[name], name))
        print("v12 pins: %d files pinned (%d added)" % (len(pins), len(new)))
        return
    bad += ["%s: not pinned (add it with tests/check_v12_pinned.py --write)" % n for n in new]
    if grown:
        bad.append("MANIFEST: its %d lines are not pinned (add the pin with tests/check_v12_pinned.py --write)" % lines)
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
