#!/usr/bin/env python3
"""R11: derive small-file sets (one 'file type' each) from the campaign corpora. No downloads.
python3 -I prepare_sets.py  -> /home/user/kolmo-review40-data/R11/sets/<set>/NNNN_<name>, manifest.json (sizes, SHA-256)
Order inside a set is the arrival order used by the learning-curve experiments (tar order / file order)."""
import hashlib, io, json, os, re, sys, tarfile
D = "/home/user/kolmo-review40-data"; OUT = D + "/R11/sets"
man = {}
def put(setn, i, name, data):
    d = os.path.join(OUT, setn); os.makedirs(d, exist_ok=True)
    name = re.sub(r"[^A-Za-z0-9._-]", "_", name)[-60:]
    with open(os.path.join(d, "%04d_%s" % (i, name)), "wb") as f: f.write(data)
    man.setdefault(setn, []).append(("%04d_%s" % (i, name), len(data), hashlib.sha256(data).hexdigest()))
# samba sources (Silesia 'samba' is a tar of samba-2.2.3a): .c and .html members, tar order, 1 KiB..256 KiB
t = tarfile.open(D + "/silesia/samba")
ic = ih = 0
for m in t:
    if not m.isfile() or ".." in m.name.split("/"): continue
    if not (1024 <= m.size <= 262144): continue
    b = t.extractfile(m).read()
    if m.name.endswith(".c"): put("samba-c", ic, m.name.replace("/", "_"), b); ic += 1
    elif m.name.endswith(".html"): put("samba-html", ih, m.name.replace("/", "_"), b); ih += 1
# OWID CO2 table: one CSV per country (header + its rows), file order
rows = open(D + "/R02/owid/owid-co2-data.csv", "rb").read().split(b"\n")
hdr, cur, buf, i = rows[0], None, [], 0
for r in rows[1:]:
    if not r: continue
    c = r.split(b",", 1)[0]
    if c != cur and buf:
        put("owid-country", i, cur.decode("latin1"), b"\n".join([hdr] + buf) + b"\n"); i += 1; buf = []
    cur = c; buf.append(r)
if buf: put("owid-country", i, cur.decode("latin1"), b"\n".join([hdr] + buf) + b"\n")
# Loghub OpenStack 2k lines: chunks of about 4 KiB cut at line ends (rotated-log / message-batch stand-in)
def chunks(data, size):
    out, i = [], 0
    while i < len(data):
        j = min(len(data), i + size); k = data.rfind(b"\n", i, j) + 1 if j < len(data) else j
        if k <= i: k = j
        out.append(data[i:k]); i = k
    return out
for setn, src, sz in (("openstack-4k", "OpenStack_2k.log", 4096), ("ssh-4k", "OpenSSH_2k.log", 4096), ("hadoop-4k", "Hadoop_2k.log", 4096)):
    for i, c in enumerate(chunks(open(D + "/R07-logs/" + src, "rb").read(), sz)): put(setn, i, src + ".part", c)
# enwik8 articles: <page>...</page> of 2 KiB..64 KiB, the first 600 in file order
e = open(D + "/enwik8", "rb").read(40 << 20); i = 0
for mm in re.finditer(rb"  <page>.*?</page>\n", e, re.S):
    b = mm.group(0)
    if 2048 <= len(b) <= 65536:
        put("enwik-article", i, "page.xml", b); i += 1
        if i == 600: break
json.dump(man, open(D + "/R11/sets/manifest.json", "w"), indent=0)
for s, v in man.items():
    sz = sorted(x[1] for x in v); print(s, len(v), "files", sum(sz), "bytes", "median", sz[len(sz)//2])
