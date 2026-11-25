"""Validate the bounded automatic-block experiment, provenance, reader outputs and phase timing."""
import csv,hashlib,json,statistics,struct
from collections import defaultdict
from pathlib import Path
root=Path(__file__).resolve().parent/"review"
meta=json.loads((root/"capped-large-inputs.json").read_text())
supply=json.loads((root/"source-capacity.json").read_text())
assert supply["unconstrained_ultra_block_bytes"]==128<<20
assert (root/"block-target.txt").read_text().strip()=="unconstrained=134217728 cap_256_MiB=8388608"
outputs=[]
for f,block_count in [("capped-small.csv",1),("capped-large.csv",2)]:
    grouped=defaultdict(list)
    with (root/f).open() as stream:
        for row in csv.DictReader(stream):
            run=row["accession"]; arm=row["arm"]; size=int(row["raw_bytes"])
            assert int(row["encoder_cap_bytes"])==256<<20 and row["sha256"]==row["restored_sha256"]
            if int(row["round"])==0: assert row["tagged_reader_restore"]==row["sha256"]
            assert int(row["blocks"])==block_count
            cuts=list(map(int,row["outer_offsets"].split("|")))
            assert cuts[0]==0 and cuts[-1]==size
            assert hashlib.sha256(b"".join(struct.pack("<Q",x) for x in cuts)).hexdigest()==row["outer_offsets_sha256"]
            assert all(int(row[k])>0 for k in ("encode_ns","stock_decode_ns","stored_bytes"))
            if block_count==2:
                assert meta[run]["sha256"]==row["sha256"] and meta[run]["input_bytes"]==size and size<=supply["inputs"][run]["complete_record_bytes"]
                if arm=="fixed": assert cuts[1]==8<<20 and int(row["cuts_on_record"])==0
                else: assert cuts[1]<8<<20 and int(row["cuts_on_record"])==1
            grouped[run,arm].append(row)
    assert len(grouped)==4
    for (run,arm),rows in sorted(grouped.items()):
        assert len(rows)==block_count and len({r["stored_sha256"] for r in rows})==1
        if block_count==1: assert grouped[run,"fixed"][0]["stored_sha256"]==rows[0]["stored_sha256"]
        e=[int(r["encode_ns"]) for r in rows];d=[int(r["stock_decode_ns"]) for r in rows]
        n=int(rows[0]["raw_bytes"]);times=[statistics.median(x) for x in (e,d,[a+b for a,b in zip(e,d)])]
        outputs.append([f,run,arm,len(rows),n,rows[0]["stored_bytes"],rows[0]["stored_sha256"],rows[0]["codecs"],round(times[0]/1e6,4),round(times[1]/1e6,4),round(n*1000/times[2],4)])
with (root/"capped-score.csv").open("w",newline="") as out:
    w=csv.writer(out);w.writerow("experiment accession arm repetitions input_bytes archive_bytes archive_sha256 codecs encode_ms decode_ms combined_MBps".split());w.writerows(outputs)
print("Verified 12 capped-automatic stock restores, 8 released-reader restores and the actual 8 MiB cut in both larger source slices.")
