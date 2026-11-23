#!/usr/bin/env python3
"""Fetch every benchmark input from its source URL, check sha256, and derive the measured files.

usage: fetch.py DATA_DIR [--groups corpus,holdout,weights] [--pin] [--manifest datasets.json]

- Downloads go to DATA_DIR/dl/ and are reused when their sha256 still matches.
- Derived files (DATA_DIR/corpus, DATA_DIR/holdout/<set>/{train,test}, DATA_DIR/weights) are rebuilt
  deterministically (fixed tar metadata, fixed .npy layout) and checked against derived_sha256.
- --pin writes the sha256 of sources and derived files that have none yet back into the manifest.
  Without --pin a missing pin is a warning; a mismatching pin is an error (a 'volatile' source only warns,
  and its derived, date-cut file is what is pinned).
- Writes DATA_DIR/FETCHED.json: every file with its size and sha256, for the results to cite.
NetCDF and Parquet steps need h5py/numpy/pyarrow (bench/repro/requirements.txt); the rest is stdlib."""
import argparse, gzip, hashlib, io, json, os, re, shutil, subprocess, sys, tarfile, zipfile

HERE = os.path.dirname(os.path.abspath(__file__))


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def download(url, dst):
    tmp = dst + ".part"
    subprocess.run(["curl", "-sSfL", "--retry", "3", "-o", tmp, url], check=True)
    os.replace(tmp, dst)


def det_tar(members, out):
    """members: list of (name, bytes) -> ustar with fixed metadata, sorted by name."""
    tmp = out + ".part"
    with tarfile.open(tmp, "w", format=tarfile.USTAR_FORMAT) as t:
        for name, data in sorted(members):
            ti = tarfile.TarInfo(name); ti.size = len(data); ti.mtime = 0; ti.mode = 0o644
            ti.uid = ti.gid = 0; ti.uname = ti.gname = ""
            t.addfile(ti, io.BytesIO(data))
    os.replace(tmp, out)


def zip_files(src):
    with zipfile.ZipFile(src) as z:
        return [(os.path.basename(i.filename), z.read(i)) for i in z.infolist() if not i.is_dir()]


def write(out, data):
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out + ".part", "wb") as f:
        f.write(data)
    os.replace(out + ".part", out)


def derive(op, src, out, root):
    o = os.path.join(root, out["out"])
    kind = out["op"]
    os.makedirs(os.path.dirname(o.rstrip("/")) or ".", exist_ok=True)
    if kind == "zip-to-tar":
        det_tar(zip_files(src), o); return [o]
    if kind == "zip-extract":
        os.makedirs(o, exist_ok=True); outs = []
        for name, data in zip_files(src):
            write(os.path.join(o, name), data); outs.append(os.path.join(o, name))
        return outs
    if kind == "zip-member":
        with zipfile.ZipFile(src) as z:
            write(o, z.read(out["member"]))
        return [o]
    if kind == "targz-to-tar":
        with tarfile.open(src, "r:gz") as t:
            mem = [(os.path.basename(m.name), t.extractfile(m).read()) for m in t.getmembers() if m.isfile()]
        det_tar(mem, o); return [o]
    if kind == "gunzip":
        with gzip.open(src) as g:
            write(o, g.read())
        return [o]
    if kind == "copy":
        os.makedirs(os.path.dirname(o), exist_ok=True); shutil.copyfile(src, o); return [o]
    if kind == "csv-cut-date":
        # GHCN-Daily station CSVs grow daily; keep the header and rows dated before the cut, in file order.
        with open(src, "rb") as f:
            lines = f.read().split(b"\n")
        head, keep = lines[0], []
        di = [c.strip(b'"') for c in head.split(b",")].index(b"DATE")
        cut = out["before"].encode()
        for ln in lines[1:]:
            if ln and ln.split(b",")[di].strip(b'"') < cut:
                keep.append(ln)
        write(o, b"\n".join([head] + keep) + b"\n"); return [o]
    if kind == "log-split-days":
        # Common Log Format: '[01/Jul/1995:00:00:01 -0400]'; one file per listed day, lines kept in order.
        days = {d.encode(): [] for d in out["days"]}
        with gzip.open(src) as g:
            for ln in g:
                m = re.search(rb"\[(\d\d/\w\w\w/\d{4}):", ln)
                if m and m.group(1) in days:
                    days[m.group(1)].append(ln)
        outs = []
        for d, ls in days.items():
            p = os.path.join(o, d.decode().replace("/", "-") + ".log")
            write(p, b"".join(ls)); outs.append(p)
        return outs
    if kind == "netcdf-to-npy":
        import h5py, numpy as np  # NetCDF4 is HDF5: read the variable's stored values (no unpacking, no scaling)
        with h5py.File(src, "r") as f:
            a = np.ascontiguousarray(f[out["var"]][()])
        os.makedirs(os.path.dirname(o), exist_ok=True)
        with open(o + ".part", "wb") as fh:
            np.save(fh, a, allow_pickle=False)
        os.replace(o + ".part", o); return [o]
    if kind == "parquet-plain":
        import pyarrow.parquet as pq  # OpenZL's parquet profile needs uncompressed, PLAIN-encoded Parquet
        t = pq.read_table(src)
        os.makedirs(os.path.dirname(o), exist_ok=True)
        pq.write_table(t, o + ".part", compression="NONE", use_dictionary=False, column_encoding="PLAIN",
                       write_statistics=False, data_page_version="1.0")
        os.replace(o + ".part", o); return [o]
    raise SystemExit(f"unknown op {kind}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("data"); ap.add_argument("--groups", default="corpus,holdout,weights")
    ap.add_argument("--pin", action="store_true"); ap.add_argument("--manifest", default=os.path.join(HERE, "datasets.json"))
    a = ap.parse_args()
    man = json.load(open(a.manifest)); groups = set(a.groups.split(","))
    root = os.path.abspath(a.data); dl = os.path.join(root, "dl"); os.makedirs(dl, exist_ok=True)
    dsha = man.setdefault("derived_sha256", {})
    report, problems, changed = {}, [], False
    for s in man["sources"]:
        if s["group"] not in groups:
            continue
        name = s["id"] + "-" + os.path.basename(s["url"])
        src = os.path.join(dl, name)
        want = s.get("sha256")
        stale = os.path.exists(src) and want and not s.get("volatile") and sha256(src) != want
        if not os.path.exists(src) or stale:
            print(f"fetch {s['url']}", flush=True)
            try:
                download(s["url"], src)
            except subprocess.CalledProcessError as e:
                problems.append(f"{s['id']}: download failed ({e.returncode})"); continue
        got = sha256(src)
        if want and got != want:
            if s.get("volatile"):
                print(f"note: {s['id']} changed upstream (volatile); the derived file is what is pinned")
            else:
                problems.append(f"{s['id']}: sha256 {got} != pinned {want}"); continue
        elif not want and not s.get("volatile"):
            if a.pin:
                s["sha256"] = got; changed = True
            else:
                print(f"warning: {s['id']} has no pinned sha256 (run with --pin once)")
        report[s["id"]] = dict(url=s["url"], sha256=got, bytes=os.path.getsize(src))
        for d in s["derive"]:
            for p in derive(d["op"], src, d, root):
                rel = os.path.relpath(p, root); h = sha256(p)
                if rel in dsha and dsha[rel] != h:
                    problems.append(f"{rel}: derived sha256 {h} != pinned {dsha[rel]}")
                elif rel not in dsha and a.pin:
                    dsha[rel] = h; changed = True
                report[rel] = dict(sha256=h, bytes=os.path.getsize(p), source=s["id"])
    json.dump(report, open(os.path.join(root, "FETCHED.json"), "w"), indent=1, sort_keys=True)
    if changed:
        man["derived_sha256"] = dict(sorted(dsha.items()))
        txt = json.dumps(man, indent=1) + "\n"
        open(a.manifest, "w").write(txt); print(f"pinned new hashes in {a.manifest}")
    for p in problems:
        print("PROBLEM:", p)
    sys.exit(1 if any("!=" in p for p in problems) else 0)


if __name__ == "__main__":
    main()
