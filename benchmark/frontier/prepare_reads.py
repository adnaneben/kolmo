"""Extract the registered number of complete records from bounded ENA HTTP ranges."""
import argparse
import gzip
from pathlib import Path
from panel import sha,write_json
SOURCES={"develop":"SRR177/003/SRR1770413/SRR1770413_1","reserve":"SRR835/SRR835775/SRR835775_1"}
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("work",type=Path);p.add_argument("manifest",type=Path)
    a=p.parse_args();d=a.work/"transfer";results={}
    for k,run in SOURCES.items():
        source=d/(k+".gz");target=d/(k+".fastq")
        headers=(d/(k+".headers")).read_text().lower()
        assert source.stat().st_size==1<<25 and "content-range: bytes 0-33554431/" in headers
        with gzip.open(source,"rb") as stream,target.open("wb") as out:
            for _ in range(100000):
                parts=[stream.readline() for _ in range(4)]
                assert parts[0].startswith(b"@") and parts[2].startswith(b"+") and parts[3].endswith(b"\n")
                assert len(parts[1].rstrip(b"\r\n"))==len(parts[3].rstrip(b"\r\n"))
                out.writelines(parts)
        results[k]=dict(url="https://ftp.sra.ebi.ac.uk/vol1/fastq/"+run+".fastq.gz",
                        prefix_sha256=sha(source),prefix_bytes=source.stat().st_size,
                        target_sha256=sha(target),target_bytes=target.stat().st_size,records=100000)
    write_json(a.manifest,results)
if __name__=="__main__":
    main()
