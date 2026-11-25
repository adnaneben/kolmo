import csv, hashlib, os, pathlib, random, resource, statistics, subprocess, tempfile, time
root=pathlib.Path('/tmp/kolmo-ppp-stream-main')
gold=pathlib.Path('/tmp/kolmo-audit-20261125.JEW7JB/main/build/kolmo')
trial=pathlib.Path('/tmp/kolmo-waves-20261125/stream-trial')
rng=random.Random(61794)
arms={'main':gold,'backport':root/'build/kolmo'}
rows=[]
for workload in ('silesia-prefix','enwik8-prefix'):
    source=trial/workload
    frame=trial/(workload+'.kolmo')
    expected=hashlib.sha256(source.read_bytes()).hexdigest()
    for cap in ('2M','max'):
        for threads in (1,4):
            group=[]
            with tempfile.TemporaryFile() as out:
                for rep in range(-1,7):
                    order=list(arms)
                    rng.shuffle(order)
                    for name in order:
                        out.seek(0);out.truncate()
                        u=resource.getrusage(resource.RUSAGE_CHILDREN)
                        start=time.perf_counter()
                        r=subprocess.run([str(arms[name]),'-d','-c','-T',str(threads),'-M',cap,str(frame)],stdout=out,stderr=subprocess.PIPE)
                        wall=time.perf_counter()-start
                        v=resource.getrusage(resource.RUSAGE_CHILDREN)
                        cpu=v.ru_utime+v.ru_stime-u.ru_utime-u.ru_stime
                        out.seek(0)
                        check=hashlib.sha256()
                        for data in iter(lambda: out.read(1 << 20), b''): check.update(data)
                        actual=check.hexdigest()
                        if r.returncode or actual!=expected: raise RuntimeError((name,cap,threads,rep,r.returncode,actual,r.stderr))
                        row=dict(workload=workload,cap=cap,threads=threads,rep=rep,arm=name,wall=wall,cpu=cpu,sha256=actual)
                        rows.append(row)
                        if rep>=0: group.append(row)
            summary={}
            for measure in ('wall','cpu'):
                summary[measure]=[statistics.median(r[measure] for r in group if r['arm']==a) for a in arms]
            print(workload,cap,threads,summary,flush=True)
csvfile=root.parent/'kolmo-ppp-stream-replay.csv'
with csvfile.open('w') as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0])); w.writeheader();w.writerows(rows)
