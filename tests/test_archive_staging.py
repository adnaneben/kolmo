#!/usr/bin/env python3
"""Public CLI tests with valid ordinary archives and real files."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

exe=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as d:
    root=Path(d)
    for case in ('name', 'nested', 'alternate-case', 'mixed-preexisting', 'select', 'bad-leaf'):
        source=root/(case+'-source'); dest=root/(case+'-dest'); archive=root/(case+'.kol')
        source.mkdir();dest.mkdir()
        # Keep the child's PID across exec so the test can use a valid member whose
        # ordinary stored name happens to equal this invocation's first stage name.
        code='import os,sys; sys.stdin.readline(); os.execv(sys.argv[1],sys.argv[1:])'
        args=[str(exe),'extract','-T','4','-C',str(dest),str(archive)]
        if case=='select': args+=['team/a','team/b']
        proc=subprocess.Popen([sys.executable,'-c',code,*args],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
        alias=f'.kolmo-{proc.pid}-1'
        if case=='name': names=['first','second',alias,'last']
        elif case=='nested': names=['first','second',alias+'/one',alias+'/two','last']
        elif case=='alternate-case': names=['first','second',alias.upper(),'last']
        elif case=='mixed-preexisting': names=['first','a/one','a/two','b/three','a/four','.user','tail']
        elif case=='select': names=['team/a','not-selected','team/b']
        else: names=['first','conflict','last'];(dest/'conflict').mkdir();(dest/'conflict'/'old').write_bytes(b'kept')
        for i,name in enumerate(names):
            p=source/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(bytes([i])*i);p.chmod(0o640+i)
            os.utime(p,(1600000000+i,1600000000+i))
        if case=='mixed-preexisting':
            (dest/'first').write_bytes(b'previous');os.link(dest/'first',root/'retained')
        subprocess.run([str(exe),'archive','-1','-T','1',str(archive),*names],cwd=source,check=True)
        stdout,stderr=proc.communicate(b'go\n')
        assert proc.returncode == (1 if case=='bad-leaf' else 0),(case,stderr)
        expected={n for n in names if (case!='select' or n!='not-selected') and (case!='bad-leaf' or n!='conflict')}
        real={str(p.relative_to(dest)) for p in dest.rglob('*') if p.is_file()}
        assert real==expected|({'conflict/old'} if case=='bad-leaf' else set()),(case,real)
        expected_dirs={str(p) for name in expected for p in Path(name).parents if str(p)!='.'}
        if case=='bad-leaf': expected_dirs.add('conflict')
        assert {str(p.relative_to(dest)) for p in dest.rglob('*') if p.is_dir()}==expected_dirs
        for name in expected:
            a,b=source/name,dest/name
            assert a.read_bytes()==b.read_bytes() and (a.stat().st_mode & 0o777)==(b.stat().st_mode & 0o777)
            assert a.stat().st_mtime_ns==b.stat().st_mtime_ns
        if case=='bad-leaf': assert (dest/'conflict'/'old').read_bytes()==b'kept'
        if case=='mixed-preexisting': assert (root/'retained').read_bytes()==b'previous'
        print(case,'passed')
# A valid large third sibling hits an OS file-size limit inside an already reused stage.
# The old member remains visible and the private stage is empty and removed.
import resource
import signal
with tempfile.TemporaryDirectory() as d:
    root=Path(d);source=root/'source';dest=root/'dest';source.mkdir();dest.mkdir()
    for name,size in [('a',1),('b',17),('large',16384)]:
        (source/name).write_bytes(b'w'*size)
    (dest/'large').write_bytes(b'prior member')
    subprocess.run([str(exe),'archive','-1',str(root/'arc'),'a','b','large'],cwd=source,check=True)
    def limit():
        signal.signal(signal.SIGXFSZ,signal.SIG_IGN)
        resource.setrlimit(resource.RLIMIT_FSIZE,(4096,4096))
    r=subprocess.run([exe,'extract','-C',dest,root/'arc'],capture_output=True,preexec_fn=limit)
    assert r.returncode==1 and b'large' in r.stderr
    assert {p.name for p in dest.iterdir()}=={'a','b','large'}
    assert (dest/'large').read_bytes()==b'prior member'
    assert (dest/'a').read_bytes()==b'w' and (dest/'b').read_bytes()==b'w'*17
    print('cached-write-limit passed')
