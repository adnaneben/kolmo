import csv, hashlib, pathlib,random,resource,statistics,subprocess,tempfile,time
p=pathlib.Path('/tmp/kolmo-ppp-stream-inputs'); bench=pathlib.Path('/tmp/kolmo-waves-20261125/stream-trial')
old='/tmp/kolmo-audit-20261125.JEW7JB/main/build/kolmo'; new='/tmp/kolmo-ppp-stream-main/build/kolmo'
rng=random.Random(14813)
rows=[]
for size in (4,16,32):
    for corpus in ('silesia-prefix','enwik8-prefix'):
        source=p/f'{corpus}-{size}'
        frame=p/f'{corpus}-{size}.kolmo'
        if size<32:
            with (bench/corpus).open('rb') as f:source.write_bytes(f.read(size<<20))
            with frame.open('wb') as f:
                subprocess.run([old,'-1','--analysis','none','--block-size','0.00390625','--stream=64','-T','1','-c',str(source)],stdout=f,check=True)
        else:source=bench/corpus;frame=bench/(corpus+'.kolmo')
        sha=hashlib.sha256(source.read_bytes()).hexdigest()
        for threads in (1,4):
            section=[]
            for rep in range(-1,15):
                order=[('A',old),('A2',old),('B',new)]
                rng.shuffle(order)
                for name,exe in order:
                    with tempfile.TemporaryFile() as f:
                        u=resource.getrusage(resource.RUSAGE_CHILDREN);t=time.perf_counter()
                        subprocess.run([exe,'-d','-c','-T',str(threads),'-M','max',str(frame)],stdout=f,check=True)
                        d=time.perf_counter()-t;v=resource.getrusage(resource.RUSAGE_CHILDREN)
                        cpu=v.ru_utime+v.ru_stime-u.ru_utime-u.ru_stime
                        f.seek(0);h=hashlib.sha256()
                        for b in iter(lambda:f.read(1048576),b''):h.update(b)
                        assert h.hexdigest()==sha
                    r=dict(size_mib=size,corpus=corpus,threads=threads,rep=rep,arm=name,wall=d,cpu=cpu,sha256=sha)
                    rows.append(r)
                    if rep>=0:section.append(r)
            pairs={k:{r['arm']:r for r in section if r['rep']==k} for k in range(15)}
            print(size,corpus,threads,*['%s %s %+5.2f%%'% (arm,metric,100*(statistics.median(x[arm][metric]/x['A'][metric] for x in pairs.values())-1)) for arm in ('A2','B') for metric in ('wall','cpu')],flush=True)
with open('/tmp/kolmo-ppp-stream-retest.csv','w') as f:
    w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)
