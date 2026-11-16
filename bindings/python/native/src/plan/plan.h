/* The plan of a block: transform and its parameters, chosen by plan/planner.c. */
typedef struct { int t, a, b, c; fset_t *fs; img_plan_t img; rec_plan_t *rec; int wide; int experimental_dna, experimental_table, experimental_pcm; uint32_t kgw; int verify;   /* kgw: IMG8 or DEC64 row bytes; verify: code graph blocks plainly too, keep the smaller */
    int cmp; uint8_t *cmpz; size_t cmpzn, cmpn;   /* ultra: table columns under codec 25 (cmpz: the coded block, when the planner's sample was the whole block) */
#ifdef KOLMO_FRONTIER
    frontier_lags lags;
#endif
} plan_t;   /* tp/tbl: a=delim b=quotes c=k-1 | shuf: a=w b=ch c=delta; fs: formulas (encoder) */
