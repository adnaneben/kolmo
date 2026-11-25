"""Supplementary valid-file public decode-limit measurements on the same regular and piped frame."""
import argparse,csv,shutil,subprocess,tempfile
from pathlib import Path
from replay import instrument,sha
ap=argparse.ArgumentParser()
for key in ('source','zip','out'):ap.add_argument('--'+key,required=True,type=Path)
a=ap.parse_args()
with tempfile.TemporaryDirectory(dir=a.out) as td:
    t=Path(td);src=t/'src';shutil.copytree(a.source/'src',src);instrument(src)
    probe=Path(__file__).with_name('probe.c').read_text()
    old='kolmo_stream_decompress(0,output,nt)'
    assert probe.count(old)==1
    probe=probe.replace('uint64_t before=rss()', 'kolmo_decode_limits limits={0}; limits.max_memory=(uint64_t)atoi(argv[4])<<20;\n    uint64_t before=rss()').replace(old,'kolmo_stream_decompress_limited(0,output,nt,&limits)')
    (t/'probe.c').write_text(probe)
    exe=t/'probe'
    subprocess.run(['cc','-O3','-mcpu=native','-ffp-contract=off','-Wno-unknown-attributes','-Wno-constant-logical-operand','-I'+str(src),t/'probe.c',a.source/'build/libkolmoarch.a','-lpthread','-lm','-Wl,-dead_strip','-o',exe],check=True)
    frame=t/'silesia.frame';output=t/'restored';original=sha(a.zip)
    with a.zip.open('rb') as f: subprocess.run([exe,'encode-frame','1',frame,'1'],stdin=f,check=True)
    h=sha(frame)
    fields='raw_bytes decoded_frame_count stream_frame_peak_B fd_index_B fd_packed_workers_peak_B compressed_input_peak_B stream_output_window_peak_B tls_retained_after_B caller_mapped_B rss_before_B rss_after_B rss_peak_B elapsed_ns'.split()
    with (a.out/'limited.csv').open('x',newline='') as raw:
        w=csv.writer(raw);w.writerow(['input_kind','limit_MiB','repeat','frame_sha256','restored_sha256','probe_sha256',*fields])
        for rep in range(3):
            for cap in (32,64):
                for desc in ('regular','pipe'):
                    with frame.open('rb') as f:
                        feed=None; inp=f
                        if desc=='pipe':feed=subprocess.Popen(['/bin/cat'],stdin=f,stdout=subprocess.PIPE);inp=feed.stdout
                        x=subprocess.check_output([exe,'stream','4',output,str(cap)],stdin=inp,text=True).strip().split(',')
                        if feed: feed.stdout.close();assert feed.wait()==0
                    restored=sha(output);assert restored==original and len(x)==len(fields)
                    w.writerow([desc,cap,rep,h,restored,sha(exe),*x]);raw.flush()
