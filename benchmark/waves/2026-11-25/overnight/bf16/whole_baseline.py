"""Run the single pinned-main L3 point for each verified whole model after the competition panel."""
import csv
import json
import os
from pathlib import Path
import sys
import tempfile
from whole_checkpoints import file_hash, timed

root=Path(__file__).resolve().parent
binary=Path(sys.argv[1]).resolve()
inputs=json.loads((root/"whole-inputs.json").read_text())["inputs"]
old=json.loads((root/"inventory.json").read_text())["binary"]["main"]
assert file_hash(binary)==old["sha256"]
fields="model commit binary_sha256 bytes packed_bytes packed_sha256 restored_sha256 encode_wall_ns decode_wall_ns encode_rss_bytes decode_rss_bytes".split()
with tempfile.TemporaryDirectory(prefix="kolmo-weights-main-") as td, (root/"whole-main.csv").open("w",newline="") as dest:
    w=csv.writer(dest); w.writerow(fields)
    for model,info in inputs.items():
        source=Path(info["path"]); t=Path(td); pack=t/"packed"; restored=t/"restored"
        e,er,_=timed([binary,"-q","-3","-T1","-o",pack,source],os.environ,t/"time")
        d,dr,_=timed([binary,"-q","-d","-T1","-o",restored,pack],os.environ,t/"time")
        h=file_hash(restored); ph=file_hash(pack)
        assert h==info["sha256"] and restored.stat().st_size==info["bytes"]
        w.writerow([model,"a1d39559686a9ec17c8614b9154a64d7b5b98672",old["sha256"],info["bytes"],pack.stat().st_size,ph,h,e,d,er,dr])
        dest.flush()
        print(model,pack.stat().st_size,ph,flush=True)
        pack.unlink(); restored.unlink()
