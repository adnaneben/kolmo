"""Fetch the first N complete reads of public FASTQ runs from ENA (a prefix of the gzip file is enough).

usage: fetch_fastq.py OUT_DIR N RUN:ENA_DIR [RUN:ENA_DIR ...]
e.g.   fetch_fastq.py data 100000 SRR1770413:SRR177/003/SRR1770413
Writes OUT_DIR/RUN.fastq with exactly 4*N lines (forward reads, file _1 of the pair).
"""
import os
import sys
import urllib.request
import zlib

out, n = sys.argv[1], int(sys.argv[2])
os.makedirs(out, exist_ok=True)
for spec in sys.argv[3:]:
    run, d = spec.split(":")
    url = f"https://ftp.sra.ebi.ac.uk/vol1/fastq/{d}/{run}_1.fastq.gz"
    req = urllib.request.Request(url, headers={"Range": "bytes=0-80000000", "User-Agent": "kolmo-benchmark"})
    raw = urllib.request.urlopen(req, timeout=300).read()
    data = zlib.decompressobj(16 + zlib.MAX_WBITS).decompress(raw)   # the truncated tail is simply not decoded
    lines = data.split(b"\n")
    if len(lines) < 4 * n + 1:
        sys.exit(f"{run}: only {len(lines) // 4} reads in the fetched prefix")
    body = b"\n".join(lines[:4 * n]) + b"\n"
    assert all(lines[4 * i].startswith(b"@") and lines[4 * i + 2].startswith(b"+") for i in range(n))
    path = os.path.join(out, run + ".fastq")
    open(path, "wb").write(body)
    print(f"{run}: {n} reads, {len(body):,} bytes, from {url} ({len(raw):,} compressed bytes fetched)")
