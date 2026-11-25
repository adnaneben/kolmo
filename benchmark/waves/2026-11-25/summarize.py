import json
from pathlib import Path
from statistics import median

def read(name):
    return json.loads(Path(name).read_text())
def rows(name):
    p = Path(name)
    return [json.loads(line) for line in p.read_text().splitlines()] if p.exists() else []
def f(before, after, unit=1, digits=2):
    return f'{before/unit:,.{digits}f} → {after/unit:,.{digits}f}'
def bytes_pair(a, b):
    return f'{a:,}' if a == b else f'{a:,} → {b:,}'
def table(head, records):
    print('| '+' | '.join(head)+' |')
    print('| '+' | '.join(['---'] * len(head))+' |')
    for row in records:
        print('| '+' | '.join(map(str,row))+' |')
    print()

wave1, wave2 = read('wave1-score.json'), read('wave2-score.json')
print('# Paired performance results\n')
print('Apple M5, macOS 27.2, Apple Clang 21.0.0, native -O3 and FP contraction disabled. Each run used real CLI file-to-file encoding followed by ordinary v12 decoding and a SHA-256 comparison. The record includes process launch and file I/O. Each preset and file has three randomized pairs. Small deltas are directional on this shared host. There is no p95 claim from this panel.\n')
print('For each preset, aggregate decimal MB/s uses the total original bytes divided by the sum of per-file median phase times. Combined uses each trial’s paired encode and decode time before computing per-file medians. Ratio is original divided by stored bytes. RSS is the largest observed phase RSS over the eight files and repetitions. Wave 1 appears in both independent sessions so each before/after comparison uses the same session.\n')
print('Core panel inputs, exactly as preserved in the previous audit. ' + ', '.join(wave1['panel']) + '.\n')
heads = ['Level', 'Stored bytes', 'Ratio', 'Encode MB/s', 'Decode MB/s', 'Combined MB/s', 'Combined ms', 'Peak RSS MiB']
for score, before, after, title in [(wave1,'main','wave1','Pinned main to Wave 1'),(wave2,'wave1','wave2','Wave 1 to Wave 2')]:
    print('## '+title+'\n')
    paired = {(d['arm'],d['name'],d['level'],d['threads']):d for d in score['details']}
    keys = [(name,lv,th) for arm,name,lv,th in paired if arm==before and (after,name,lv,th) in paired]
    exact = sum(paired[(before,*key)]['frame_sha256']==paired[(after,*key)]['frame_sha256'] for key in keys)
    print(f"{score['n_rows']} verified round trips. {exact}/{len(keys)} paired input/level/thread settings produced byte-identical frames.\n")
    out=[]
    for lv in score['levels']:
        a,b = [score['groups'][x][lv] for x in (before,after)]
        r=a['raw']
        assert b['raw']==r
        ratio = f"{r/a['size']:.5f}×" if a['size']==b['size'] else f"{r/a['size']:.5f}× → {r/b['size']:.5f}×"
        rates = [f(r/a[k],r/b[k],1e6) for k in ('enc_s','dec_s','total_s')]
        out.append([lv,bytes_pair(a['size'],b['size']),ratio,*rates,
                    f(a['total_s'],b['total_s'],1e-3),f(a['peak_rss'],b['peak_rss'],2**20)])
    table(heads,out)
    print('### Full-size and specialist inputs\n')
    out=[]
    for name,lv,th in keys:
        if name in score['panel']:
            continue
        a,b = [paired[(x,name,lv,th)] for x in (before,after)]
        r=a['raw']
        out.append([name,lv,th,bytes_pair(a['size'],b['size']),f(r/a['total_s'],r/b['total_s'],1e6),
                    f(a['enc_cpu_s'],b['enc_cpu_s']),f(a['dec_cpu_s'],b['dec_cpu_s']),
                    f(max(a['enc_rss'],a['dec_rss']),max(b['enc_rss'],b['dec_rss']),2**20)])
    table(['Input','Level','Threads','Stored bytes','Combined MB/s','Encode CPU s','Decode CPU s','Peak RSS MiB'],out)

fast=rows('fast-recheck.jsonl')
if fast:
    print('## Level-1 recheck\n')
    print('The broad Wave 2 pass had a visible level-1 slowdown concentrated in BGL. A focused retest ran 15 pairs on BGL and two controls. This remains a shared-host measurement.\n')
    out=[]
    for name in sorted({r['input'] for r in fast}):
        a,b=([r for r in fast if r['input']==name and r['arm']==x] for x in ['wave1','wave2'])
        assert len(a)==len(b)==15
        assert len({r['frame_sha256'] for r in a+b})==1
        ae,be=[median(r['enc']['wall_s'] for r in g) for g in (a,b)]
        ad,bd=[median(r['dec']['wall_s'] for r in g) for g in (a,b)]
        at,bt=[median(r['enc']['wall_s']+r['dec']['wall_s'] for r in g) for g in (a,b)]
        out.append([name,f(ae,be,.001,3),f(ad,bd,.001,3),f(at,bt,.001,3),f'{(bt/at-1)*100:+.2f}%'])
    table(['Input','Encode ms','Decode ms','Combined ms','Combined time change'],out)

tensor=rows('tensor.jsonl')
if tensor:
    print('## Opt-in level-9 weight retry\n')
    print('Default Wave 2 and its explicit tensor override used the same native encoder. Every completed frame was decoded with the v0.0.2 release build. The known BF16 results are an engineering confirmation of the original regression. Matching family files are one producer group.\n')
    out=[]
    for name,th in sorted({(r['input'],r['threads']) for r in tensor}):
        a,b=([r for r in tensor if (r['input'],r['threads'],r['arm'])==(name,th,x)]
             for x in ('wave2-default','wave2-tensor-opt-in'))
        if len(a)<7 or len(b)<7:
            continue
        assert len({r['frame_sha256'] for r in a})==len({r['frame_sha256'] for r in b})==1
        n=a[0]['raw_bytes']
        ae,be=[median(r['enc']['wall_s'] for r in g) for g in (a,b)]
        ad,bd=[median(r['dec']['wall_s'] for r in g) for g in (a,b)]
        at,bt=[median(r['enc']['wall_s']+r['dec']['wall_s'] for r in g) for g in (a,b)]
        ac,bc=[median(r['enc']['cpu_s'] for r in g) for g in (a,b)]
        ram=[max(max(r['enc']['rss_bytes'],r['dec']['rss_bytes']) for r in g) for g in (a,b)]
        size=[g[0]['stored_bytes'] for g in (a,b)]
        out.append([name,th,bytes_pair(*size),f'{(size[1]/size[0]-1)*100:+.2f}%',
                    f(n/ae,n/be,1e6),f(n/ad,n/bd,1e6),f(n/at,n/bt,1e6),f(ac,bc),f(*ram,2**20)])
    table(['Input','Threads','Stored bytes','Size change','Encode MB/s','Decode MB/s','Combined MB/s','Encode CPU s','Peak RSS MiB'],out)

neon=rows('neon.jsonl')
if neon:
    print('## ARM64 mixer intrinsics\n')
    print('Direct CM4 kernel harness. Each case used one 256 KiB prefix, forced scalar or NEON dispatch, and seven paired trials after one warmup. Both paths emitted exactly the same bytes and decoded one shared scalar reference. This measures direct CM4 execution, including the harness process and I/O.\n')
    out=[]
    for name in sorted({r['sample'] for r in neon}):
        a,b=([r for r in neon if r['sample']==name and r['isa']==x] for x in ('c','neon'))
        if len(a)<7 or len(b)<7:
            continue
        assert len({r['frame_sha256'] for r in a+b})==1
        ae,be=[median(r['encode']['wall_ns'] for r in g) for g in (a,b)]
        ad,bd=[median(r['decode']['wall_ns'] for r in g) for g in (a,b)]
        at,bt=[median(r['encode']['wall_ns']+r['decode']['wall_ns'] for r in g) for g in (a,b)]
        n=a[0]['raw_bytes']
        out.append([name,a[0]['frame_bytes'],f(ae,be,1e6),f(ad,bd,1e6),f(n*1e9/at,n*1e9/bt,1e6)])
    table(['Input','Stored bytes','Encode ms','Decode ms','Combined MB/s'],out)

print('## Stream-index reuse follow-up\n')
print('The archive-tested Wave 2 source and the #79 follow-up decoded the same valid 32 MiB CZNS members. The source was encoded once using level 1, one thread, analysis disabled, a 4 KiB block target and a 64 MiB stream segment. Compare seven randomized pairs after one warmup on each input, cap and arm. Decode wall includes file I/O and process startup. The decoder memory option does not include the compressed member buffer, as reflected in process RSS.\n')
out=[]
for file,name in [('stream-reuse.jsonl','Silesia prefix'),('stream-enwik.jsonl','enwik8 prefix')]:
    stream=rows(file)
    for limit in ('2M','max'):
        a,b=([r for r in stream if r['mode']==arm and r['limit']==limit] for arm in ('before','reuse'))
        if len(a)<7 or len(b)<7:
            continue
        assert len({(r['frame_sha256'],r['raw_sha256']) for r in a+b})==1
        ms=[median(r['wall_s'] for r in g) for g in (a,b)]
        cpu=[median(r['cpu_s'] for r in g) for g in (a,b)]
        rss=[max(r['rss_bytes'] for r in g) for g in (a,b)]
        out.append([name,limit,f"{a[0]['frame_bytes']:,}",f(*ms,.001,3),f'{ms[0]/ms[1]:.2f}×',f(*cpu,.001,3),f(*rss,2**20)])
table(['Input','Memory option','Frame bytes','Decode wall ms','Decode speedup','Decode CPU ms','Peak RSS MiB'],out)

zipnn=rows('zipnn.jsonl')
if zipnn:
    print('## ZipNN BF16 size reference\n')
    print('ZipNN 0.5.4 ran in-process with one thread. The runner compressed the BF16 payload with its stock dtype byte grouping and preserved the original SafeTensors header verbatim as counted side data. Each of seven warm trials restored the exact entire file. The timer starts at the input copy. Process launch, input/output file I/O and native CLI framing are different from the Kolmo replay, so compare size directly and keep these timing boundaries distinct.\n')
    out=[]
    for name in sorted({r['input'] for r in zipnn}):
        z=[r for r in zipnn if r['input']==name]
        assert len(z)==7 and len({r['stored_bytes'] for r in z})==1
        k=[r for r in tensor if r['input']==name and r['arm']=='wave2-tensor-opt-in']
        kbytes=f"{k[0]['stored_bytes']:,}" if len(k)>=7 else 'Pending'
        out.append([name,f"{z[0]['stored_bytes']:,}",kbytes,
                    f"{median(r['encode_s']+r['decode_s'] for r in z)*1000:.3f}"])
    table(['Input','ZipNN including header','Kolmo opt-in level-9 bytes','ZipNN warm encode + decode ms'],out)
