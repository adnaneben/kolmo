import random,subprocess,sys,os
R=random.Random(int(sys.argv[2])); src=open(sys.argv[1],'rb').read(); n=int(sys.argv[3]); crash=ok=rej=0
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:exitcode=99',UBSAN_OPTIONS='halt_on_error=1:exitcode=99')
for i in range(n):
    b=bytearray(src); k=R.choice([1,1,2,4,8])
    for _ in range(k):
        j=R.randrange(32,len(b)); m=R.randrange(3)
        if m==0: b[j]^=1<<R.randrange(8)
        elif m==1: b[j]=R.randrange(256)
        else: b[j]=R.choice([0,255,0x80,0x7f])
    open('/tmp/wr/m.czn','wb').write(b)
    r=subprocess.run(['/tmp/wr/kolmo-asan','-d','-T1','/tmp/wr/m.czn','/tmp/wr/m.out'],env=env,capture_output=True,timeout=30)
    if r.returncode==99 or b'Sanitizer' in r.stderr or b'runtime error' in r.stderr: crash+=1; open(f'/tmp/wr/crash{i}.czn','wb').write(b); print(r.stderr[-400:].decode(errors='replace'))
    elif r.returncode==0: ok+=1
    else: rej+=1
print(sys.argv[1],'mutants',n,'crash',crash,'decoded',ok,'rejected',rej)
