"""Run a native Model 4 explicit-cap transfer comparison from pinned external inputs."""
import argparse, collections, csv, hashlib, json, os, pathlib, random, statistics, struct, subprocess, tempfile, time
from pathlib import Path

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def timed(args):
    env={k:v for k,v in os.environ.items() if not k.startswith(('KOLMO_', 'CZN_'))}
    start=time.perf_counter()
    pid=os.posix_spawn(str(args[0]),[str(v) for v in args],env)
    _,status,usage=os.wait4(pid,0)
    if os.waitstatus_to_exitcode(status):raise RuntimeError(str(args))
    return time.perf_counter()-start,usage.ru_utime+usage.ru_stime,usage.ru_maxrss

def headers(p):
    with p.open('rb') as f:
        h=f.read(32)
        assert h[:5]==b'CZN1\x0c'
        count=struct.unpack_from('<I',h,20)[0];assert 1<=count<=4096
        index=f.read(16*count);out=[]
        for i in range(count):
            raw,stored,_,codec,transform=struct.unpack_from('<IIIBB',index,16*i);b=f.read(stored)
            assert len(b)==stored
            # Four native plan bytes precede the CM stream. The ninth model nibble denotes Model 4.
            mode4=codec==7 and len(b)>=16 and b[8]>>4==9
            out.append({'raw':raw,'stored':stored,'codec':codec,'transform':transform,'prefix':b[:16].hex(),
                        'shift':(b[8]&15)+2 if mode4 else None})
        return out

def main():
    ap=argparse.ArgumentParser()
    for a in ['baseline','candidate','released','brotli','out']:ap.add_argument(a,type=Path)
    args=ap.parse_args();w=args.out.resolve()
    sources=[('GMP Info',Path('/opt/homebrew/share/info/gmp.info-1'),None),('Libgcrypt Info',Path('/opt/homebrew/share/info/gcrypt.info-1'),None),
             ('Webster 512KiB',Path('/usr/share/dict/web2'),524288)]
    arms={'main numeric17':(args.baseline,'17'),'fixed numeric17':(args.candidate,'17'),'main full':(args.baseline,'full'),'Brotli 11':(args.brotli,None)}
    controls={'main scaled':(args.baseline,'scaled'),'fixed scaled':(args.candidate,'scaled'),'fixed full':(args.candidate,'full'),'fixed numeric16':(args.candidate,'16')}
    rows=[];cross=[]; seen=set(); control_checks=[]
    manifest={'source_rev':'a1d39559686a9ec17c8614b9154a64d7b5b98672','binaries':{k:sha(getattr(args,k)) for k in ('baseline','candidate','released','brotli')},'inputs':{},
              'platform':os.uname().machine,'brotli_version':subprocess.check_output([args.brotli,'--version'],text=True).strip(),'seed':663710}
    rng=random.Random(manifest['seed'])
    with tempfile.TemporaryDirectory() as sd, (w/'trials.jsonl').open('w') as stream:
        tmp=Path(sd)
        for name,p,limit in sources:
            src=tmp/'subject';src.write_bytes(p.read_bytes()[:limit]);expected=sha(src)
            manifest['inputs'][name]=dict(file=p.name,whole_file_sha256=sha(p),sha256=expected,bytes=src.stat().st_size,slice='all' if limit is None else f'first {limit} bytes')
            (w/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
            hashes=collections.defaultdict(set)
            for rep in range(8):
                active=controls if rep==7 else arms
                for arm in rng.sample(list(active),len(active)):
                    exe,mode=active[arm];frame=tmp/'frame';back=tmp/'restored'
                    frame.unlink(missing_ok=True);back.unlink(missing_ok=True)
                    if mode is None:
                        enc=[exe,'-q','11','-n','-f','-o',frame,src];dec=[exe,'-d','-n','-f','-o',back,frame]
                    else:
                        enc=[exe,'-U','-T','1','--x-tables='+mode,'-q','-f',src,'-o',frame];dec=[exe,'-d','-T','1','-q','-f',frame,'-o',back]
                    e,ec,er=timed(enc);d,dc,dr=timed(dec)
                    assert sha(back)==expected
                    u=sha(frame);hashes[arm].add(u);hs=headers(frame) if mode else []
                    rec=dict(input=name,arm=arm,rep=rep if rep<7 else None,input_sha256=expected,raw_bytes=src.stat().st_size,
                             stored_bytes=frame.stat().st_size,frame_sha256=u,restore_sha256=sha(back),encode_s=e,decode_s=d,enc_cpu=ec,dec_cpu=dc,
                             enc_rss_bytes=er,dec_rss_bytes=dr,blocks=hs,shared_host_load1=os.getloadavg()[0])
                    stream.write(json.dumps(rec)+'\n');stream.flush();rows.append(rec)
                    if mode is not None and u not in seen:
                        back.unlink()
                        timed([args.released,'-d','-T','1','-q','-f',frame,'-o',back]);assert sha(back)==expected
                        cross.append(dict(input=name,arm=arm,frame_sha256=u,released_restore_sha256=sha(back)))
                        (w/'stock-compatibility.json').write_text(json.dumps(cross,indent=2)+'\n');seen.add(u)
            assert all(len(v)==1 for v in hashes.values())
            for a,b in [('main numeric17','main scaled'),('main scaled','fixed scaled'),('main full','fixed full')]:
                assert hashes[a]==hashes[b];control_checks.append(dict(input=name,a=a,b=b,frame_sha256=next(iter(hashes[a]))))
    (w/'unchanged-settings.json').write_text(json.dumps(control_checks,indent=2)+'\n')
    out=[]
    for (name,arm) in dict.fromkeys((r['input'],r['arm']) for r in rows if r['rep'] is not None):
        rr=[r for r in rows if r['input']==name and r['arm']==arm]
        res=dict(input=name,arm=arm,trials=len(rr),raw_bytes=rr[0]['raw_bytes'],stored_bytes=rr[0]['stored_bytes'],shift=','.join(str(b['shift']) for b in rr[0]['blocks']))
        for key in ['encode_s','decode_s','enc_cpu','dec_cpu','enc_rss_bytes','dec_rss_bytes']:
            res[key]=statistics.median(r[key] for r in rr)
        res['paired_combined_s']=statistics.median(r['encode_s']+r['decode_s'] for r in rr)
        for rate,phase in [('encode_MB_s','encode_s'),('decode_MB_s','decode_s'),('combined_MB_s','paired_combined_s')]:
            res[rate]=res['raw_bytes']/res[phase]/1e6
        out.append(res)
    with (w/'summary.csv').open('w',newline='') as f:
        cw=csv.DictWriter(f,fieldnames=out[0]);cw.writeheader();cw.writerows(out)
if __name__=='__main__':main()
