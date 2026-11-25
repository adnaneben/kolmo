"""Replay with a macOS default build, a whole GHCN CSV and the whole Silesia ZIP."""
import argparse
import csv
import hashlib
import json
import os
import random
import shutil
import subprocess
import tempfile
from pathlib import Path
def sha(p):
    with p.open('rb') as f:
        h=hashlib.sha256()
        for b in iter(lambda:f.read(1<<20),b''): h.update(b)
        return h.hexdigest()
def change(file, old, new):
    s=file.read_text();assert s.count(old)==1,old
    file.write_text(s.replace(old,new))
def instrument(src):
    s=src/'core/stream.c'
    change(s,'int r = sdec_frame(D, *frame, have);','member(*fcap); int r = sdec_frame(D, *frame, have);')
    change(s,'return sdec_frame(D, *frame, have);','member(*fcap); return sdec_frame(D, *frame, have);')
    change(s,'int64_t dl = 0; uint32_t b = 0;','maxwith(&window_peak,D->ocap); int64_t dl = 0; uint32_t b = 0;')
    f=src/'api/api.c'
    change(f,'if (!index) return KOLMO_ERR_ALLOC;\n    memcpy(index, header, HDR);',
             'if (!index) return KOLMO_ERR_ALLOC; read_index(index_size);\n    memcpy(index, header, HDR);')
    d=src/'core/decode.c'
    change(d,'codec_ctx *xp = &S->x;','if(packed) packed_add(J->maxcompressed ? J->maxcompressed : 1); codec_ctx *xp = &S->x;')
    change(d,'\n    free(packed);\n','\n    if(packed) packed_drop(J->maxcompressed ? J->maxcompressed : 1); free(packed);\n')
    change(d,'pb = malloc(cl); if (!pb) return KOLMO_ERR_ALLOC;','pb = malloc(cl); if (!pb) return KOLMO_ERR_ALLOC; packed_add(cl);')
    change(d,'free(pb); free(t);','if(pb) packed_drop(cl); free(pb); free(t);')
def main():
    ap=argparse.ArgumentParser()
    for k in ('source','csv','zip','out'):ap.add_argument('--'+k,type=Path,required=True)
    a=ap.parse_args();a.out=a.out.resolve();a.out.mkdir(parents=True,exist_ok=True)
    arch=a.source/'build/libkolmoarch.a'
    with tempfile.TemporaryDirectory(dir=a.out) as tmp:
        t=Path(tmp);src=t/'src';shutil.copytree(a.source/'src',src)
        instrument(src)
        exe=t/'probe'
        subprocess.run(['cc','-O3','-mcpu=native','-ffp-contract=off','-Wno-unknown-attributes','-Wno-constant-logical-operand','-I'+str(src),Path(__file__).with_name('probe.c'),arch,'-lpthread','-lm','-Wl,-dead_strip','-o',exe],check=True)
        meta={'source_commit':subprocess.check_output(['git','-C',a.source,'rev-parse','HEAD'],text=True).strip(),'instrument_binary_sha256':sha(exe),'architecture_archive_sha256':sha(arch),'stock_cli_sha256':sha(a.source/'build/kolmo'),'inputs':{}}
        cases=[];frames={};sources={'ghcn':a.csv,'silesia-zip':a.zip}
        for name,original in sources.items():
            m=meta['inputs'][name]={'original':original.name,'bytes':original.stat().st_size,'sha256':sha(original),'level':3 if name=='ghcn' else 1,'frames':{}}
            for enc in ('frame','stream'):
                frame=t/(name+'.'+enc)
                with original.open('rb') as f:subprocess.run([exe,'encode-'+enc,'1',frame,str(m['level'])],stdin=f,check=True)
                m['frames'][enc]={'bytes':frame.stat().st_size,'sha256':sha(frame)}
                frames[name,enc]=frame
                for api,fd in [('stream','regular'),('stream','pipe')]+([('file','regular'),('mapped','regular')] if enc=='frame' else []):
                    for nt in (1,4):cases.append((name,enc,api,fd,nt))
        output=t/'restored';rng=random.Random(202611259)
        fields='raw_bytes decoded_frame_count stream_frame_peak_B fd_index_B fd_packed_workers_peak_B compressed_input_peak_B stream_output_window_peak_B tls_retained_after_B caller_mapped_B rss_before_B rss_after_B rss_peak_B elapsed_ns'.split()
        with (a.out/'raw.csv').open('x',newline='') as f:
            w=csv.writer(f);w.writerow(['source','format','api','input_kind','threads','repeat','restored_sha256',*fields])
            for rep in range(3):
                rng.shuffle(cases)
                for name,enc,api,kind,nt in cases:
                    frame=frames[name,enc];args=[exe,api,str(nt),output,'0']
                    with frame.open('rb') as data:
                        if kind=='regular': answer=subprocess.check_output(args,stdin=data,text=True)
                        else:
                            feed=subprocess.Popen(['/bin/cat'],stdin=data,stdout=subprocess.PIPE)
                            answer=subprocess.check_output(args,stdin=feed.stdout,text=True);feed.stdout.close();assert feed.wait()==0
                    restored=sha(output);assert restored==meta['inputs'][name]['sha256']
                    v=answer.strip().split(',');assert len(v)==len(fields)
                    w.writerow([name,enc,api,kind,nt,rep,restored,*v]);f.flush()
                print('pass',rep,'complete',flush=True)
        (a.out/'manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
if __name__=='__main__':main()
