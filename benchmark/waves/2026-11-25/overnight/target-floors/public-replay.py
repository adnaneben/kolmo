"""Compile one consumer against the old public header and link it unchanged to each build."""
import argparse,csv,json,math,subprocess,tempfile
from pathlib import Path
ap=argparse.ArgumentParser()
for k in ('baseline_tree','fixed_tree','b26','out'):ap.add_argument('--'+k.replace('_','-'),required=True,type=Path)
a=ap.parse_args();producer=json.loads((a.b26/'freeze.json').read_text())['producers']
with tempfile.TemporaryDirectory(dir=a.out) as td, (a.out/'public-cost.csv').open('x',newline='') as of:
    td=Path(td);obj=td/'consumer.o'
    subprocess.run(['cc','-O2','-I'+str(a.baseline_tree/'src'),'-c',Path(__file__).with_name('public-report.c'),'-o',obj],check=True)
    w=csv.writer(of);w.writerow('source target build chosen predicted_bytes actual_bytes predicted_cost predicted_enc_s predicted_dec_s report_size_B predicted_size_offset_B'.split())
    for build,tree in [('before',a.baseline_tree),('after',a.fixed_tree)]:
        out=td/build
        subprocess.run(['cc',obj,tree/'build/libkolmo.a','-lpthread','-lm','-o',out],check=True)
        for name,rec in producer.items():
            for spec in ['encode>=999GB/s','cost:size*2+enc*3+dec*5,decode>=999GB/s']:
                row=subprocess.check_output([out,spec,rec['source']],text=True).strip().split(',')
                pred,actual=int(row[1]),int(row[2]);cost,enc,dec=map(float,row[3:6]);assert actual>0
                if build=='before':assert pred==0 and cost==0
                else:
                    assert pred==actual
                    expected=actual if spec.startswith('encode') else 2*actual+3*enc+5*dec
                    assert math.isclose(cost,expected,rel_tol=1e-12)
                w.writerow([name,spec,build,*row])
