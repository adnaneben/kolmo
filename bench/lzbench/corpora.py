#!/usr/bin/env python3
"""Fetch and assemble the three lzbench corpora used by Kolmo's market benchmark, checking sha256 at every step.

    python3 -I corpora.py <out-dir> [--from-dir <dir with silesia.zip, enwik8.zip, cantrbry.tgz>]

Writes <out-dir>/silesia.tar, <out-dir>/enwik8, <out-dir>/canterbury.tar and <out-dir>/silesia/<file> (the twelve
Silesia files, for the per-file run).

- silesia.tar is rebuilt byte for byte as the copy measured on the Apple M5 (sha256 SILESIA_TAR below): the twelve
  files of the canonical silesia.zip in the order and with the header fields of that copy (GNU magic, mode 644, empty
  uid/gid fields, the files' 2002 mtimes, one zero block at the end). Kolmo's ratios can then be compared across
  machines byte for byte.
- enwik8 is the canonical file from Matt Mahoney's site.
- canterbury.tar: the Mac copy was made with macOS tar and carries AppleDouble (._*) members and a user name, so it
  cannot be rebuilt elsewhere. This script writes a neutral tar instead (the eleven files of cantrbry.tgz, sorted,
  uid/gid 0, no names, mtime 0, POSIX ustar). Its sha256 is CANTERBURY_TAR below; results on it are compared with
  a Mac run on the same neutral tar, not with the original report.
"""
import hashlib, io, os, shutil, sys, tarfile, urllib.request, zipfile

URLS = {
    "silesia.zip": "https://sun.aei.polsl.pl/~sdeor/corpus/silesia.zip",
    "enwik8.zip": "https://mattmahoney.net/dc/enwik8.zip",
    "cantrbry.tgz": "https://corpus.canterbury.ac.nz/resources/cantrbry.tgz",
}

# sha256 of every file the corpora are made of (checked after extraction, so a re-packed archive still passes).
SILESIA = [  # (name, mtime of the Mac copy, sha256), in the Mac copy's order
    ("dickens", 1018614068, "b24c37886142e11d0ee687db6ab06f936207aa7f2ea1fd1d9a36763c7a507e6a"),
    ("mozilla", 1022871028, "657fc3764b0c75ac9de9623125705831ebbfbe08fed248df73bc2dc66e2a963b"),
    ("mr", 1048158746, "68637ed52e3e4860174ed2dc0840ac77d5f1a60abbcb13770d5754e3774d53e6"),
    ("nci", 1017782516, "fc63a31770947b8c2062d3b19ca94c00485a232bb91b502021948fee983e1635"),
    ("ooffice", 1025755200, "e7ee013880d34dd5208283d0d3d91b07f442e067454276095ded14f322a656eb"),
    ("osdb", 1018547778, "60f027179302ca3ad87c58ac90b6be72ec23588aaa7a3b7fe8ecc0f11def3fa3"),
    ("reymont", 1017787212, "0eac0114a3dfe6e2ee1f345a0f79d653cb26c3bc9f0ed79238af4933422b7578"),
    ("samba", 1017066842, "93ba07bc44d8267789c1d911992f40b089ffa2140b4a160fac11ccae9a40e7b2"),
    ("sao", 1016933916, "c2d0ea2cc59d4c21b7fe43a71499342a00cbe530a1d5548770e91ecd6214adcc"),
    ("webster", 1017052758, "6a68f69b26daf09f9dd84f7470368553194a0b294fcfa80f1604efb11143a383"),
    ("xml", 975628466, "0e82e54e695c1938e4193448022543845b33020c8be6bf3bf3ead2224903e08c"),
    ("x-ray", 1017925212, "7de9fce1405dc44ae5e6813ed21cd5751e761bd4265655a005d39b9685d1c9ad"),
]
SILESIA_TAR = "a667bac80089b3d04d5496772149e632f0758974b0579d743cd8d6e5e0d63eb2"
ENWIK8 = "2b49720ec4d78c3c9fabaee6e4179a5e997302b3a70029f30f2d582218c024a8"
CANTERBURY = {
    "alice29.txt": "7467306ee0feed4971260f3c87421154a05be571d944e9cb021a5713700c38f0",
    "asyoulik.txt": "eaa3526fe53859f34ecdf255712f9ecf0b2c903451d4755b2edaa2e2599cb0fc",
    "cp.html": "e0cd21cef5b6c4069461e949be100080c3ce887de6f1dd8626c480528efaaf61",
    "fields.c": "85d73e354cc50cec76cb5a50537cf8dc035f8cbb8480f9e1cbe2f7d6c23393c7",
    "grammar.lsp": "1b0805dfc0ae706b35aac2bb4e15f02485efd24dda5dbd29de7b2f84d1a88c15",
    "kennedy.xls": "9af47239ca29dfe20e633f80bbbb9a4cc9783d0803d7b2b5626f42e4c3790420",
    "lcet10.txt": "5314ba1dbb03f471df88bec6cd120a938ef60d0fd3511c5c1dce61bf7463245f",
    "plrabn12.txt": "07e2e0b461af78c7c647cb53dab39de560198e16f799b4516eccf0fbd69f764c",
    "ptt5": "0ec3a75089bb52342813496b17e51377bc9eba3cb519a444d67025354841d650",
    "sum": "ee5733cd76ecc2f9d8ff156adc3c02a7a851051dcf43a2d56ff4ee4ff606bdb3",
    "xargs.1": "c58aeb5d2d1e12751d47e7412b45784405fc30a5671b03d480fa05776e183619",
}
CANTERBURY_TAR = "4d2c5c67219e85827774abd3c95f6b89abb5c7449b778b2559d02787b44e1e0f"  # 2,826,240 B


def sha(b):
    return hashlib.sha256(b).hexdigest()


def check(name, data, want):
    got = sha(data)
    if want and got != want:
        sys.exit(f"corpora: sha256 mismatch for {name}: got {got}, want {want}")
    print(f"ok  {got}  {name} ({len(data)} B)")


def fetch(name, from_dir):
    if from_dir and os.path.exists(os.path.join(from_dir, name)):
        with open(os.path.join(from_dir, name), "rb") as f:
            return f.read()
    req = urllib.request.Request(URLS[name], headers={"User-Agent": "kolmo-bench"})
    with urllib.request.urlopen(req, timeout=300) as r:
        return r.read()


def oct_field(v, width):  # width-1 octal digits and a NUL
    return b"%0*o\0" % (width - 1, v)


def silesia_header(name, size, mtime):
    """One 512-byte header laid out exactly as in the Mac copy of silesia.tar."""
    h = bytearray(512)
    h[0:len(name)] = name.encode()
    h[100:108] = b"000644 \0"
    # uid (108) and gid (116) stay NUL
    h[124:136] = oct_field(size, 12)
    h[136:148] = oct_field(mtime, 12)
    h[156:157] = b"0"
    h[257:265] = b"ustar  \0"
    h[148:156] = b" " * 8
    h[148:156] = oct_field(sum(h), 8)
    return bytes(h)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    out = sys.argv[1]
    from_dir = sys.argv[sys.argv.index("--from-dir") + 1] if "--from-dir" in sys.argv else None
    os.makedirs(os.path.join(out, "silesia"), exist_ok=True)

    z = zipfile.ZipFile(io.BytesIO(fetch("silesia.zip", from_dir)))
    tar = bytearray()
    for name, mtime, want in SILESIA:
        data = z.read(name)
        check("silesia/" + name, data, want)
        with open(os.path.join(out, "silesia", name), "wb") as f:
            f.write(data)
        tar += silesia_header(name, len(data), mtime) + data + b"\0" * (-len(data) % 512)
    tar += b"\0" * 512
    check("silesia.tar", tar, SILESIA_TAR)
    with open(os.path.join(out, "silesia.tar"), "wb") as f:
        f.write(tar)

    data = zipfile.ZipFile(io.BytesIO(fetch("enwik8.zip", from_dir))).read("enwik8")
    check("enwik8", data, ENWIK8)
    with open(os.path.join(out, "enwik8"), "wb") as f:
        f.write(data)

    src = tarfile.open(fileobj=io.BytesIO(fetch("cantrbry.tgz", from_dir)), mode="r:gz")
    files = {os.path.basename(m.name): src.extractfile(m).read() for m in src.getmembers() if m.isfile()}
    if sorted(files) != sorted(CANTERBURY):
        sys.exit(f"corpora: cantrbry.tgz holds {sorted(files)}, want {sorted(CANTERBURY)}")
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w", format=tarfile.USTAR_FORMAT) as t:
        for name in sorted(files):
            check("canterbury/" + name, files[name], CANTERBURY[name])
            ti = tarfile.TarInfo("canterbury/" + name)
            ti.size, ti.mtime, ti.mode, ti.uid, ti.gid, ti.uname, ti.gname = len(files[name]), 0, 0o644, 0, 0, "", ""
            t.addfile(ti, io.BytesIO(files[name]))
    check("canterbury.tar (neutral)", buf.getvalue(), CANTERBURY_TAR)
    with open(os.path.join(out, "canterbury.tar"), "wb") as f:
        f.write(buf.getvalue())


if __name__ == "__main__":
    main()
