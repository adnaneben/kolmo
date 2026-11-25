"""Create the bounded, emitter-neutral reciprocal trial from the named base source."""
from pathlib import Path
import argparse
p=argparse.ArgumentParser(description=__doc__)
p.add_argument("src",type=Path)
p.add_argument("out",type=Path)
p.add_argument("--counters",action="store_true")
p.add_argument("--reference",action="store_true")
a=p.parse_args();s=a.src.read_text()
if not a.reference:
    s=s.replace("kolmo_cp *cp;                        /* the pipeline", "uint64_t reciprocal;                 /* ceil(2^32 / K) for a 20-bit bucket */\n    kolmo_cp *cp;                        /* the pipeline")
    pos=s.index("/* chunk j for the classes in mask")
    s=s[:pos]+'''/* KO_LV uses <=20 hash bits, and K<=16. Rounding the reciprocal up
 * adds strictly less than 2^-12 to h/K, below the nearest possible
 * positive gap 1/K. The product therefore gives exactly floor(h/K). */
static inline unsigned ko_bucket_class(const ko_mt *Q, uint32_t h) {
    return h - (uint32_t)((h * Q->reciprocal) >> 32) * (uint32_t)Q->K;
}
'''+s[pos:]
    s=s.replace("KO_H4(M->in + p, M->hlog) % K;", "ko_bucket_class(Q, KO_H4(M->in + p, M->hlog));")
    s=s.replace("KO_H4(M->in + p, M->hlog) % (uint32_t)Q->K,", "ko_bucket_class(Q, KO_H4(M->in + p, M->hlog)),")
    s=s.replace("Q->cp = kolmo_cp_start(K,", "Q->reciprocal = ((UINT64_C(1) << 32) + (unsigned)K - 1) / (unsigned)K;\n    Q->cp = kolmo_cp_start(K,")
if a.counters:
    s="#include <stdatomic.h>\n#include <stdio.h>\n"+s
    s=s.replace("ko_ar ar[KO_MTMAXK][KO_MTR];","_Atomic uint64_t worker_hashes; uint64_t parse_hashes;\n    ko_ar ar[KO_MTMAXK][KO_MTR];")
    s=s.replace("for (uint32_t p = c0; p < c1; p++) {", "atomic_fetch_add_explicit(&Q->worker_hashes, c1-c0, memory_order_relaxed);\n    for (uint32_t p = c0; p < c1; p++) {")
    s=s.replace("kolmo_cp_end(Q->cp);","kolmo_cp_end(Q->cp);\n    fprintf(stderr, \"B36 %d %llu %llu\\n\", Q->K, (unsigned long long)atomic_load(&Q->worker_hashes), (unsigned long long)Q->parse_hashes);")
    s=s.replace("ko_mt *Q = M->mt;\n", "ko_mt *Q = M->mt; Q->parse_hashes++;\n")
a.out.write_text(s)
