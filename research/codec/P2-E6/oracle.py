import re,sys
pat=re.compile(rb'-?(0|[1-9][0-9]*)(\.[0-9]+)?$')
def kinds(b):
    if b.count(b'\n')<1: return 'opaque'
    hl=b.index(b'\n')+1; body=b[hl:]
    if not body.endswith(b'\n') or any(c in body for c in (b'"',b'\r',b'\0')): return 'opaque'
    lines=body.split(b'\n')[:-1]
    if len(lines)<16: return 'opaque'
    k=lines[0].count(b',')+1
    if any(l.count(b',')!=k-1 for l in lines): return 'opaque'
    out=''
    for c in zip(*[l.split(b',') for l in lines]):
        sc=set()
        for x in c:
            m=pat.match(x)
            if not m: sc=None; break
            digits=x.replace(b'.',b'').lstrip(b'-')
            v=int(x.replace(b'.',b''))
            if x.startswith(b'-') and v==0: sc=None; break
            if not (-2**63<=v<2**63) or len(digits)>19 or (m.group(2) and len(m.group(2))-1>18): sc=None; break
            sc.add(len(m.group(2))-1 if m.group(2) else 0)
        out+='T' if sc is None or len(sc)!=1 else ('I' if sc=={0} else 'D%d'%sc.pop())
    return out
print(kinds(open(sys.argv[1],'rb').read()))
