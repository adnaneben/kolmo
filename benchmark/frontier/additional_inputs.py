"""Validate and fingerprint the additional public file-format targets."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
from panel import sha, write_json

ARTIFACTS = {
    "events": ("gharchive.gz","https://data.gharchive.org/2016-01-01-0.json.gz"),
    "reads": ("ecoli-reads.gz","https://ftp.sra.ebi.ac.uk/vol1/fastq/SRR258/003/SRR2584863/SRR2584863_1.fastq.gz"),
    "taxi": ("taxi.parquet","https://d37ci6vzurychx.cloudfront.net/trip-data/yellow_tripdata_2022-01.parquet"),
    "quic": ("rfc9000.pdf","https://www.rfc-editor.org/rfc/rfc9000.pdf"),
}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("directory",type=Path)
    p.add_argument("manifest",type=Path)
    args=p.parse_args()
    results={}
    for name,(filename,url) in ARTIFACTS.items():
        src=args.directory/filename
        dst=src
        info={}
        if name=="events":
            dst=src.with_name("events.jsonl")
            with gzip.open(src,"rb") as f,dst.open("wb") as out:
                count=0
                for line in f:
                    assert isinstance(json.loads(line),dict)
                    out.write(line); count+=1
            info["records"]=count
        if name=="reads":
            # ENA filereport, SRR2584863 forward FASTQ archive.
            assert src.stat().st_size==183292913
            check=hashlib.md5()
            with src.open("rb") as archive:
                for block in iter(lambda: archive.read(1<<20),b""):
                    check.update(block)
            assert check.hexdigest()=="4a1529a4165970302d6d8cb50b6e7a43"
            dst=src.with_name("reads.fastq")
            with gzip.open(src,"rb") as f,dst.open("wb") as out:
                for _ in range(100000):
                    record=[f.readline() for j in range(4)]
                    assert record[0].startswith(b"@") and record[2].startswith(b"+")
                    assert record[3].endswith(b"\n") and len(record[1].rstrip())==len(record[3].rstrip())
                    out.writelines(record)
            info["records"]=100000
        if name=="taxi":
            with src.open("rb") as f:
                assert f.read(4)==b"PAR1"
                f.seek(-4,2)
                assert f.read(4)==b"PAR1"
        if name=="quic":
            with src.open("rb") as f:
                assert f.read(5)==b"%PDF-"
                f.seek(-128,2)
                assert b"%%EOF" in f.read()
        results[name]=dict(url=url,artifact_sha256=sha(src),artifact_bytes=src.stat().st_size,
                           target_sha256=sha(dst),target_bytes=dst.stat().st_size,**info)
    write_json(args.manifest,results)

if __name__=="__main__":
    main()
