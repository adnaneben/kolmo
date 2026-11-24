#!/usr/bin/env python3
"""Add kolmo to an lzbench checkout as an ordinary codec, in the layout lzbench's CONTRIBUTING.md asks for.

    python3 -I bench/lzbench/install.py <lzbench-dir> [--ref v0.0.2] [--kolmo <kolmo checkout>]

Then build lzbench as usual, e.g. `make -C <lzbench-dir> -j BUILD_ONLY="KOLMO ZSTD XZ"`.

What it changes in the lzbench tree (the same set of changes the upstream pull request would make):
  misc/kolmo/src/         kolmo's src/ at --ref, unchanged (`git archive`), with LICENSE, NOTICE and a README
  mk/kolmo.mk             objects and flags (bench/lzbench/kolmo.mk here)
  bench/codecs.h          the two declarations, in alphabetical order of BENCH_REMOVE_ macros
  bench/misc_codecs.cpp   the wrapper (bench/lzbench/kolmo_codec.cpp here), in alphabetical order
  bench/lzbench.h         comp_desc entry, per-level algorithms, aliases LZ+ENTROPY, SYMMETRIC, SLOW, FASTEST
  README.md, CHANGELOG    the codec table row and the change log line
It is idempotent: running it again replaces misc/kolmo and leaves the registrations as they are.
--ref defaults to the tag of the VERSION file (v<VERSION>); --ref WORKTREE copies the working tree's src/ instead.
"""
import argparse, io, os, re, shutil, subprocess, sys, tarfile

HERE = os.path.dirname(os.path.abspath(__file__))


def sh(args, cwd):
    return subprocess.run(args, cwd=cwd, check=True, capture_output=True).stdout


def edit(path, fn):
    with open(path) as f:
        s = f.read()
    t = fn(s)
    if t != s:
        with open(path, "w") as f:
            f.write(t)


def insert_sorted_block(s, block, start_re, key, marker):
    """Insert block before the first `#ifndef BENCH_REMOVE_X` (matched by start_re) whose X sorts after key."""
    if marker in s:
        return s
    for m in re.finditer(start_re, s, re.M):
        if m.group(1) > key:
            return s[: m.start()] + block + s[m.start():]
    sys.exit(f"install: no place for {key} found")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("lzbench")
    ap.add_argument("--kolmo", default=os.path.normpath(os.path.join(HERE, "..", "..")))
    ap.add_argument("--ref")
    a = ap.parse_args()
    lzb, kolmo = os.path.abspath(a.lzbench), os.path.abspath(a.kolmo)
    for p in ("bench/codecs.h", "bench/lzbench.h", "bench/misc_codecs.cpp", "mk"):
        if not os.path.exists(os.path.join(lzb, p)):
            sys.exit(f"install: {lzb} is not an lzbench checkout with mk/*.mk files (lzbench 2.4.1 or later)")

    ref = a.ref
    if ref is None:
        with open(os.path.join(kolmo, "VERSION")) as f:
            ref = "v" + f.read().strip()
    # 1. Sources: kolmo's src/ unchanged, plus its licence files.
    dst = os.path.join(lzb, "misc", "kolmo")
    shutil.rmtree(dst, ignore_errors=True)
    os.makedirs(dst)
    if ref == "WORKTREE":
        shutil.copytree(os.path.join(kolmo, "src"), os.path.join(dst, "src"))
        for n in ("LICENSE", "NOTICE", "VERSION"):
            shutil.copy(os.path.join(kolmo, n), dst)
        commit, date = sh(["git", "rev-parse", "HEAD"], kolmo).decode().strip() + " plus local changes", "-"
    else:
        tar = sh(["git", "archive", "--format=tar", ref, "src", "LICENSE", "NOTICE", "VERSION"], kolmo)
        tarfile.open(fileobj=io.BytesIO(tar)).extractall(dst, filter="data") if sys.version_info >= (3, 12) \
            else tarfile.open(fileobj=io.BytesIO(tar)).extractall(dst)
        commit = sh(["git", "rev-parse", ref + "^{commit}"], kolmo).decode().strip()
        date = sh(["git", "log", "-1", "--format=%cs", ref], kolmo).decode().strip()
    with open(os.path.join(dst, "VERSION")) as f:
        version = f.read().strip()
    os.remove(os.path.join(dst, "VERSION"))
    with open(os.path.join(dst, "README.md"), "w") as f:
        f.write(f"""# kolmo {version}

Structure-aware lossless compressor: https://github.com/adnaneben/kolmo (Apache License 2.0, `LICENSE`; `NOTICE`).

`src/` is kolmo's `src/` directory at `{ref}` (commit {commit}, {date}), unchanged. lzbench compiles the
library objects listed in `mk/kolmo.mk`; `src/cli` and `src/experimental` are not built. `src/algo/sais` is a subset
of libsais (Apache License 2.0 with LLVM Exceptions, its `LICENSE` there) whose symbols kolmo renamed to `kolmo_sais_*`,
so it does not collide with the libsais copy inside lzbench's bsc.
""")

    # 2. Build fragment.
    shutil.copy(os.path.join(HERE, "kolmo.mk"), os.path.join(lzb, "mk", "kolmo.mk"))

    # 3. Declarations.
    decl = """#ifndef BENCH_REMOVE_KOLMO
int64_t lzbench_kolmo_compress(char *inbuf, size_t insize, char *outbuf, size_t outsize, codec_options_t *codec_options);
int64_t lzbench_kolmo_decompress(char *inbuf, size_t insize, char *outbuf, size_t outsize, codec_options_t *codec_options);
#else
#define lzbench_kolmo_compress NULL
#define lzbench_kolmo_decompress NULL
#endif // BENCH_REMOVE_KOLMO

"""
    edit(os.path.join(lzb, "bench/codecs.h"),
         lambda s: insert_sorted_block(s, decl, r"^#ifndef BENCH_REMOVE_([A-Z0-9_]+)\s*$", "KOLMO", "lzbench_kolmo_compress"))

    # 4. Wrapper.
    with open(os.path.join(HERE, "kolmo_codec.cpp")) as f:
        wrapper = f.read() + "\n\n"
    edit(os.path.join(lzb, "bench/misc_codecs.cpp"),
         lambda s: insert_sorted_block(s, wrapper, r"^#ifndef BENCH_REMOVE_([A-Z0-9_]+)\s*$", "KOLMO", "lzbench_kolmo_compress"))

    # 5. Registration in bench/lzbench.h.
    def reg(s):
        if '"kolmo"' in s:
            return s
        entry = ('    { "kolmo",      "kolmo %s",             "LZ77 + Huffman/ANS, BWT or CM", 1,  10,    0,   NO_THREADING, '
                 'lzbench_kolmo_compress,      lzbench_kolmo_decompress,      NULL,                    NULL },\n' % version)
        i = s.index("\n", s.index('{ "kanzi",      "kanzi')) + 1
        s = s[:i] + entry + s[i:]
        levels = ('    { "kolmo",    1,  1, "LZ77" },                  // KLZ\n'
                  '    { "kolmo",    2,  7, "LZ77 + Huffman/ANS" },    // KLZH2\n'
                  '    { "kolmo",    8,  9, "BWT or LZMA-class, per block" },\n'
                  '    { "kolmo",   10, 10, "CM" },                    // ultra\n')
        last = [m.end() for m in re.finditer(r'^    \{ "kanzi",\s+\d+,\s+\d+,.*\n', s, re.M)][-1]
        s = s[:last] + levels + s[last:]
        for old, new in (('kanzi,2,3,4/', 'kanzi,2,3,4/kolmo,1,3,5,7/'),       # LZ+ENTROPY
                         ('kanzi,5,6,7,8,9/', 'kanzi,5,6,7,8,9/kolmo,8,9/'),    # SYMMETRIC
                         ('"memcpy/glza"', '"memcpy/glza/kolmo,10"'),           # SLOW
                         ('gobrrr,0/libdeflate,1/', 'gobrrr,0/kolmo,1/libdeflate,1/')):  # FASTEST
            if s.count(old) != 1:
                sys.exit(f"install: bench/lzbench.h: expected one '{old}'")
            s = s.replace(old, new)
        return s
    edit(os.path.join(lzb, "bench/lzbench.h"), reg)

    # 6. README table and CHANGELOG.
    def readme(s):
        if "[kolmo " in s:
            return s
        i = s.index("\n", s.index("| [kanzi ")) + 1
        return s[:i] + (f"| [kolmo {version}](https://github.com/adnaneben/kolmo) | {date} | 64-bit little-endian only; "
                        "not built on Windows |\n") + s[i:]
    edit(os.path.join(lzb, "README.md"), readme)

    def changelog(s):
        if "added kolmo" in s:
            return s
        line = f"- added kolmo {version} (structure-aware: LZ, BWT or CM per block; levels 1-9 and 10 = ultra)\n"
        head = s.index("\n") + 1
        for m in re.finditer(r"^- added (\S+)", s[head:], re.M):
            if m.group(1).lower() > "kolmo":
                return s[: head + m.start()] + line + s[head + m.start():]
        sys.exit("install: CHANGELOG: no place for the kolmo line")
    edit(os.path.join(lzb, "CHANGELOG"), changelog)
    print(f"kolmo {version} ({ref}) installed in {lzb}")


if __name__ == "__main__":
    main()
