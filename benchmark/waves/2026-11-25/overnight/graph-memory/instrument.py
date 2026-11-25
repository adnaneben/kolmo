"""Apply audit hooks to an isolated, digest-pinned copy of graph.c."""
from pathlib import Path
import hashlib,sys
f=Path(sys.argv[1])
s=f.read_text()
if hashlib.sha256(f.read_bytes()).hexdigest()!='a001d415506a616ca95948c78d1745c61ba3bc386eeb2fa502a6d3e3e11ea4a9':
    raise SystemExit('expected pinned, unmodified graph.c in a disposable checkout')
def replace(a,b):
    global s
    if s.count(a)!=1: raise SystemExit('unexpected source shape')
    s=s.replace(a,b)
a='typedef struct { uint8_t *p; size_t n, cap; } kbuf;'
replace(a,a+'\n#include "probe.h"')
a='    const uint64_t maxs = (uint64_t)len * 4 + (1u << 20);'
replace(a,'    tr_check(!tr.active); memset(&tr,0,sizeof tr); tr.active=1; tr.phase=LEAF;\n'+a)
a='    if (i != n) goto out;\n    for (uint64_t q = 0; q < ns; q++)'
replace(a,'    tr.phase=INV;\n'+a)
a='        kgnode *t = &nd[qq]; kbuf *o = &s[t->firstout];'
replace(a,a+' tr.ops[t->op]++;')
a='            kbuf r = {0}; if (kg_fields_inv'
replace(a,'''            size_t cursor_bytes=0;
            for (uint64_t q=0;q<k;q++) {
                kgdec *D=dec[t->firstout+1+q];
                if (D) cursor_bytes+=(size_t)(D->tge-D->tg)+(size_t)(D->sce-D->sc)+(size_t)(D->mte-D->mt)+(size_t)(D->txe-D->tx);
            }
            tr_check(cursor_bytes<=tr.fused_live);
'''+a)
a='            s[t->in[0]] = r;\n        } else if (t->op == KG_DECIMAL)'
replace(a,'            tr.fused_live-=cursor_bytes;\n'+a)
a='                dec[t->in[0]] = D; have[t->in[0]] = 1; continue;'
replace(a,'                tr_fuse(o); '+a.strip())
a='            continue;                                               /* the concatenation stays alive for its views */'
replace(a,'            tr_views(s,ns);\n'+a)
a='    int rc = -1; uint64_t xb = 0, q = 0, skips = 0, vb = 0, db = 0;'
replace(a,'    if (DV && tr.active && dc->n>tr.dict_peak) tr.dict_peak=dc->n;\n'+a)
a='    free(s); free(have); free(cons); free(nd); free(ins); free(prod); free(dec); free(decs);'
replace(a,a+'\n    tr_done(n,len,rc);')
s+='\n#undef malloc\n#undef calloc\n#undef realloc\n#undef free\n'
f.write_text(s)
