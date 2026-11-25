"""Execute the same frozen input on stock, original controls and targeted portable references."""
from pathlib import Path
import csv
import hashlib
import json
import os
import subprocess
import sys
import tempfile
HERE=Path(__file__).resolve().parent
specs=[
    ("r0.0.2/text.txt.L4.kolmo","r0.0.2/in/text.txt"),
    ("r0.0.2/series.i32.L3.kolmo","r0.0.2/in/series.i32"),
    ("r0.0.2/values.f64.L3.kolmo","r0.0.2/in/values.f64"),
    ("img.ppm.fast.kolmo","img.ppm"),
    ("r0.0.2/table.csv.L3.kolmo","r0.0.2/in/table.csv"),
    ("r0.0.2/derived.csv.L3.kolmo","r0.0.2/in/derived.csv"),
    ("r0.0.2/stations.csv.L3.kolmo","r0.0.2/in/stations.csv"),
]
fields=['table_level','force_c','research_reference','crc_arm_bytes','crc_portable_bytes','decimal_neon_values',
        'decimal_swar_calls','int_neon_values','int_total_values','dec64_neon_values','dec64_total_values',
        'img8_neon_bytes','word_bulk_neon_output_bytes','word_remainder_output_bytes','word_remainder_neon_scan_iterations']
def sha(b): return hashlib.sha256(b).hexdigest()
def check(ok,msg):
    if not ok: raise ValueError(msg)
def main():
    repo,stock,probe,out=map(Path,sys.argv[1:])
    root=repo/'tests/v12'
    pins=(root.parent/'v12.sha256').read_bytes()
    check(sha(pins)=='acf083f620ee945e6e693f44ef7230e4df85c70d31fb204d7582bd18e8ab9c4d','unexpected corpus')
    pd={key:h for h,key in (r.split('  ',1) for r in pins.decode().splitlines())}
    check(sha(stock.read_bytes())=='dbd92f4a170e36c9701aec15236648cd7b78e9f90a180fc8ffd79661aebbb6f2','unrecognized #106 binary')
    rows=[]
    with tempfile.TemporaryDirectory() as tmp:
        dest=Path(tmp)/'restored'
        for fn,orig in specs:
            frame=(root/fn).read_bytes();data=(root/orig).read_bytes()
            check(sha(frame)==pd[fn] and sha(data)==pd[orig],'unpinned input')
            for arm in ['stock','native','cpu-c','force-c','reference']:
                env={k:v for k,v in os.environ.items() if not k.startswith(('KOLMO_','CZN_','ISA_RESEARCH_'))}
                if arm in ('cpu-c','force-c','reference'):env['KOLMO_CPU']='c'
                if arm in ('force-c','reference'):env['ISA_RESEARCH_CONTROL']=arm
                exe=stock if arm=='stock' else probe
                r=subprocess.run([exe,'-dq','-T1','-o',dest,'--',root/fn],env=env,capture_output=True,text=True,check=True)
                result=dest.read_bytes();dest.unlink()
                check(result==data,'wrong restored output '+fn)
                values={k:'' for k in fields}
                if arm!='stock':
                    nums=r.stderr.strip().split()
                    check(nums[0]=='ISATRACE' and len(nums)==len(fields)+1,r.stderr)
                    values=dict(zip(fields,map(int,nums[1:])))
                rows.append(dict(frame=fn,frame_bytes=len(frame),frame_sha256=sha(frame),original=orig,
                                 original_bytes=len(data),restored_sha256=sha(result),control=arm,**values))
    for i in range(0,len(rows),5):
        _,native,cpu,force,ref=rows[i:i+5]
        check(all(native[k]==cpu[k] for k in fields),'KOLMO_CPU=c diverged on arm64')
        check(all(ref[k]==0 for k in ['crc_arm_bytes','decimal_neon_values','int_neon_values','dec64_neon_values','img8_neon_bytes','word_bulk_neon_output_bytes','word_remainder_neon_scan_iterations']),str(ref))
        check(ref['crc_portable_bytes']==native['crc_arm_bytes']>0,'CRC bytes diverged')
    out.mkdir(parents=True,exist_ok=True)
    with (out/'measurements.csv').open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
    info=dict(source='8d23095b2f09d5c9e3c3425123bea63641ca5d71',stock_sha256=sha(stock.read_bytes()),
              instrumented_sha256=sha(probe.read_bytes()),host='Apple M5 arm64, macOS 27.2, Apple clang 21',
              inputs=len(specs),exact_decodes=len(rows),counts={})
    for a in ['native','cpu-c','force-c','reference']:
        info['counts'][a]={k:sum(row[k] for row in rows if row['control']==a) for k in fields[3:]}
    (out/'results.json').write_text(json.dumps(info,indent=2)+'\n')
    print(json.dumps(info,indent=2))
if __name__=='__main__': main()
