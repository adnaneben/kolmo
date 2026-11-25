"""Exercise ordinary repeated requests, distinct missing counts and the 16-argument boundary."""
import os, subprocess,sys,tempfile
from pathlib import Path
bin=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as td:
    root=Path(td); src=root/'input'; src.mkdir()
    # The default macOS filesystem is normally case- and normalization-insensitive.
    names=['-dash','two words','001','alpha','zeta','é']+['file-%02d'%i for i in range(24)]
    for i,n in enumerate(names): (src/n).write_bytes(('ordinary member %d'%i).encode())
    arc=root/'a.kol'
    # The first operand is an output pathname; subsequent file operands starting with '-' are ordinary names.
    subprocess.run([bin,'archive','-1',arc,*names,names[3],names[3]],cwd=src,check=True)
    for nn in (1,2,15,16,17,26):
        fresh=names[:nn]
        for requested,missing in [(fresh,0),([*fresh,fresh[-1]],0),([*fresh,'unknown','unknown'],2),([*fresh,'absent','elsewhere'],2)]:
            with tempfile.TemporaryDirectory(dir=root) as dst:
                proc=subprocess.run([bin,'extract','-C',dst,arc,*reversed(requested)],capture_output=True)
                if proc.returncode!=bool(missing): raise AssertionError((requested,proc.returncode,proc.stderr))
                if missing: assert f'{missing} of {len(requested)} names'.encode() in proc.stderr
                paths={p.name:p.read_bytes() for p in Path(dst).iterdir()}
                assert paths=={n:(src/n).read_bytes() for n in fresh}
    print('24 ordinary selection cases passed; exact files, duplicate requests, repeated archive entries, missing counts and stack/heap boundary verified')
