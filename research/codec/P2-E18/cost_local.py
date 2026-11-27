import subprocess,time,os,sys,statistics,glob
Z='/tmp/ozl/build/cli/zli'; K='/home/claude/kolmo/build/kolmo'; G='/tmp/gx/gf'
def t(cmd):
    best=[]
    for _ in range(5):
        t0=time.perf_counter(); subprocess.run(cmd,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL); best.append(time.perf_counter()-t0)
    return statistics.median(best)
def cost(e,d,b): return (e+3.3*d)*3.89/72/3600 + b/1e9*0.023
for f in sorted(glob.glob('/tmp/e18/ghcn/*.npy')):
    rows=[]
    pol={'ozl9':([Z,'compress','-p','le-i32','-l','9','-f','-o','/tmp/c.x',f],[Z,'decompress','-f','-o','/tmp/c.y','/tmp/c.x']),
         'cm_p0':([G,'gf','cm','0',f,'/tmp/c.x'],[K,'-d','-T1','/tmp/c.x','/tmp/c.y']),
         'cm_p1':([G,'gf','cm','1',f,'/tmp/c.x'],[K,'-d','-T1','/tmp/c.x','/tmp/c.y']),
         'bsc_p0':([G,'gf','bsc','0',f,'/tmp/c.x'],[K,'-d','-T1','/tmp/c.x','/tmp/c.y']),
         'kfast':([K,'-m','fast','-T1',f,'/tmp/c.x'],[K,'-d','-T1','/tmp/c.x','/tmp/c.y'])}
    out=[]
    for k,(e,d) in pol.items():
        if os.path.exists('/tmp/c.y'): os.remove('/tmp/c.y')
        te=t(e); b=os.path.getsize('/tmp/c.x'); td=t(d)
        assert open('/tmp/c.y','rb').read()==open(f,'rb').read(), k
        out.append((k,b,te*1e3,td*1e3,cost(te,td,b)*1e9))
    o=out[0]
    print(os.path.basename(f)[11:30],' '.join('%s %d B %.1f/%.1f ms %.0f n$%s'%(k,b,e,d,c,'*' if c<=o[4] else '') for k,b,e,d,c in out))
