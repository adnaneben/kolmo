/* tunable settings (defaults are the hand-tuned values; tune/ga.py searches them with -D overrides) */
#ifndef CZ_TBL_GAIN_PCT
#define CZ_TBL_GAIN_PCT 97      /* keep the table transform if the zstd -1 trial is below this % of plain */
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
#ifndef CZ_HC_TEXT_LEVEL
#define CZ_HC_TEXT_LEVEL 22
#endif
#ifndef CZ_ECO_LEVEL
#define CZ_ECO_LEVEL 8
#endif
#ifndef CZ_FAST_LEVEL
#define CZ_FAST_LEVEL 3
#endif
