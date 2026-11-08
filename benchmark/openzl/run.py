import subprocess, time, statistics, os, json, sys
D='/tmp/ozp/data/'; O='/tmp/ozp/out/'
K='/tmp/czfz/kolmo-repo/build/kolmo'; Z='/tmp/ozl/src/zli'
S='/tmp/lzb/sil/'
panel=[]  # (group, file, openzl profile args list of alternatives, trained zlc or None)
for m in ['dickens','mozilla','mr','nci','ooffice','osdb','reymont','samba','webster','xml','x-ray']:
    prof=[['-p','serial']]
    if m=='mr': prof+= [['-p','be-u16'],['-p','le-u16']]
    if m=='x-ray': prof+= [['-p','le-u16'],['-p','be-u16']]
    panel.append(('generic' if m not in ('mr','x-ray') else 'numeric 16-bit', S+m, prof, None))
panel.append(('SAO (OpenZL has a dedicated profile)', S+'sao', [['-p','sao'],['-p','sddl2','--profile-arg','/tmp/ozl/src/examples/sddl2/sao_silesia.sddl'],['-p','serial']], None))
panel.append(('CSV', D+'owid_test.csv', [['-p','csv']], '/tmp/ozl/t/owid.zlc'))
panel.append(('CSV', D+'energy_test.csv', [['-p','csv']], '/tmp/ozl/e/energy.zlc'))
panel.append(('CSV wide', D+'wide_test.csv', [['-p','csv','--profile-arg',';']], '/tmp/ozl/w/wide.zlc'))
panel.append(('Parquet', D+'owid_energy_test.parquet', [['-p','parquet']], '/tmp/ozp/energy_pq.zlc'))
panel.append(('Parquet', D+'owid_co2.parquet', [['-p','parquet']], None))
for f in ['img_astronaut.u8','img_coffee.u8','img_camera.u8','img_chelsea.u8']: panel.append(('numeric u8 (raw image)', D+f, [['-p','u8']], None))
panel.append(('numeric i32', D+'year.i32', [['-p','le-i32']], None))
panel.append(('numeric i64', D+'population.i64', [['-p','le-i64']], None))
panel.append(('numeric i64', D+'log_timestamps_ms.i64', [['-p','le-i64']], None))
panel.append(('numeric f64', D+'owid_f64_colmajor.u64', [['-p','le-u64'],['-p','numeric-ml-selector-64']], None))
for f in ['img_astronaut.npy','lfw_subset.npy','owid_energy_f64.npy']: panel.append(('NumPy', D+f, [['-p','numpy']], None))
panel.append(('Pickle', D+'owid_energy.pkl', [['-p','pickle']], None))
panel.append(('ONNX', D+'magika.onnx', [['-p','onnx']], None))
panel.append(('Safetensors', D+'magika.safetensors', [['-p','safetensors']], None))
panel.append(('GGUF', D+'magika.gguf', [['-p','gguf']], None))
def t(cmd):
    s=time.perf_counter(); r=subprocess.run(cmd,capture_output=True); e=time.perf_counter()-s
    return e, r.returncode, r.stderr.decode(errors='replace')[-300:]
def measure(cc, dc, cfile, src, back, reps):
    tcs=[];tds=[];err=''
    for r in range(reps):
        tc,rc,e=t(cc)
        if rc: return None, e
        td,rc,e=t(dc)
        if rc: return None, e
        tcs.append(tc); tds.append(td)
    ok = open(back,'rb').read()==open(src,'rb').read()
    return dict(size=os.path.getsize(cfile), tc=statistics.median(tcs), td=statistics.median(tds), ok=ok), ''
res=[]
for grp,src,profs,zlc in panel:
    n=os.path.getsize(src); name=os.path.basename(src)
    reps=3 if n<60e6 else 1
    rows=[]
    for kl,args in [('kolmo fast -q',['-m','fast','-q']),('kolmo fast',['-m','fast']),('kolmo hc',['-m','hc']),('kolmo max',['-m','max'])]:
        rp = 1 if (kl=='kolmo max' and n>20e6) else reps
        m,e=measure([K]+args+['-T1',src,O+'k.czn'],[K,'-d','-T1',O+'k.czn',O+'k.back'],O+'k.czn',src,O+'k.back',rp)
        rows.append((kl,m,e))
    for lvl in ['6','9']:
        best=None
        for pa in profs:
            m,e=measure([Z,'compress']+pa+['-l',lvl,'-f',src,'-o',O+'z.zl'],[Z,'decompress','-f',O+'z.zl','-o',O+'z.back'],O+'z.zl',src,O+'z.back',reps)
            if m and m['ok'] and (best is None or m['size']<best[1]['size']): best=(' '.join(pa),m)
            if not m: rows.append((f'openzl L{lvl} {" ".join(pa)}',None,e))
        if best: rows.append((f'openzl L{lvl} [{best[0]}]',best[1],''))
    if zlc:
        m,e=measure([Z,'compress','-c',zlc,'-f',src,'-o',O+'z.zl'],[Z,'decompress','-f',O+'z.zl','-o',O+'z.back'],O+'z.zl',src,O+'z.back',reps)
        rows.append(('openzl trained',m,e))
    rec={'group':grp,'file':name,'n':n,'rows':[{'set':a,'m':b,'err':c} for a,b,c in rows]}
    res.append(rec); json.dump(res,open('/tmp/ozp/results.json','w'),indent=1)
    best_k=min((r for r in rows if r[0].startswith('kolmo') and r[1]), key=lambda r:r[1]['size'])
    print(grp, name, n, [(r[0], r[1]['size'] if r[1] else 'ERR') for r in rows], flush=True)
print('DONE')
