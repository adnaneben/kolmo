/* tunable settings (defaults are the hand-tuned values; tune/ga.py searches them with -D overrides) */
#ifndef CZ_TBL_GAIN_PCT
#define CZ_TBL_GAIN_PCT 97      /* keep the table transform if the size probe (kprobe_size) is below this % of plain */
#endif
#ifndef CZ_LOG_GAIN_PCT
#define CZ_LOG_GAIN_PCT 92
#endif
#ifndef CZ_WORD_GAIN_PCT
#define CZ_WORD_GAIN_PCT 99
#endif
#ifndef CZ_STRUCT_GAIN_PCT
#define CZ_STRUCT_GAIN_PCT 97
#endif
#ifndef KOLMO_STRONG_SHUF_FLOAT_PCT
#define KOLMO_STRONG_SHUF_FLOAT_PCT 102   /* max and ultra: a shuffle whose float rival scored within this % of it is coded both ways (planner.c) */
#endif
#ifndef KOLMO_ECO_TEXT_BWT
#define KOLMO_ECO_TEXT_BWT 0   /* owner option A6(a): eco keeps its LZ coder (R_ECO: KLZH2 level 7) for text and JSON token streams (1 = W4 docs rule: block sorting) */
#endif
#ifndef KOLMO_BAL_WIDE
#define KOLMO_BAL_WIDE 1      /* balanced, plain coding of a large segment: blocks of up to KOLMO_BAL_WIDE_BLOCK, split evenly
                               * (core/compress.c). Kept from the zstd 8 MiB window path: KLZH2's window covers the block */
#endif
#define KOLMO_BAL_WIDE_MIN ((size_t)4 << 20)
#define KOLMO_BAL_WIDE_BLOCK ((uint64_t)6 << 20)
#ifndef CZ_ECO_LEVEL
#define CZ_ECO_LEVEL 8
#endif
#ifndef CZ_FAST_LEVEL
#define CZ_FAST_LEVEL 3
#endif
