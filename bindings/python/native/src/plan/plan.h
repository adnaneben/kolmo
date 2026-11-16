/* The plan of a block: transform and its parameters, chosen by plan/planner.c. */
typedef struct { int t, a, b, c; fset_t *fs; img_plan_t img; rec_plan_t *rec; int wide; int experimental_dna, experimental_table, experimental_pcm; uint32_t kgw; int verify; int lines;   /* lines: blocks end at line ends (KOLMO_T_COLUMNS); kgw: IMG8 or DEC64 row bytes; verify: code graph blocks plainly too, keep the smaller */
#ifdef KOLMO_FRONTIER
    frontier_lags lags;
#endif
} plan_t;   /* tp/tbl: a=delim b=quotes c=k-1 | shuf: a=w b=ch c=delta; fs: formulas (encoder) */
