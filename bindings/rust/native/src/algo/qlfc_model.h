/*-----------------------------------------------------------*/
/* QLFC statistical models, ported to C from libbsc 3.4.0     */
/*-----------------------------------------------------------*/
/*
   Copyright (c) 2009-2026 Ilya Grebnov <ilya.grebnov@gmail.com>  (libbsc, coder/qlfc/qlfc_model.h,
   coder/common/predictor.h)
   C port: Copyright 2026 Adnane Aqartit and contributors.

   Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with
   the License. You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
   Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
   an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
   specific language governing permissions and limitations under the License.

   The constants are libbsc's, unchanged. M_* drive the adaptive coder (with mixers), F_* the static coder.
*/
#ifndef KOLMO_ALGO_QLFC_MODEL_H
#define KOLMO_ALGO_QLFC_MODEL_H
#include <stdint.h>
#include "qlfc_tables.h"

enum {
    M_RANK_TS_TH0 = 1,     M_RANK_TS_AR0 = 57,
    M_RANK_TS_TH1 = -111,     M_RANK_TS_AR1 = 31,
    M_RANK_TC_TH0 = 291,     M_RANK_TC_AR0 = 250,
    M_RANK_TC_TH1 = 154,     M_RANK_TC_AR1 = 528,
    M_RANK_TP_TH0 = 375,     M_RANK_TP_AR0 = 163,
    M_RANK_TP_TH1 = 313,     M_RANK_TP_AR1 = 639,
    M_RANK_TM_TH0 = -41,     M_RANK_TM_AR0 = 96,
    M_RANK_TM_TH1 = 53,     M_RANK_TM_AR1 = 49,
    M_RANK_TM_LR0 = 20,     M_RANK_TM_LR1 = 47,
    M_RANK_TM_LR2 = 27,
    M_RANK_ES_TH0 = -137,     M_RANK_ES_AR0 = 17,
    M_RANK_ES_TH1 = 482,     M_RANK_ES_AR1 = 40,
    M_RANK_EC_TH0 = 61,     M_RANK_EC_AR0 = 192,
    M_RANK_EC_TH1 = 200,     M_RANK_EC_AR1 = 133,
    M_RANK_EP_TH0 = 54,     M_RANK_EP_AR0 = 1342,
    M_RANK_EP_TH1 = 578,     M_RANK_EP_AR1 = 1067,
    M_RANK_EM_TH0 = -11,     M_RANK_EM_AR0 = 318,
    M_RANK_EM_TH1 = 144,     M_RANK_EM_AR1 = 848,
    M_RANK_EM_LR0 = 49,     M_RANK_EM_LR1 = 41,
    M_RANK_EM_LR2 = 40,
    M_RANK_MS_TH0 = -145,     M_RANK_MS_AR0 = 18,
    M_RANK_MS_TH1 = 114,     M_RANK_MS_AR1 = 24,
    M_RANK_MC_TH0 = -43,     M_RANK_MC_AR0 = 69,
    M_RANK_MC_TH1 = -36,     M_RANK_MC_AR1 = 78,
    M_RANK_MP_TH0 = -2,     M_RANK_MP_AR0 = 1119,
    M_RANK_MP_TH1 = 11,     M_RANK_MP_AR1 = 1181,
    M_RANK_MM_TH0 = -203,     M_RANK_MM_AR0 = 20,
    M_RANK_MM_TH1 = -271,     M_RANK_MM_AR1 = 15,
    M_RANK_MM_LR0 = 263,     M_RANK_MM_LR1 = 175,
    M_RANK_MM_LR2 = 17,
    M_RANK_PS_TH0 = -99,     M_RANK_PS_AR0 = 32,
    M_RANK_PS_TH1 = 318,     M_RANK_PS_AR1 = 42,
    M_RANK_PC_TH0 = 17,     M_RANK_PC_AR0 = 101,
    M_RANK_PC_TH1 = 1116,     M_RANK_PC_AR1 = 246,
    M_RANK_PP_TH0 = 22,     M_RANK_PP_AR0 = 964,
    M_RANK_PP_TH1 = -2,     M_RANK_PP_AR1 = 1110,
    M_RANK_PM_TH0 = -194,     M_RANK_PM_AR0 = 21,
    M_RANK_PM_TH1 = -129,     M_RANK_PM_AR1 = 20,
    M_RANK_PM_LR0 = 480,     M_RANK_PM_LR1 = 202,
    M_RANK_PM_LR2 = 17,
    M_RUN_TS_TH0 = -93,     M_RUN_TS_AR0 = 34,
    M_RUN_TS_TH1 = -4,     M_RUN_TS_AR1 = 51,
    M_RUN_TC_TH0 = 139,     M_RUN_TC_AR0 = 423,
    M_RUN_TC_TH1 = 244,     M_RUN_TC_AR1 = 162,
    M_RUN_TP_TH0 = 275,     M_RUN_TP_AR0 = 450,
    M_RUN_TP_TH1 = -6,     M_RUN_TP_AR1 = 579,
    M_RUN_TM_TH0 = -68,     M_RUN_TM_AR0 = 25,
    M_RUN_TM_TH1 = 1,     M_RUN_TM_AR1 = 64,
    M_RUN_TM_LR0 = 15,     M_RUN_TM_LR1 = 50,
    M_RUN_TM_LR2 = 78,
    M_RUN_ES_TH0 = -116,     M_RUN_ES_AR0 = 31,
    M_RUN_ES_TH1 = 43,     M_RUN_ES_AR1 = 45,
    M_RUN_EC_TH0 = 165,     M_RUN_EC_AR0 = 222,
    M_RUN_EC_TH1 = 30,     M_RUN_EC_AR1 = 324,
    M_RUN_EP_TH0 = 315,     M_RUN_EP_AR0 = 857,
    M_RUN_EP_TH1 = 109,     M_RUN_EP_AR1 = 867,
    M_RUN_EM_TH0 = -14,     M_RUN_EM_AR0 = 215,
    M_RUN_EM_TH1 = 61,     M_RUN_EM_AR1 = 73,
    M_RUN_EM_LR0 = 35,     M_RUN_EM_LR1 = 37,
    M_RUN_EM_LR2 = 42,
    M_RUN_MS_TH0 = -176,     M_RUN_MS_AR0 = 14,
    M_RUN_MS_TH1 = -141,     M_RUN_MS_AR1 = 21,
    M_RUN_MC_TH0 = 84,     M_RUN_MC_AR0 = 172,
    M_RUN_MC_TH1 = 37,     M_RUN_MC_AR1 = 263,
    M_RUN_MP_TH0 = 2,     M_RUN_MP_AR0 = 15,
    M_RUN_MP_TH1 = -197,     M_RUN_MP_AR1 = 20,
    M_RUN_MM_TH0 = -27,     M_RUN_MM_AR0 = 142,
    M_RUN_MM_TH1 = -146,     M_RUN_MM_AR1 = 27,
    M_RUN_MM_LR0 = 51,     M_RUN_MM_LR1 = 44,
    M_RUN_MM_LR2 = 80,
    F_RANK_TS_TH0 = -116,     F_RANK_TS_AR0 = 33,
    F_RANK_TS_TH1 = -78,     F_RANK_TS_AR1 = 34,
    F_RANK_TC_TH0 = -2,     F_RANK_TC_AR0 = 282,
    F_RANK_TC_TH1 = 12,     F_RANK_TC_AR1 = 274,
    F_RANK_TP_TH0 = 4,     F_RANK_TP_AR0 = 697,
    F_RANK_TP_TH1 = 55,     F_RANK_TP_AR1 = 1185,
    F_RANK_TM_LR0 = 17,     F_RANK_TM_LR1 = 14,
    F_RANK_TM_LR2 = 1,
    F_RANK_ES_TH0 = -177,     F_RANK_ES_AR0 = 23,
    F_RANK_ES_TH1 = -370,     F_RANK_ES_AR1 = 11,
    F_RANK_EC_TH0 = -14,     F_RANK_EC_AR0 = 271,
    F_RANK_EC_TH1 = 3,     F_RANK_EC_AR1 = 308,
    F_RANK_EP_TH0 = -3,     F_RANK_EP_AR0 = 788,
    F_RANK_EP_TH1 = 135,     F_RANK_EP_AR1 = 1364,
    F_RANK_EM_LR0 = 22,     F_RANK_EM_LR1 = 6,
    F_RANK_EM_LR2 = 4,
    F_RANK_MS_TH0 = -254,     F_RANK_MS_AR0 = 16,
    F_RANK_MS_TH1 = -177,     F_RANK_MS_AR1 = 20,
    F_RANK_MC_TH0 = -55,     F_RANK_MC_AR0 = 73,
    F_RANK_MC_TH1 = -54,     F_RANK_MC_AR1 = 74,
    F_RANK_MP_TH0 = -6,     F_RANK_MP_AR0 = 575,
    F_RANK_MP_TH1 = 1670,     F_RANK_MP_AR1 = 1173,
    F_RANK_MM_LR0 = 15,     F_RANK_MM_LR1 = 10,
    F_RANK_MM_LR2 = 7,
    F_RANK_PS_TH0 = -126,     F_RANK_PS_AR0 = 32,
    F_RANK_PS_TH1 = -126,     F_RANK_PS_AR1 = 32,
    F_RANK_PC_TH0 = -33,     F_RANK_PC_AR0 = 120,
    F_RANK_PC_TH1 = -25,     F_RANK_PC_AR1 = 157,
    F_RANK_PP_TH0 = -6,     F_RANK_PP_AR0 = 585,
    F_RANK_PP_TH1 = 150,     F_RANK_PP_AR1 = 275,
    F_RANK_PM_LR0 = 16,     F_RANK_PM_LR1 = 11,
    F_RANK_PM_LR2 = 5,
    F_RUN_TS_TH0 = -68,     F_RUN_TS_AR0 = 38,
    F_RUN_TS_TH1 = -112,     F_RUN_TS_AR1 = 36,
    F_RUN_TC_TH0 = -4,     F_RUN_TC_AR0 = 221,
    F_RUN_TC_TH1 = -13,     F_RUN_TC_AR1 = 231,
    F_RUN_TP_TH0 = 0,     F_RUN_TP_AR0 = 0,
    F_RUN_TP_TH1 = 0,     F_RUN_TP_AR1 = 0,
    F_RUN_TM_LR0 = 14,     F_RUN_TM_LR1 = 18,
    F_RUN_TM_LR2 = 0,
    F_RUN_ES_TH0 = -90,     F_RUN_ES_AR0 = 45,
    F_RUN_ES_TH1 = -92,     F_RUN_ES_AR1 = 44,
    F_RUN_EC_TH0 = -3,     F_RUN_EC_AR0 = 325,
    F_RUN_EC_TH1 = -11,     F_RUN_EC_AR1 = 341,
    F_RUN_EP_TH0 = 24,     F_RUN_EP_AR0 = 887,
    F_RUN_EP_TH1 = -4,     F_RUN_EP_AR1 = 765,
    F_RUN_EM_LR0 = 14,     F_RUN_EM_LR1 = 15,
    F_RUN_EM_LR2 = 3,
    F_RUN_MS_TH0 = -275,     F_RUN_MS_AR0 = 14,
    F_RUN_MS_TH1 = -185,     F_RUN_MS_AR1 = 22,
    F_RUN_MC_TH0 = -18,     F_RUN_MC_AR0 = 191,
    F_RUN_MC_TH1 = -15,     F_RUN_MC_AR1 = 241,
    F_RUN_MP_TH0 = -73,     F_RUN_MP_AR0 = 54,
    F_RUN_MP_TH1 = -214,     F_RUN_MP_AR1 = 19,
    F_RUN_MM_LR0 = 7,     F_RUN_MM_LR1 = 15,
    F_RUN_MM_LR2 = 10,
};

/* Probability counters: 12-bit probability of a 0 bit. */
static inline void qc_update_bit0(int16_t *p, int threshold, int rate) { *p = (int16_t)(*p + (((4096 - threshold - *p) * rate) >> 12)); }
static inline void qc_update_bit1(int16_t *p, int threshold, int rate) { *p = (int16_t)(*p - (((*p - threshold) * rate) >> 12)); }
static inline void qc_update_bit(unsigned bit, int16_t *p, int th0, int ar0, int th1, int ar1) {
    int delta0 = *p * ar0 - ((4096 - th0) * ar0 - 4095);
    int delta1 = *p * ar1 - (th1 * ar1);
    *p = (int16_t)(*p - ((bit ? delta1 : delta0) >> 12));
}

/* Three-input logistic mixer with an interpolated APM (libbsc's ProbabilityMixer). The products are computed in
   32-bit unsigned arithmetic and converted back, which is the wrap-around the C++ build performs on x86-64 and
   AArch64 (where the int products can overflow on extreme inputs), without signed-overflow undefined behaviour. */
typedef struct {
    int16_t s0, s1, s2;
    int mixed, index;
    int16_t map[17];
    int w0, w1, w2;
} qlfc_mixer;

static inline int qm_mul3(int a, int b, int c) { return (int)((uint32_t)a * (uint32_t)b * (uint32_t)c); }
static inline int qm_dot(int s0, int w0, int s1, int w1, int s2, int w2) {
    return (int)((uint32_t)s0 * (uint32_t)w0 + (uint32_t)s1 * (uint32_t)w1 + (uint32_t)s2 * (uint32_t)w2);
}
static inline void qm_init(qlfc_mixer *m) {
    m->w0 = m->w1 = 2048 << 5; m->w2 = 0; m->s0 = m->s1 = m->s2 = 0; m->mixed = 0; m->index = 0;
    for (int p = 0; p < 17; ++p) m->map[p] = (int16_t)bsc_squash((p - 8) * 256);
}
static inline int qm_mixup(qlfc_mixer *m, int p0, int p1, int p2) {
    m->s0 = (int16_t)bsc_stretch(p0); m->s1 = (int16_t)bsc_stretch(p1); m->s2 = (int16_t)bsc_stretch(p2);
    int16_t sp = (int16_t)(qm_dot(m->s0, m->w0, m->s1, m->w1, m->s2, m->w2) >> 17);
    if (sp < -2047) sp = -2047;
    if (sp > 2047) sp = 2047;
    m->index = (sp + 2048) >> 8;
    const int weight = sp & 255, probability = bsc_squash(sp);
    const int mapped = m->map[m->index] + (((m->map[m->index + 1] - m->map[m->index]) * weight) >> 8);
    return m->mixed = (3 * probability + mapped) >> 2;
}
static inline void qm_learn(qlfc_mixer *m, int eps, int lr0, int lr1, int lr2) {
    m->w0 -= qm_mul3(lr0, eps, m->s0) >> 16;
    m->w1 -= qm_mul3(lr1, eps, m->s1) >> 16;
    m->w2 -= qm_mul3(lr2, eps, m->s2) >> 16;
}
static inline void qm_update0(qlfc_mixer *m, int lr0, int lr1, int lr2, int th, int ar) {
    qc_update_bit0(&m->map[m->index], th, ar); qc_update_bit0(&m->map[m->index + 1], th, ar);
    qm_learn(m, m->mixed - 4095, lr0, lr1, lr2);
}
static inline void qm_update1(qlfc_mixer *m, int lr0, int lr1, int lr2, int th, int ar) {
    qc_update_bit1(&m->map[m->index], th, ar); qc_update_bit1(&m->map[m->index + 1], th, ar);
    qm_learn(m, m->mixed - 1, lr0, lr1, lr2);
}
/* MixupAndUpdateBit0/1 of libbsc: the same arithmetic as mixup followed by update. */
static inline int qm_mix_update0(qlfc_mixer *m, int p0, int p1, int p2, int lr0, int lr1, int lr2, int th, int ar) {
    int r = qm_mixup(m, p0, p1, p2); qm_update0(m, lr0, lr1, lr2, th, ar); return r;
}
static inline int qm_mix_update1(qlfc_mixer *m, int p0, int p1, int p2, int lr0, int lr1, int lr2, int th, int ar) {
    int r = qm_mixup(m, p0, p1, p2); qm_update1(m, lr0, lr1, lr2, th, ar); return r;
}

#define QLFC_ALPHABET 256
typedef struct {
    struct {
        int16_t StaticModel;
        int16_t StateModel[QLFC_ALPHABET];
        int16_t CharModel[QLFC_ALPHABET];
        struct { int16_t StaticModel[8]; int16_t StateModel[QLFC_ALPHABET][8]; int16_t CharModel[QLFC_ALPHABET][8]; } Exponent;
        struct { int16_t StaticModel[128]; int16_t StateModel[QLFC_ALPHABET][128]; int16_t CharModel[QLFC_ALPHABET][128]; } Mantissa[8];
        struct { int16_t StaticModel[QLFC_ALPHABET]; int16_t StateModel[QLFC_ALPHABET][QLFC_ALPHABET]; int16_t CharModel[QLFC_ALPHABET][QLFC_ALPHABET]; } Escape;
    } Rank;
    struct {
        int16_t StaticModel;
        int16_t StateModel[QLFC_ALPHABET];
        int16_t CharModel[QLFC_ALPHABET];
        struct { int16_t StaticModel[32]; int16_t StateModel[QLFC_ALPHABET][32]; int16_t CharModel[QLFC_ALPHABET][32]; } Exponent;
        struct { int16_t StaticModel[32]; int16_t StateModel[QLFC_ALPHABET][32]; int16_t CharModel[QLFC_ALPHABET][32]; } Mantissa[32];
    } Run;
    qlfc_mixer mixerOfRank[QLFC_ALPHABET];
    qlfc_mixer mixerOfRankExponent[8][8];
    qlfc_mixer mixerOfRankMantissa[8];
    qlfc_mixer mixerOfRankEscape[QLFC_ALPHABET];
    qlfc_mixer mixerOfRun[QLFC_ALPHABET];
    qlfc_mixer mixerOfRunExponent[32][32];
    qlfc_mixer mixerOfRunMantissa[32];
} qlfc_model;
#endif
