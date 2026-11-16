#!/usr/bin/env python3
"""Fetch and cut the dataset families used by the features-a lane (kolmo train, --target).

Run with `python3 -I prepare.py DATA_DIR`. Every download lands in DATA_DIR/raw and is treated as untrusted data:
archives are read member by member with path checks, nothing from them is executed. The cut sets are written to
DATA_DIR/sets/<family>/{train,test}/ and their sizes and SHA-256 to DATA_DIR/sets/manifest.json.

Families (train on the first part, test on later, unseen parts of the same source):
  csv-quakes     USGS earthquake catalogue as CSV, one file per month (train: Jan 2020, test: Feb-Jun 2020)
  json-quakes    the same months as GeoJSON (train: Jan, test: Feb-Mar)
  log-hadoop     Loghub Hadoop: one file per application container log (train: every 5th file)
  log-openstack  Loghub OpenStack: 256 KiB chunks of the logs (train: the first 10%)
  log-ssh        Loghub SSH (OpenSSH): 1 MiB chunks, as rotated logs (train: the first 4 chunks)
  num-power      UCI household power, ';' table, one file per month (train: Dec 2006 + Jan 2007, test: 12 months)
  num-power-f32  the same months as little-endian float32 NPY arrays, 7 columns (missing values as NaN)
  msg-geojson    single GeoJSON features (one earthquake each, ~1 KB)
  msg-ssh        single SSH log lines (~100 B)
  msg-openstack  single OpenStack log lines (~300 B)
"""
import hashlib, io, json, os, struct, sys, tarfile, urllib.request, zipfile

DATA = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "data")
RAW = os.path.join(DATA, "raw"); SETS = os.path.join(DATA, "sets")
USGS = "https://earthquake.usgs.gov/fdsnws/event/1/query?format={fmt}&starttime={a}&endtime={b}&orderby=time-asc&limit=20000"
MONTHS = [("2020-01-01", "2020-02-01"), ("2020-02-01", "2020-03-01"), ("2020-03-01", "2020-04-01"),
          ("2020-04-01", "2020-05-01"), ("2020-05-01", "2020-06-01"), ("2020-06-01", "2020-07-01")]
ZEN = "https://zenodo.org/records/8196385/files/{}?download=1"
POWER = "https://archive.ics.uci.edu/static/public/235/individual+household+electric+power+consumption.zip"


def fetch(url, name):
    os.makedirs(RAW, exist_ok=True)
    p = os.path.join(RAW, name)
    if not os.path.exists(p):
        req = urllib.request.Request(url, headers={"User-Agent": "kolmo-benchmark"})
        with urllib.request.urlopen(req, timeout=600) as r, open(p + ".part", "wb") as f:
            while True:
                b = r.read(1 << 20)
                if not b: break
                f.write(b)
        os.replace(p + ".part", p)
    return p


def put(family, part, name, data):
    d = os.path.join(SETS, family, part); os.makedirs(d, exist_ok=True)
    assert "/" not in name and not name.startswith(".")
    with open(os.path.join(d, name), "wb") as f: f.write(data)


def members_tar(path):
    with tarfile.open(path, "r:gz") as t:
        for m in t:
            if m.isfile() and ".." not in m.name.split("/"):
                yield m.name, t.extractfile(m).read()


def members_zip(path):
    with zipfile.ZipFile(path) as z:
        for i in z.infolist():
            if not i.is_dir() and ".." not in i.filename.split("/"):
                yield i.filename, z.read(i)


def chunks(data, size):
    out, i = [], 0
    while i < len(data):
        j = min(len(data), i + size)
        k = data.rfind(b"\n", i, j) + 1 if j < len(data) else j
        if k <= i: k = j
        out.append(data[i:k]); i = k
    return out


def quakes():
    for k, (a, b) in enumerate(MONTHS):
        csv = open(fetch(USGS.format(fmt="csv", a=a, b=b), f"quakes-{a[:7]}.csv"), "rb").read()
        put("csv-quakes", "train" if k == 0 else "test", f"{a[:7]}.csv", csv)
        if k < 3:
            gj = open(fetch(USGS.format(fmt="geojson", a=a, b=b), f"quakes-{a[:7]}.geojson"), "rb").read()
            put("json-quakes", "train" if k == 0 else "test", f"{a[:7]}.json", gj)
            feats = json.loads(gj)["features"]
            part = "train" if k == 0 else "test"
            n = 2000 if k == 0 else 3000
            for i, f in enumerate(feats[:n]):
                put("msg-geojson", part, f"{a[:7]}-{i:05d}.json", json.dumps(f, separators=(",", ":")).encode())


def logs():
    hz = fetch(ZEN.format("Hadoop.zip"), "Hadoop.zip")
    files = sorted((n, d) for n, d in members_zip(hz) if n.endswith(".log") and len(d) > 0)
    for i, (n, d) in enumerate(files):
        put("log-hadoop", "train" if i % 5 == 0 else "test", n.replace("/", "_"), d)
    os_ = fetch(ZEN.format("OpenStack.tar.gz"), "OpenStack.tar.gz")
    data = b"".join(d for n, d in sorted(members_tar(os_)) if n.endswith(".log"))
    cs = chunks(data, 256 << 10); ntr = max(1, len(cs) // 10)
    for i, c in enumerate(cs): put("log-openstack", "train" if i < ntr else "test", f"chunk{i:04d}.log", c)
    lines = data.split(b"\n")
    for i, l in enumerate(lines[:2000]): put("msg-openstack", "train", f"{i:05d}.log", l + b"\n")
    for i, l in enumerate(lines[len(lines) // 2:len(lines) // 2 + 3000]): put("msg-openstack", "test", f"{i:05d}.log", l + b"\n")
    ssh = fetch(ZEN.format("SSH.tar.gz"), "SSH.tar.gz")
    data = b"".join(d for n, d in sorted(members_tar(ssh)) if n.endswith(".log"))
    cs = chunks(data, 1 << 20)
    for i, c in enumerate(cs[:4]): put("log-ssh", "train", f"chunk{i:03d}.log", c)
    for i, c in enumerate(cs[4:24]): put("log-ssh", "test", f"chunk{i + 4:03d}.log", c)
    lines = data.split(b"\n")
    for i, l in enumerate(lines[:2000]): put("msg-ssh", "train", f"{i:05d}.log", l + b"\n")
    for i, l in enumerate(lines[len(lines) // 2:len(lines) // 2 + 3000]): put("msg-ssh", "test", f"{i:05d}.log", l + b"\n")


def power():
    z = fetch(POWER, "power.zip")
    txt = next(d for n, d in members_zip(z) if n.endswith(".txt"))
    lines = txt.split(b"\n"); head = lines[0] + b"\n"
    months = {}
    for l in lines[1:]:
        if not l: continue
        d = l.split(b";", 1)[0].split(b"/")          # d/m/yyyy
        months.setdefault((int(d[2]), int(d[1])), []).append(l + b"\n")
    keys = sorted(months)[:14]
    for k, key in enumerate(keys):
        part = "train" if k < 2 else "test"
        body = b"".join(months[key])
        put("num-power", part, f"{key[0]}-{key[1]:02d}.txt", head + body)
        rows = []
        for l in months[key]:
            f = l.rstrip(b"\r\n").split(b";")[2:]
            rows.append([float(x) if x not in (b"?", b"") else float("nan") for x in f])
        arr = b"".join(struct.pack("<7f", *r) for r in rows)
        hdr = "{'descr': '<f4', 'fortran_order': False, 'shape': (%d, 7), }" % len(rows)
        hdr = hdr + " " * (63 - (len(hdr) + 10) % 64) + "\n"
        npy = b"\x93NUMPY\x01\x00" + struct.pack("<H", len(hdr)) + hdr.encode() + arr
        put("num-power-f32", part, f"{key[0]}-{key[1]:02d}.npy", npy)


if __name__ == "__main__":
    quakes(); logs(); power()
    man = {}
    for fam in sorted(os.listdir(SETS)):
        if not os.path.isdir(os.path.join(SETS, fam)): continue
        for part in ("train", "test"):
            d = os.path.join(SETS, fam, part); h = hashlib.sha256(); n = b = 0
            for f in sorted(os.listdir(d)):
                x = open(os.path.join(d, f), "rb").read(); h.update(f.encode() + b"\0" + x); n += 1; b += len(x)
            man[f"{fam}/{part}"] = {"files": n, "bytes": b, "sha256": h.hexdigest()}
    json.dump(man, open(os.path.join(SETS, "manifest.json"), "w"), indent=1)
    print(json.dumps(man, indent=1))
