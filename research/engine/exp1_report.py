import csv,subprocess,os,sys,json
R=list(csv.DictReader(open('/tmp/cmp/exp1.csv')))
def find(n):
    for d in ('/tmp/lzb/sil/','/tmp/czfz/data3/','/tmp/ozp/data/','/tmp/lzb/json/','/tmp/newdata/'):
        if os.path.exists(d+n): return d+n
SIL={'dickens','mozilla','mr','nci','ooffice','osdb','reymont','samba','sao','webster','x-ray','xml'}
rows=[]
for r in R:
    f=find(r['file']); subprocess.run(['/tmp/cmp/kolmo_n20','-m','fast','-T1',f,'/dev/shm/e1.k'],check=True); kb=os.path.getsize('/dev/shm/e1.k')
    g=lambda k: float(r[k])
    A=g('zstd3'); L=g('zstd_lit_bytes'); K1=g('K1'); S=A-L
    def tot(lit): return A+(lit-K1)                     # zstd's frame with our literal coding (harness overhead cancels)
    best_static=min([g('O1K8'),g('O1K16')]+([g('POS')] if g('POS') else [])+([g('COL')] if g('COL') else [])+[g('SUBblk'),K1])
    d=dict(file=r['file'],set='silesia' if r['file'] in SIL else 'structured',n=int(g('n')),zstd3=int(A),kolmo_bal=kb,lit_share=L/A,
           o1k8=tot(g('O1K8')),o1k16=tot(g('O1K16')),pos=tot(g('POS')) if g('POS') else None,P=int(g('P')),col=tot(g('COL')) if g('COL') else None,sub=tot(g('SUBblk')),
           best_static=tot(best_static),bino1=tot(g('BINO1')),binmix=tot(g('BINMIX')),
           seq=S,seq_ctx=S*g('SC')/g('S0'),seq_adp=S*g('SA')/g('S0'),
           both_static=tot(best_static)-S+S*min(1,g('SC')/g('S0')),both_adapt=tot(g('BINMIX'))-S+S*g('SA')/g('S0'),
           tz=g('t_zstd_dec_ms'),t_base=g('t_base_ms'),d_o1=g('t_o1_ms')-g('t_base_ms'),d_pos=(g('t_pos_ms')-g('t_base_ms')) if g('POS') else None,d_sub=g('t_sub_ms')-g('t_base_ms'),
           nlit=int(g('nlit')),binrate=g('bin_dec_MBps_lit'),chk=r['chk'])
    rows.append(d)
json.dump(rows,open('/tmp/cmp/exp1_rows.json','w'),indent=1)
def show(sel,title):
    X=[d for d in rows if sel(d)]; S=lambda k: sum((d[k] if d[k] is not None else d['zstd3']) for d in X)
    A=S('zstd3'); print(f"\n== {title}: {len(X)} files, {sum(d['n'] for d in X)/1e6:.0f} MB; zstd -3 = {A/1e6:.2f} MB; kolmo balanced = {S('kolmo_bal')/1e6:.2f} MB ({100*(S('kolmo_bal')/A-1):+.1f}%)")
    print(f"   literal sections are {100*sum(d['lit_share']*d['zstd3'] for d in X)/A:.0f}% of zstd's output")
    for k,lab in (('o1k8','prev-byte classes, 8 Huffman streams'),('o1k16','prev-byte classes, 16 streams'),('pos','position in record (where a period is found)'),('col','field number (text)'),('sub','minus byte at last offset, per block'),('best_static','best static literal scheme per file'),('both_static','best static literals + context-split sequences'),('bino1','adaptive binary, order 1'),('binmix','adaptive binary mix (upper bound)'),('both_adapt','adaptive mix literals + adaptive sequences')):
        print(f"   {lab:52s} {S(k)/1e6:8.2f} MB  {100*(S(k)/A-1):+6.2f}% vs zstd -3")
    tz=sum(d['tz'] for d in X); print(f"   decode: zstd {tz:.0f} ms; added by prev-byte gather {sum(d['d_o1'] for d in X):+.0f} ms ({100*sum(d['d_o1'] for d in X)/tz:+.0f}%); by sub {sum(d['d_sub'] for d in X):+.0f} ms ({100*sum(d['d_sub'] for d in X)/tz:+.0f}%); binary decoder {sum(d['nlit'] for d in X)/1e6/ (sum(d['nlit']/d['binrate'] for d in X)/1e6):.0f} MB/s of literals = {sum(d['nlit']/1e6/d['binrate'] for d in X)*1e3:+.0f} ms ({100*sum(d['nlit']/1e6/d['binrate'] for d in X)*1e3/tz:+.0f}%)")
show(lambda d:d['set']=='silesia','Silesia'); show(lambda d:d['set']!='silesia','Structured set'); show(lambda d:True,'All')
print("\nper file: zstd3, kolmo balanced, best static literals, +ctx sequences, adaptive mix both  (as % of zstd -3)")
for d in rows: print(f"  {d['file'][:26]:26s} {d['zstd3']/1e6:7.2f} MB  kolmo {100*d['kolmo_bal']/d['zstd3']:6.1f}  static {100*d['best_static']/d['zstd3']:6.1f}  both-static {100*d['both_static']/d['zstd3']:6.1f}  mix {100*d['both_adapt']/d['zstd3']:6.1f}   lit share {100*d['lit_share']:3.0f}%  P={d['P']}  chk={d['chk']}")
