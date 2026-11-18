/*-----------------------------------------------------------*/
/* Quantized Local Frequency Coding, ported to C from libbsc  */
/*-----------------------------------------------------------*/
/*
   Copyright (c) 2009-2026 Ilya Grebnov <ilya.grebnov@gmail.com>  (libbsc 3.4.0, coder/qlfc/qlfc.cpp, coder/coder.cpp)
   C port and changes: Copyright 2026 Adnane Aqartit and contributors.

   Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with
   the License. You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
   Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on
   an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the
   specific language governing permissions and limitations under the License.

   What Kolmo uses of libbsc's coder: the static QLFC model (codec 5, rc2) and the adaptive model with mixers
   (codec 18). The "fast" model is not ported. Changes from libbsc:
   - C. The rank transform is libbsc's two-symbols-per-step algorithm with a plain C loop over the 256 ranks (the
     compiler vectorises it); the run and rank scans use 8-byte words on every CPU. Same output.
   - Decoders are bounds-checked: the decoded size must fit the caller's buffer, runs must fit what is left, the
     exponent loops are capped at the sizes the model arrays have, the range decoder stops at the end of its input.
     libbsc trusted these. Valid streams decode exactly as before.
   - Coder blocks are encoded and decoded on threads (libbsc did so only with OpenMP); the encoder records the last
     position its overflow check saw, so the serial result (capacity shrinking as blocks are written) is reproduced
     exactly from the parallel one.
*/
#include "qlfc.h"
#include "qlfc_model.h"
#include "rangecoder.h"
#include "bytes.h"
#include "par.h"
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* ---- model initialisation ---- */

static void qlfc_fill(void *dst, size_t size, int16_t v) { int16_t *p = (int16_t *)dst; for (size_t i = 0; i < size / 2; ++i) p[i] = v; }

static void qlfc_init_model(qlfc_model *m, int with_mixers) {
    qlfc_fill(&m->Rank, sizeof(m->Rank), 2048);
    qlfc_fill(&m->Run, sizeof(m->Run), 2048);
    if (!with_mixers) return;
    for (int i = 0; i < QLFC_ALPHABET; ++i) { qm_init(&m->mixerOfRank[i]); qm_init(&m->mixerOfRankEscape[i]); qm_init(&m->mixerOfRun[i]); }
    for (int b = 0; b < 8; ++b) { qm_init(&m->mixerOfRankMantissa[b]); for (int c = 0; c < 8; ++c) qm_init(&m->mixerOfRankExponent[c][b]); }
    for (int b = 0; b < 32; ++b) { qm_init(&m->mixerOfRunMantissa[b]); for (int c = 0; c < 32; ++c) qm_init(&m->mixerOfRunExponent[c][b]); }
}

/* ---- rank transform (encoder) ---- */

/* Index of the last byte before position i that differs from input[i] (input[k+1..i] all equal), or -1. */
static inline ptrdiff_t qlfc_scan_back(const uint8_t *input, ptrdiff_t i, uint8_t c) {
    while (i >= 8) {
        i -= 8;
        uint64_t m = kb_ld64(input + i) ^ kb_ld64(input + i + 1);
        if (m) return i + (63 - __builtin_clzll(m)) / 8;
    }
    do {} while (--i >= 0 && input[i] == c);
    return i;
}

/* Writes the rank of each run (from the end) to buffer[j..n), returns buffer + j; MTFTable receives the initial
   move-to-front order. libbsc's SIMD transform, two runs per step. */
static uint8_t *qlfc_transform(const uint8_t *input, uint8_t *buffer, int n, uint8_t *MTFTable) {
    int8_t ranks[256] __attribute__((aligned(64))), flags[256];
    for (int i = 0; i < 256; ++i) { ranks[i] = (int8_t)(i - 128); flags[i] = 0; }
    ptrdiff_t i = (ptrdiff_t)n - 1, j = n; uint8_t nSymbols = 0;
    for (; i >= 0;) {
        uint8_t c1 = input[i]; i = qlfc_scan_back(input, i, c1); if (i < 0) { i = 0; break; }
        uint8_t c2 = input[i]; i = qlfc_scan_back(input, i, c2);
        int8_t r1 = ranks[c1], r2 = ranks[c2]; r2 = (int8_t)(r2 + (r1 > r2));
        buffer[--j] = (uint8_t)(r1 + 128); if (!flags[c1]) { flags[c1] = 1; buffer[j] = nSymbols++; }
        buffer[--j] = (uint8_t)(r2 + 128); if (!flags[c2]) { flags[c2] = 1; buffer[j] = nSymbols++; }
        for (int t = 0; t < 256; ++t) ranks[t] = (int8_t)(ranks[t] + (r1 > ranks[t]) + (r2 > ranks[t]));
        ranks[c1] = -127; ranks[c2] = -128;
    }
    if (i >= 0) {
        uint8_t c = input[0]; int8_t r = ranks[c];
        buffer[--j] = (uint8_t)(r + 128); if (!flags[c]) { flags[c] = 1; buffer[j] = nSymbols++; }
        for (int t = 0; t < 256; ++t) ranks[t] = (int8_t)(ranks[t] + (ranks[t] < r));
        ranks[c] = -128;
    }
    buffer[n - 1] = 1;
    for (int k = 0; k < 256; ++k) MTFTable[ranks[k] + 128] = (uint8_t)k;
    for (int k = 1; k < 256; ++k) if (!flags[MTFTable[k]]) { MTFTable[k] = MTFTable[k - 1]; break; }
    return buffer + j;
}

/* ---- MTF table header, shared by both models ---- */

static int qlfc_encode_header(kolmo_rc *coder, const uint8_t *MTFTable) {
    uint8_t usedChar[256]; memset(usedChar, 0, sizeof(usedChar));
    int prevChar = -1, maxRank = 7;
    for (int rank = 0; rank < 256; ++rank) {
        int currentChar = MTFTable[rank];
        for (int bit = 7; bit >= 0; --bit) {
            int bit0 = 0, bit1 = 0;
            for (int c = (currentChar >> (bit + 1)) << (bit + 1); c < ((currentChar >> (bit + 1)) + 1) << (bit + 1); ++c)
                if (c == prevChar || usedChar[c] == 0) { if (c & (1 << bit)) bit1 = 1; else bit0 = 1; if (bit0 && bit1) break; }
            if (bit0 && bit1) rc_encode_flat(coder, currentChar & (1 << bit));
        }
        if (currentChar == prevChar) { maxRank = rank > 1 ? kb_bsr32((uint32_t)(rank - 1)) : 0; break; }
        prevChar = currentChar; usedChar[currentChar] = 1;
    }
    return maxRank;
}

static int qlfc_decode_header(kolmo_rc *coder, uint8_t *MTFTable) {
    uint8_t usedChar[256]; memset(usedChar, 0, sizeof(usedChar));
    int prevChar = -1, maxRank = 7;
    for (int rank = 0; rank < 256; ++rank) {
        int currentChar = 0;
        for (int bit = 7; bit >= 0; --bit) {
            int bit0 = 0, bit1 = 0;
            for (int c = currentChar << (bit + 1); c < (currentChar + 1) << (bit + 1); ++c)
                if (c == prevChar || usedChar[c] == 0) { if (c & (1 << bit)) bit1 = 1; else bit0 = 1; if (bit0 && bit1) break; }
            if (bit0 && bit1) currentChar += currentChar + rc_decode_bit(coder, 2048);
            else { if (bit0) currentChar += currentChar + 0; if (bit1) currentChar += currentChar + 1; }
        }
        MTFTable[rank] = (uint8_t)currentChar;
        if (currentChar == prevChar) { maxRank = rank > 1 ? kb_bsr32((uint32_t)(rank - 1)) : 0; break; }
        prevChar = currentChar; usedChar[currentChar] = 1;
    }
    return maxRank;
}

/* Length of the run of input[0] starting at input (input < end). */
static inline int qlfc_run(const uint8_t *input, const uint8_t *end) {
    const uint8_t *p = input + 1; const uint64_t v = 0x0101010101010101ull * input[0];
    while (end - p >= 8) { uint64_t m = kb_ld64(p) ^ v; if (m) return (int)(p - input) + kb_bsf64(m) / 8; p += 8; }
    while (p < end && *p == input[0]) p++;
    return (int)(p - input);
}

#define MIX3(c, s, p, L0, L1, L2) (((c) * (L0) + (s) * (L1) + (p) * (L2)) >> 5)

/* ---- static model ---- */

static int qlfc_static_encode(const uint8_t *input, uint8_t *output, uint8_t *buffer, int inputSize, int outputSize,
                              qlfc_model *model, long *lastCheck) {
    uint8_t MTFTable[256];
    qlfc_init_model(model, 0);
    int contextRank0 = 0, contextRank4 = 0, contextRun = 0, avgRank = 0;
    uint8_t rankHistory[256], runHistory[256]; memset(rankHistory, 0, 256); memset(runHistory, 0, 256);
    const uint8_t *rankArray = qlfc_transform(input, buffer, inputSize, MTFTable);
    kolmo_rc coder; rc_init_encoder(&coder, output, outputSize);
    rc_encode_word(&coder, (uint32_t)inputSize);
    const int maxRank = qlfc_encode_header(&coder, MTFTable);
    const uint8_t *inputEnd = input + inputSize, *rankArrayEnd = buffer + inputSize;
    *lastCheck = LONG_MIN;

    while (rankArray < rankArrayEnd) {
        *lastCheck = rc_pending(&coder);
        if (rc_check_eob(&coder)) return KB_NOT_COMPRESSIBLE;
        int currentChar = *input, runSize = qlfc_run(input, inputEnd); input += runSize;
        int rank = *rankArray++, history = rankHistory[currentChar];
        int state = model_rank_state(contextRank4, contextRun, history);
        int16_t *sp = &model->Rank.StateModel[state], *cp = &model->Rank.CharModel[currentChar], *pp = &model->Rank.StaticModel;
        if (avgRank < 32) {
            if (rank == 1) {
                rankHistory[currentChar] = 0;
                int probability = MIX3(*cp, *sp, *pp, F_RANK_TM_LR0, F_RANK_TM_LR1, F_RANK_TM_LR2);
                qc_update_bit0(sp, F_RANK_TS_TH0, F_RANK_TS_AR0); qc_update_bit0(cp, F_RANK_TC_TH0, F_RANK_TC_AR0); qc_update_bit0(pp, F_RANK_TP_TH0, F_RANK_TP_AR0);
                rc_encode_bit0(&coder, probability);
            } else {
                {
                    int probability = MIX3(*cp, *sp, *pp, F_RANK_TM_LR0, F_RANK_TM_LR1, F_RANK_TM_LR2);
                    qc_update_bit1(sp, F_RANK_TS_TH1, F_RANK_TS_AR1); qc_update_bit1(cp, F_RANK_TC_TH1, F_RANK_TC_AR1); qc_update_bit1(pp, F_RANK_TP_TH1, F_RANK_TP_AR1);
                    rc_encode_bit1(&coder, probability);
                }
                int bitRankSize = kb_bsr32((uint32_t)rank); rankHistory[currentChar] = (uint8_t)bitRankSize;
                sp = &model->Rank.Exponent.StateModel[state][0]; cp = &model->Rank.Exponent.CharModel[currentChar][0]; pp = &model->Rank.Exponent.StaticModel[0];
                for (int bit = 1; bit < bitRankSize; ++bit, ++sp, ++cp, ++pp) {
                    int probability = MIX3(*cp, *sp, *pp, F_RANK_EM_LR0, F_RANK_EM_LR1, F_RANK_EM_LR2);
                    qc_update_bit1(sp, F_RANK_ES_TH1, F_RANK_ES_AR1); qc_update_bit1(cp, F_RANK_EC_TH1, F_RANK_EC_AR1); qc_update_bit1(pp, F_RANK_EP_TH1, F_RANK_EP_AR1);
                    rc_encode_bit1(&coder, probability);
                }
                if (bitRankSize < maxRank) {
                    int probability = MIX3(*cp, *sp, *pp, F_RANK_EM_LR0, F_RANK_EM_LR1, F_RANK_EM_LR2);
                    qc_update_bit0(sp, F_RANK_ES_TH0, F_RANK_ES_AR0); qc_update_bit0(cp, F_RANK_EC_TH0, F_RANK_EC_AR0); qc_update_bit0(pp, F_RANK_EP_TH0, F_RANK_EP_AR0);
                    rc_encode_bit0(&coder, probability);
                }
                sp = &model->Rank.Mantissa[bitRankSize].StateModel[state][0]; cp = &model->Rank.Mantissa[bitRankSize].CharModel[currentChar][0]; pp = &model->Rank.Mantissa[bitRankSize].StaticModel[0];
                for (int context = 1, bit = bitRankSize - 1; bit >= 0; --bit) {
                    int probability = MIX3(cp[context], sp[context], pp[context], F_RANK_MM_LR0, F_RANK_MM_LR1, F_RANK_MM_LR2);
                    unsigned b = (unsigned)(rank >> bit) & 1;
                    qc_update_bit(b, &sp[context], F_RANK_MS_TH0, F_RANK_MS_AR0, F_RANK_MS_TH1, F_RANK_MS_AR1);
                    qc_update_bit(b, &cp[context], F_RANK_MC_TH0, F_RANK_MC_AR0, F_RANK_MC_TH1, F_RANK_MC_AR1);
                    qc_update_bit(b, &pp[context], F_RANK_MP_TH0, F_RANK_MP_AR0, F_RANK_MP_TH1, F_RANK_MP_AR1);
                    context += context + (int)b; rc_encode_bit(&coder, b, probability);
                }
            }
        } else {
            rankHistory[currentChar] = (uint8_t)kb_bsr32((uint32_t)rank);
            sp = &model->Rank.Escape.StateModel[state][0]; cp = &model->Rank.Escape.CharModel[currentChar][0]; pp = &model->Rank.Escape.StaticModel[0];
            for (int context = 1, bit = maxRank; bit >= 0; --bit) {
                int probability = MIX3(cp[context], sp[context], pp[context], F_RANK_PM_LR0, F_RANK_PM_LR1, F_RANK_PM_LR2);
                unsigned b = (unsigned)(rank >> bit) & 1;
                qc_update_bit(b, &sp[context], F_RANK_PS_TH0, F_RANK_PS_AR0, F_RANK_PS_TH1, F_RANK_PS_AR1);
                qc_update_bit(b, &cp[context], F_RANK_PC_TH0, F_RANK_PC_AR0, F_RANK_PC_TH1, F_RANK_PC_AR1);
                qc_update_bit(b, &pp[context], F_RANK_PP_TH0, F_RANK_PP_AR0, F_RANK_PP_TH1, F_RANK_PP_AR1);
                context += context + (int)b; rc_encode_bit(&coder, b, probability);
            }
        }

        avgRank = (avgRank * 124 + rank * 4) >> 7;
        rank = rank - 1;
        history = runHistory[currentChar];
        state = model_run_state(contextRank0, contextRun, rank, history);
        sp = &model->Run.StateModel[state]; cp = &model->Run.CharModel[currentChar]; pp = &model->Run.StaticModel;
        if (runSize == 1) {
            runHistory[currentChar] = (uint8_t)((runHistory[currentChar] + 2) >> 2);
            int probability = MIX3(*cp, *sp, *pp, F_RUN_TM_LR0, F_RUN_TM_LR1, F_RUN_TM_LR2);
            qc_update_bit0(sp, F_RUN_TS_TH0, F_RUN_TS_AR0); qc_update_bit0(cp, F_RUN_TC_TH0, F_RUN_TC_AR0); qc_update_bit0(pp, F_RUN_TP_TH0, F_RUN_TP_AR0);
            rc_encode_bit0(&coder, probability);
        } else {
            {
                int probability = MIX3(*cp, *sp, *pp, F_RUN_TM_LR0, F_RUN_TM_LR1, F_RUN_TM_LR2);
                qc_update_bit1(sp, F_RUN_TS_TH1, F_RUN_TS_AR1); qc_update_bit1(cp, F_RUN_TC_TH1, F_RUN_TC_AR1); qc_update_bit1(pp, F_RUN_TP_TH1, F_RUN_TP_AR1);
                rc_encode_bit1(&coder, probability);
            }
            int bitRunSize = kb_bsr32((uint32_t)runSize); runHistory[currentChar] = (uint8_t)((runHistory[currentChar] + 3 * bitRunSize + 3) >> 2);
            sp = &model->Run.Exponent.StateModel[state][0]; cp = &model->Run.Exponent.CharModel[currentChar][0]; pp = &model->Run.Exponent.StaticModel[0];
            for (int bit = 1; bit < bitRunSize; ++bit, ++sp, ++cp, ++pp) {
                int probability = MIX3(*cp, *sp, *pp, F_RUN_EM_LR0, F_RUN_EM_LR1, F_RUN_EM_LR2);
                qc_update_bit1(sp, F_RUN_ES_TH1, F_RUN_ES_AR1); qc_update_bit1(cp, F_RUN_EC_TH1, F_RUN_EC_AR1); qc_update_bit1(pp, F_RUN_EP_TH1, F_RUN_EP_AR1);
                rc_encode_bit1(&coder, probability);
            }
            {
                int probability = MIX3(*cp, *sp, *pp, F_RUN_EM_LR0, F_RUN_EM_LR1, F_RUN_EM_LR2);
                qc_update_bit0(sp, F_RUN_ES_TH0, F_RUN_ES_AR0); qc_update_bit0(cp, F_RUN_EC_TH0, F_RUN_EC_AR0); qc_update_bit0(pp, F_RUN_EP_TH0, F_RUN_EP_AR0);
                rc_encode_bit0(&coder, probability);
            }
            sp = &model->Run.Mantissa[bitRunSize].StateModel[state][0]; cp = &model->Run.Mantissa[bitRunSize].CharModel[currentChar][0]; pp = &model->Run.Mantissa[bitRunSize].StaticModel[0];
            for (int context = 1, bit = bitRunSize - 1; bit >= 0; --bit) {
                int probability = MIX3(cp[context], sp[context], pp[context], F_RUN_MM_LR0, F_RUN_MM_LR1, F_RUN_MM_LR2);
                unsigned b = (unsigned)(runSize >> bit) & 1;
                qc_update_bit(b, &sp[context], F_RUN_MS_TH0, F_RUN_MS_AR0, F_RUN_MS_TH1, F_RUN_MS_AR1);
                qc_update_bit(b, &cp[context], F_RUN_MC_TH0, F_RUN_MC_AR0, F_RUN_MC_TH1, F_RUN_MC_AR1);
                qc_update_bit(b, &pp[context], F_RUN_MP_TH0, F_RUN_MP_AR0, F_RUN_MP_TH1, F_RUN_MP_AR1);
                int ctx = context + context + (int)b; context++; if (bitRunSize <= 5) context = ctx;
                rc_encode_bit(&coder, b, probability);
            }
        }
        contextRank0 = ((contextRank0 << 1) | (rank == 0 ? 1 : 0)) & 0x7;
        contextRank4 = ((contextRank4 << 2) | (rank < 3 ? rank : 3)) & 0xff;
        contextRun = ((contextRun << 1) | (runSize < 3 ? 1 : 0)) & 0xf;
    }
    return rc_finish_encoder(&coder);
}

/* Shared decoder tail: write a run of `c`, with bounds. */
/* Runs and move-to-front steps are short: with SSE2, up to 16 bytes move in one vector (a call to memset or memmove
 * per symbol was 4% of the decoder's instructions); the bytes written are the same. */
#if defined(__SSE2__)
#include <emmintrin.h>
static const uint8_t qlfc_lanes[32] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
#endif
static inline int qlfc_put_run(uint8_t *output, int *i, int n, int c, int runSize) {
    if ((unsigned)runSize > (unsigned)(n - *i)) return 0;
#if defined(__SSE2__)
    if (runSize <= 16 && n - *i >= 16) { _mm_storeu_si128((__m128i *)(output + *i), _mm_set1_epi8((char)c)); *i += runSize; return 1; }
#endif
    memset(output + *i, c, (size_t)runSize); *i += runSize; return 1;
}
static inline void qlfc_mtf_update(uint8_t *MTFTable, int rank, int currentChar) {
#if defined(__SSE2__)
    if (rank < 16) {   /* lanes below rank take the next byte, the others stay (the table has 256 bytes) */
        __m128i a = _mm_loadu_si128((const __m128i *)MTFTable), b = _mm_loadu_si128((const __m128i *)(MTFTable + 1));
        __m128i m = _mm_loadu_si128((const __m128i *)(qlfc_lanes + 16 - rank));
        _mm_storeu_si128((__m128i *)MTFTable, _mm_or_si128(_mm_and_si128(m, b), _mm_andnot_si128(m, a)));
        MTFTable[rank] = (uint8_t)currentChar; return;
    }
#endif
    memmove(MTFTable, MTFTable + 1, (size_t)rank); MTFTable[rank] = (uint8_t)currentChar;
}

static int qlfc_static_decode(const uint8_t *input, const uint8_t *inputEnd, uint8_t *output, int outCap, qlfc_model *model) {
    kolmo_rc coder;
    uint8_t MTFTable[256];
    qlfc_init_model(model, 0);
    int contextRank0 = 0, contextRank4 = 0, contextRun = 0, avgRank = 0;
    uint8_t rankHistory[256], runHistory[256]; memset(rankHistory, 0, 256); memset(runHistory, 0, 256);
    rc_init_decoder(&coder, input, inputEnd);
    int n = (int)rc_decode_word(&coder);
    if (n < 0 || n > outCap) return KB_DATA_CORRUPT;
    const int maxRank = qlfc_decode_header(&coder, MTFTable);

    for (int i = 0; i < n;) {
        int currentChar = MTFTable[0], history = rankHistory[currentChar];
        int state = model_rank_state(contextRank4, contextRun, history);
        int16_t *sp = &model->Rank.StateModel[state], *cp = &model->Rank.CharModel[currentChar], *pp = &model->Rank.StaticModel;
        int rank = 1;
        if (avgRank < 32) {
            if (rc_decode_bit(&coder, MIX3(*cp, *sp, *pp, F_RANK_TM_LR0, F_RANK_TM_LR1, F_RANK_TM_LR2))) {
                qc_update_bit1(sp, F_RANK_TS_TH1, F_RANK_TS_AR1); qc_update_bit1(cp, F_RANK_TC_TH1, F_RANK_TC_AR1); qc_update_bit1(pp, F_RANK_TP_TH1, F_RANK_TP_AR1);
                sp = &model->Rank.Exponent.StateModel[state][0]; cp = &model->Rank.Exponent.CharModel[currentChar][0]; pp = &model->Rank.Exponent.StaticModel[0];
                int bitRankSize = 1;
                for (;;) {
                    if (bitRankSize == maxRank) break;
                    int probability = MIX3(*cp, *sp, *pp, F_RANK_EM_LR0, F_RANK_EM_LR1, F_RANK_EM_LR2);
                    if (rc_peek_bit(&coder, probability)) {
                        qc_update_bit1(sp, F_RANK_ES_TH1, F_RANK_ES_AR1); sp++;
                        qc_update_bit1(cp, F_RANK_EC_TH1, F_RANK_EC_AR1); cp++;
                        qc_update_bit1(pp, F_RANK_EP_TH1, F_RANK_EP_AR1); pp++;
                        bitRankSize++;
                        rc_decode_bit1(&coder, probability);
                        if (bitRankSize > 7) return KB_DATA_CORRUPT;
                    } else {
                        qc_update_bit0(sp, F_RANK_ES_TH0, F_RANK_ES_AR0); qc_update_bit0(cp, F_RANK_EC_TH0, F_RANK_EC_AR0); qc_update_bit0(pp, F_RANK_EP_TH0, F_RANK_EP_AR0);
                        rc_decode_bit0(&coder, probability);
                        break;
                    }
                }
                rankHistory[currentChar] = (uint8_t)bitRankSize;
                sp = &model->Rank.Mantissa[bitRankSize].StateModel[state][0]; cp = &model->Rank.Mantissa[bitRankSize].CharModel[currentChar][0]; pp = &model->Rank.Mantissa[bitRankSize].StaticModel[0];
                for (int bit = bitRankSize - 1; bit >= 0; --bit) {
                    unsigned b = (unsigned)rc_decode_bit(&coder, MIX3(cp[rank], sp[rank], pp[rank], F_RANK_MM_LR0, F_RANK_MM_LR1, F_RANK_MM_LR2));
                    qc_update_bit(b, &sp[rank], F_RANK_MS_TH0, F_RANK_MS_AR0, F_RANK_MS_TH1, F_RANK_MS_AR1);
                    qc_update_bit(b, &cp[rank], F_RANK_MC_TH0, F_RANK_MC_AR0, F_RANK_MC_TH1, F_RANK_MC_AR1);
                    qc_update_bit(b, &pp[rank], F_RANK_MP_TH0, F_RANK_MP_AR0, F_RANK_MP_TH1, F_RANK_MP_AR1);
                    rank += rank + (int)b;
                }
            } else {
                rankHistory[currentChar] = 0;
                qc_update_bit0(sp, F_RANK_TS_TH0, F_RANK_TS_AR0); qc_update_bit0(cp, F_RANK_TC_TH0, F_RANK_TC_AR0); qc_update_bit0(pp, F_RANK_TP_TH0, F_RANK_TP_AR0);
            }
        } else {
            sp = &model->Rank.Escape.StateModel[state][0]; cp = &model->Rank.Escape.CharModel[currentChar][0]; pp = &model->Rank.Escape.StaticModel[0];
            rank = 0;
            for (int context = 1, bit = maxRank; bit >= 0; --bit) {
                unsigned b = (unsigned)rc_decode_bit(&coder, MIX3(cp[context], sp[context], pp[context], F_RANK_PM_LR0, F_RANK_PM_LR1, F_RANK_PM_LR2));
                qc_update_bit(b, &sp[context], F_RANK_PS_TH0, F_RANK_PS_AR0, F_RANK_PS_TH1, F_RANK_PS_AR1);
                qc_update_bit(b, &cp[context], F_RANK_PC_TH0, F_RANK_PC_AR0, F_RANK_PC_TH1, F_RANK_PC_AR1);
                qc_update_bit(b, &pp[context], F_RANK_PP_TH0, F_RANK_PP_AR0, F_RANK_PP_TH1, F_RANK_PP_AR1);
                context += context + (int)b; rank += rank + (int)b;
            }
            if (rank == 0) return KB_DATA_CORRUPT;               /* the encoder never writes rank 0 here */
            rankHistory[currentChar] = (uint8_t)kb_bsr32((uint32_t)rank);
        }
        qlfc_mtf_update(MTFTable, rank, currentChar);

        avgRank = (avgRank * 124 + rank * 4) >> 7;
        rank = rank - 1;
        history = runHistory[currentChar];
        state = model_run_state(contextRank0, contextRun, rank, history);
        sp = &model->Run.StateModel[state]; cp = &model->Run.CharModel[currentChar]; pp = &model->Run.StaticModel;
        if (rc_decode_bit(&coder, MIX3(*cp, *sp, *pp, F_RUN_TM_LR0, F_RUN_TM_LR1, F_RUN_TM_LR2))) {
            qc_update_bit1(sp, F_RUN_TS_TH1, F_RUN_TS_AR1); qc_update_bit1(cp, F_RUN_TC_TH1, F_RUN_TC_AR1); qc_update_bit1(pp, F_RUN_TP_TH1, F_RUN_TP_AR1);
            sp = &model->Run.Exponent.StateModel[state][0]; cp = &model->Run.Exponent.CharModel[currentChar][0]; pp = &model->Run.Exponent.StaticModel[0];
            int runSize = 1, bitRunSize = 1;
            for (;;) {
                int probability = MIX3(*cp, *sp, *pp, F_RUN_EM_LR0, F_RUN_EM_LR1, F_RUN_EM_LR2);
                if (rc_peek_bit(&coder, probability)) {
                    qc_update_bit1(sp, F_RUN_ES_TH1, F_RUN_ES_AR1); sp++;
                    qc_update_bit1(cp, F_RUN_EC_TH1, F_RUN_EC_AR1); cp++;
                    qc_update_bit1(pp, F_RUN_EP_TH1, F_RUN_EP_AR1); pp++;
                    bitRunSize++;
                    rc_decode_bit1(&coder, probability);
                    if (bitRunSize > 30) return KB_DATA_CORRUPT;
                } else {
                    qc_update_bit0(sp, F_RUN_ES_TH0, F_RUN_ES_AR0); qc_update_bit0(cp, F_RUN_EC_TH0, F_RUN_EC_AR0); qc_update_bit0(pp, F_RUN_EP_TH0, F_RUN_EP_AR0);
                    rc_decode_bit0(&coder, probability);
                    break;
                }
            }
            runHistory[currentChar] = (uint8_t)((runHistory[currentChar] + 3 * bitRunSize + 3) >> 2);
            sp = &model->Run.Mantissa[bitRunSize].StateModel[state][0]; cp = &model->Run.Mantissa[bitRunSize].CharModel[currentChar][0]; pp = &model->Run.Mantissa[bitRunSize].StaticModel[0];
            for (int context = 1, bit = bitRunSize - 1; bit >= 0; --bit) {
                unsigned b = (unsigned)rc_decode_bit(&coder, MIX3(cp[context], sp[context], pp[context], F_RUN_MM_LR0, F_RUN_MM_LR1, F_RUN_MM_LR2));
                qc_update_bit(b, &sp[context], F_RUN_MS_TH0, F_RUN_MS_AR0, F_RUN_MS_TH1, F_RUN_MS_AR1);
                qc_update_bit(b, &cp[context], F_RUN_MC_TH0, F_RUN_MC_AR0, F_RUN_MC_TH1, F_RUN_MC_AR1);
                qc_update_bit(b, &pp[context], F_RUN_MP_TH0, F_RUN_MP_AR0, F_RUN_MP_TH1, F_RUN_MP_AR1);
                runSize += runSize + (int)b; int ctx = context + context + (int)b; context++; if (bitRunSize <= 5) context = ctx;
            }
            contextRank0 = ((contextRank0 << 1) | (rank == 0 ? 1 : 0)) & 0x7;
            contextRank4 = ((contextRank4 << 2) | (rank < 3 ? rank : 3)) & 0xff;
            contextRun = ((contextRun << 1) | (runSize < 3 ? 1 : 0)) & 0xf;
            if (!qlfc_put_run(output, &i, n, currentChar, runSize)) return KB_DATA_CORRUPT;
        } else {
            runHistory[currentChar] = (uint8_t)((runHistory[currentChar] + 2) >> 2);
            qc_update_bit0(sp, F_RUN_TS_TH0, F_RUN_TS_AR0); qc_update_bit0(cp, F_RUN_TC_TH0, F_RUN_TC_AR0); qc_update_bit0(pp, F_RUN_TP_TH0, F_RUN_TP_AR0);
            contextRank0 = ((contextRank0 << 1) | (rank == 0 ? 1 : 0)) & 0x7;
            contextRank4 = ((contextRank4 << 2) | (rank < 3 ? rank : 3)) & 0xff;
            contextRun = ((contextRun << 1) | 1) & 0xf;
            output[i++] = (uint8_t)currentChar;
        }
    }
    return n;
}

/* ---- adaptive model (mixers) ---- */

#define ENC_BIT0(MX, L, TH, AR) rc_encode_bit0(&coder, qm_mix_update0(MX, p0, p1, p2, L##0, L##1, L##2, TH, AR))
#define ENC_BIT1(MX, L, TH, AR) rc_encode_bit1(&coder, qm_mix_update1(MX, p0, p1, p2, L##0, L##1, L##2, TH, AR))

static int qlfc_adaptive_encode(const uint8_t *input, uint8_t *output, uint8_t *buffer, int inputSize, int outputSize,
                                qlfc_model *model, long *lastCheck) {
    uint8_t MTFTable[256];
    qlfc_init_model(model, 1);
    int contextRank0 = 0, contextRank4 = 0, contextRun = 0, avgRank = 0;
    uint8_t rankHistory[256], runHistory[256]; memset(rankHistory, 0, 256); memset(runHistory, 0, 256);
    const uint8_t *rankArray = qlfc_transform(input, buffer, inputSize, MTFTable);
    kolmo_rc coder; rc_init_encoder(&coder, output, outputSize);
    rc_encode_word(&coder, (uint32_t)inputSize);
    const int maxRank = qlfc_encode_header(&coder, MTFTable);
    const uint8_t *inputEnd = input + inputSize, *rankArrayEnd = buffer + inputSize;
    *lastCheck = LONG_MIN;

    while (rankArray < rankArrayEnd) {
        *lastCheck = rc_pending(&coder);
        if (rc_check_eob(&coder)) return KB_NOT_COMPRESSIBLE;
        int currentChar = *input, runSize = qlfc_run(input, inputEnd); input += runSize;
        int rank = *rankArray++, history = rankHistory[currentChar];
        int state = model_rank_state(contextRank4, contextRun, history);
        int16_t *sp = &model->Rank.StateModel[state], *cp = &model->Rank.CharModel[currentChar], *pp = &model->Rank.StaticModel;
        qlfc_mixer *mixer = &model->mixerOfRank[currentChar];
        int p0, p1, p2;
        if (avgRank < 32) {
            if (rank == 1) {
                rankHistory[currentChar] = 0;
                p0 = *cp; p1 = *sp; p2 = *pp;
                qc_update_bit0(sp, M_RANK_TS_TH0, M_RANK_TS_AR0); qc_update_bit0(cp, M_RANK_TC_TH0, M_RANK_TC_AR0); qc_update_bit0(pp, M_RANK_TP_TH0, M_RANK_TP_AR0);
                ENC_BIT0(mixer, M_RANK_TM_LR, M_RANK_TM_TH0, M_RANK_TM_AR0);
            } else {
                p0 = *cp; p1 = *sp; p2 = *pp;
                qc_update_bit1(sp, M_RANK_TS_TH1, M_RANK_TS_AR1); qc_update_bit1(cp, M_RANK_TC_TH1, M_RANK_TC_AR1); qc_update_bit1(pp, M_RANK_TP_TH1, M_RANK_TP_AR1);
                ENC_BIT1(mixer, M_RANK_TM_LR, M_RANK_TM_TH1, M_RANK_TM_AR1);

                int bitRankSize = kb_bsr32((uint32_t)rank); rankHistory[currentChar] = (uint8_t)bitRankSize;
                sp = &model->Rank.Exponent.StateModel[state][0]; cp = &model->Rank.Exponent.CharModel[currentChar][0]; pp = &model->Rank.Exponent.StaticModel[0];
                mixer = &model->mixerOfRankExponent[history < 1 ? 1 : history][1];
                for (int bit = 1; bit < bitRankSize; ++bit, ++sp, ++cp, ++pp) {
                    p0 = *cp; p1 = *sp; p2 = *pp;
                    qc_update_bit1(sp, M_RANK_ES_TH1, M_RANK_ES_AR1); qc_update_bit1(cp, M_RANK_EC_TH1, M_RANK_EC_AR1); qc_update_bit1(pp, M_RANK_EP_TH1, M_RANK_EP_AR1);
                    ENC_BIT1(mixer, M_RANK_EM_LR, M_RANK_EM_TH1, M_RANK_EM_AR1);
                    mixer = &model->mixerOfRankExponent[history <= bit ? bit + 1 : history][bit + 1];
                }
                if (bitRankSize < maxRank) {
                    p0 = *cp; p1 = *sp; p2 = *pp;
                    qc_update_bit0(sp, M_RANK_ES_TH0, M_RANK_ES_AR0); qc_update_bit0(cp, M_RANK_EC_TH0, M_RANK_EC_AR0); qc_update_bit0(pp, M_RANK_EP_TH0, M_RANK_EP_AR0);
                    ENC_BIT0(mixer, M_RANK_EM_LR, M_RANK_EM_TH0, M_RANK_EM_AR0);
                }
                sp = &model->Rank.Mantissa[bitRankSize].StateModel[state][0]; cp = &model->Rank.Mantissa[bitRankSize].CharModel[currentChar][0]; pp = &model->Rank.Mantissa[bitRankSize].StaticModel[0];
                mixer = &model->mixerOfRankMantissa[bitRankSize];
                for (int context = 1, bit = bitRankSize - 1; bit >= 0; --bit) {
                    p0 = cp[context]; p1 = sp[context]; p2 = pp[context];
                    if ((rank >> bit) & 1) {
                        qc_update_bit1(&sp[context], M_RANK_MS_TH1, M_RANK_MS_AR1); qc_update_bit1(&cp[context], M_RANK_MC_TH1, M_RANK_MC_AR1); qc_update_bit1(&pp[context], M_RANK_MP_TH1, M_RANK_MP_AR1);
                        ENC_BIT1(mixer, M_RANK_MM_LR, M_RANK_MM_TH1, M_RANK_MM_AR1);
                        context += context + 1;
                    } else {
                        qc_update_bit0(&sp[context], M_RANK_MS_TH0, M_RANK_MS_AR0); qc_update_bit0(&cp[context], M_RANK_MC_TH0, M_RANK_MC_AR0); qc_update_bit0(&pp[context], M_RANK_MP_TH0, M_RANK_MP_AR0);
                        ENC_BIT0(mixer, M_RANK_MM_LR, M_RANK_MM_TH0, M_RANK_MM_AR0);
                        context += context;
                    }
                }
            }
        } else {
            rankHistory[currentChar] = (uint8_t)kb_bsr32((uint32_t)rank);
            sp = &model->Rank.Escape.StateModel[state][0]; cp = &model->Rank.Escape.CharModel[currentChar][0]; pp = &model->Rank.Escape.StaticModel[0];
            for (int context = 1, bit = maxRank; bit >= 0; --bit) {
                mixer = &model->mixerOfRankEscape[context];
                p0 = cp[context]; p1 = sp[context]; p2 = pp[context];
                if ((rank >> bit) & 1) {
                    qc_update_bit1(&sp[context], M_RANK_PS_TH1, M_RANK_PS_AR1); qc_update_bit1(&cp[context], M_RANK_PC_TH1, M_RANK_PC_AR1); qc_update_bit1(&pp[context], M_RANK_PP_TH1, M_RANK_PP_AR1);
                    ENC_BIT1(mixer, M_RANK_PM_LR, M_RANK_PM_TH1, M_RANK_PM_AR1);
                    context += context + 1;
                } else {
                    qc_update_bit0(&sp[context], M_RANK_PS_TH0, M_RANK_PS_AR0); qc_update_bit0(&cp[context], M_RANK_PC_TH0, M_RANK_PC_AR0); qc_update_bit0(&pp[context], M_RANK_PP_TH0, M_RANK_PP_AR0);
                    ENC_BIT0(mixer, M_RANK_PM_LR, M_RANK_PM_TH0, M_RANK_PM_AR0);
                    context += context;
                }
            }
        }

        avgRank = (avgRank * 124 + rank * 4) >> 7;
        rank = rank - 1;
        history = runHistory[currentChar];
        state = model_run_state(contextRank0, contextRun, rank, history);
        sp = &model->Run.StateModel[state]; cp = &model->Run.CharModel[currentChar]; pp = &model->Run.StaticModel;
        mixer = &model->mixerOfRun[currentChar];
        if (runSize == 1) {
            runHistory[currentChar] = (uint8_t)((runHistory[currentChar] + 2) >> 2);
            p0 = *cp; p1 = *sp; p2 = *pp;
            qc_update_bit0(sp, M_RUN_TS_TH0, M_RUN_TS_AR0); qc_update_bit0(cp, M_RUN_TC_TH0, M_RUN_TC_AR0); qc_update_bit0(pp, M_RUN_TP_TH0, M_RUN_TP_AR0);
            ENC_BIT0(mixer, M_RUN_TM_LR, M_RUN_TM_TH0, M_RUN_TM_AR0);
        } else {
            p0 = *cp; p1 = *sp; p2 = *pp;
            qc_update_bit1(sp, M_RUN_TS_TH1, M_RUN_TS_AR1); qc_update_bit1(cp, M_RUN_TC_TH1, M_RUN_TC_AR1); qc_update_bit1(pp, M_RUN_TP_TH1, M_RUN_TP_AR1);
            ENC_BIT1(mixer, M_RUN_TM_LR, M_RUN_TM_TH1, M_RUN_TM_AR1);

            int bitRunSize = kb_bsr32((uint32_t)runSize); runHistory[currentChar] = (uint8_t)((runHistory[currentChar] + 3 * bitRunSize + 3) >> 2);
            sp = &model->Run.Exponent.StateModel[state][0]; cp = &model->Run.Exponent.CharModel[currentChar][0]; pp = &model->Run.Exponent.StaticModel[0];
            mixer = &model->mixerOfRunExponent[history < 1 ? 1 : history][1];
            for (int bit = 1; bit < bitRunSize; ++bit, ++sp, ++cp, ++pp) {
                p0 = *cp; p1 = *sp; p2 = *pp;
                qc_update_bit1(sp, M_RUN_ES_TH1, M_RUN_ES_AR1); qc_update_bit1(cp, M_RUN_EC_TH1, M_RUN_EC_AR1); qc_update_bit1(pp, M_RUN_EP_TH1, M_RUN_EP_AR1);
                ENC_BIT1(mixer, M_RUN_EM_LR, M_RUN_EM_TH1, M_RUN_EM_AR1);
                mixer = &model->mixerOfRunExponent[history <= bit ? bit + 1 : history][bit + 1];
            }
            p0 = *cp; p1 = *sp; p2 = *pp;
            qc_update_bit0(sp, M_RUN_ES_TH0, M_RUN_ES_AR0); qc_update_bit0(cp, M_RUN_EC_TH0, M_RUN_EC_AR0); qc_update_bit0(pp, M_RUN_EP_TH0, M_RUN_EP_AR0);
            ENC_BIT0(mixer, M_RUN_EM_LR, M_RUN_EM_TH0, M_RUN_EM_AR0);

            sp = &model->Run.Mantissa[bitRunSize].StateModel[state][0]; cp = &model->Run.Mantissa[bitRunSize].CharModel[currentChar][0]; pp = &model->Run.Mantissa[bitRunSize].StaticModel[0];
            mixer = &model->mixerOfRunMantissa[bitRunSize];
            for (int context = 1, bit = bitRunSize - 1; bit >= 0; --bit) {
                p0 = cp[context]; p1 = sp[context]; p2 = pp[context];
                if ((runSize >> bit) & 1) {
                    qc_update_bit1(&sp[context], M_RUN_MS_TH1, M_RUN_MS_AR1); qc_update_bit1(&cp[context], M_RUN_MC_TH1, M_RUN_MC_AR1); qc_update_bit1(&pp[context], M_RUN_MP_TH1, M_RUN_MP_AR1);
                    ENC_BIT1(mixer, M_RUN_MM_LR, M_RUN_MM_TH1, M_RUN_MM_AR1);
                    if (bitRunSize <= 5) context += context + 1; else context++;
                } else {
                    qc_update_bit0(&sp[context], M_RUN_MS_TH0, M_RUN_MS_AR0); qc_update_bit0(&cp[context], M_RUN_MC_TH0, M_RUN_MC_AR0); qc_update_bit0(&pp[context], M_RUN_MP_TH0, M_RUN_MP_AR0);
                    ENC_BIT0(mixer, M_RUN_MM_LR, M_RUN_MM_TH0, M_RUN_MM_AR0);
                    if (bitRunSize <= 5) context += context + 0; else context++;
                }
            }
        }
        contextRank0 = ((contextRank0 << 1) | (rank == 0 ? 1 : 0)) & 0x7;
        contextRank4 = ((contextRank4 << 2) | (rank < 3 ? rank : 3)) & 0xff;
        contextRun = ((contextRun << 1) | (runSize < 3 ? 1 : 0)) & 0xf;
    }
    return rc_finish_encoder(&coder);
}

#define DEC_BIT(MX, P0, P1, P2) rc_decode_bit(&coder, qm_mixup(MX, P0, P1, P2))
#define MX_UP0(MX, L, TH, AR) qm_update0(MX, L##0, L##1, L##2, TH, AR)
#define MX_UP1(MX, L, TH, AR) qm_update1(MX, L##0, L##1, L##2, TH, AR)

static int qlfc_adaptive_decode(const uint8_t *input, const uint8_t *inputEnd, uint8_t *output, int outCap, qlfc_model *model) {
    kolmo_rc coder;
    uint8_t MTFTable[256];
    qlfc_init_model(model, 1);
    int contextRank0 = 0, contextRank4 = 0, contextRun = 0, avgRank = 0;
    uint8_t rankHistory[256], runHistory[256]; memset(rankHistory, 0, 256); memset(runHistory, 0, 256);
    rc_init_decoder(&coder, input, inputEnd);
    int n = (int)rc_decode_word(&coder);
    if (n < 0 || n > outCap) return KB_DATA_CORRUPT;
    const int maxRank = qlfc_decode_header(&coder, MTFTable);

    for (int i = 0; i < n;) {
        int currentChar = MTFTable[0], history = rankHistory[currentChar];
        int state = model_rank_state(contextRank4, contextRun, history);
        int16_t *sp = &model->Rank.StateModel[state], *cp = &model->Rank.CharModel[currentChar], *pp = &model->Rank.StaticModel;
        qlfc_mixer *mixer = &model->mixerOfRank[currentChar];
        int rank = 1;
        if (avgRank < 32) {
            if (DEC_BIT(mixer, *cp, *sp, *pp)) {
                qc_update_bit1(sp, M_RANK_TS_TH1, M_RANK_TS_AR1); qc_update_bit1(cp, M_RANK_TC_TH1, M_RANK_TC_AR1); qc_update_bit1(pp, M_RANK_TP_TH1, M_RANK_TP_AR1);
                MX_UP1(mixer, M_RANK_TM_LR, M_RANK_TM_TH1, M_RANK_TM_AR1);
                sp = &model->Rank.Exponent.StateModel[state][0]; cp = &model->Rank.Exponent.CharModel[currentChar][0]; pp = &model->Rank.Exponent.StaticModel[0];
                mixer = &model->mixerOfRankExponent[history < 1 ? 1 : history][1];
                int bitRankSize = 1;
                for (;;) {
                    if (bitRankSize == maxRank) break;
                    if (DEC_BIT(mixer, *cp, *sp, *pp)) {
                        qc_update_bit1(sp, M_RANK_ES_TH1, M_RANK_ES_AR1); sp++;
                        qc_update_bit1(cp, M_RANK_EC_TH1, M_RANK_EC_AR1); cp++;
                        qc_update_bit1(pp, M_RANK_EP_TH1, M_RANK_EP_AR1); pp++;
                        MX_UP1(mixer, M_RANK_EM_LR, M_RANK_EM_TH1, M_RANK_EM_AR1);
                        bitRankSize++;
                        if (bitRankSize > 7) return KB_DATA_CORRUPT;
                        mixer = &model->mixerOfRankExponent[history < bitRankSize ? bitRankSize : history][bitRankSize];
                    } else {
                        qc_update_bit0(sp, M_RANK_ES_TH0, M_RANK_ES_AR0); qc_update_bit0(cp, M_RANK_EC_TH0, M_RANK_EC_AR0); qc_update_bit0(pp, M_RANK_EP_TH0, M_RANK_EP_AR0);
                        MX_UP0(mixer, M_RANK_EM_LR, M_RANK_EM_TH0, M_RANK_EM_AR0);
                        break;
                    }
                }
                rankHistory[currentChar] = (uint8_t)bitRankSize;
                sp = &model->Rank.Mantissa[bitRankSize].StateModel[state][0]; cp = &model->Rank.Mantissa[bitRankSize].CharModel[currentChar][0]; pp = &model->Rank.Mantissa[bitRankSize].StaticModel[0];
                mixer = &model->mixerOfRankMantissa[bitRankSize];
                for (int bit = bitRankSize - 1; bit >= 0; --bit) {
                    if (DEC_BIT(mixer, cp[rank], sp[rank], pp[rank])) {
                        qc_update_bit1(&sp[rank], M_RANK_MS_TH1, M_RANK_MS_AR1); qc_update_bit1(&cp[rank], M_RANK_MC_TH1, M_RANK_MC_AR1); qc_update_bit1(&pp[rank], M_RANK_MP_TH1, M_RANK_MP_AR1);
                        MX_UP1(mixer, M_RANK_MM_LR, M_RANK_MM_TH1, M_RANK_MM_AR1);
                        rank += rank + 1;
                    } else {
                        qc_update_bit0(&sp[rank], M_RANK_MS_TH0, M_RANK_MS_AR0); qc_update_bit0(&cp[rank], M_RANK_MC_TH0, M_RANK_MC_AR0); qc_update_bit0(&pp[rank], M_RANK_MP_TH0, M_RANK_MP_AR0);
                        MX_UP0(mixer, M_RANK_MM_LR, M_RANK_MM_TH0, M_RANK_MM_AR0);
                        rank += rank;
                    }
                }
            } else {
                rankHistory[currentChar] = 0;
                qc_update_bit0(sp, M_RANK_TS_TH0, M_RANK_TS_AR0); qc_update_bit0(cp, M_RANK_TC_TH0, M_RANK_TC_AR0); qc_update_bit0(pp, M_RANK_TP_TH0, M_RANK_TP_AR0);
                MX_UP0(mixer, M_RANK_TM_LR, M_RANK_TM_TH0, M_RANK_TM_AR0);
            }
        } else {
            sp = &model->Rank.Escape.StateModel[state][0]; cp = &model->Rank.Escape.CharModel[currentChar][0]; pp = &model->Rank.Escape.StaticModel[0];
            rank = 0;
            for (int context = 1, bit = maxRank; bit >= 0; --bit) {
                mixer = &model->mixerOfRankEscape[context];
                if (DEC_BIT(mixer, cp[context], sp[context], pp[context])) {
                    qc_update_bit1(&sp[context], M_RANK_PS_TH1, M_RANK_PS_AR1); qc_update_bit1(&cp[context], M_RANK_PC_TH1, M_RANK_PC_AR1); qc_update_bit1(&pp[context], M_RANK_PP_TH1, M_RANK_PP_AR1);
                    MX_UP1(mixer, M_RANK_PM_LR, M_RANK_PM_TH1, M_RANK_PM_AR1);
                    context += context + 1; rank += rank + 1;
                } else {
                    qc_update_bit0(&sp[context], M_RANK_PS_TH0, M_RANK_PS_AR0); qc_update_bit0(&cp[context], M_RANK_PC_TH0, M_RANK_PC_AR0); qc_update_bit0(&pp[context], M_RANK_PP_TH0, M_RANK_PP_AR0);
                    MX_UP0(mixer, M_RANK_PM_LR, M_RANK_PM_TH0, M_RANK_PM_AR0);
                    context += context; rank += rank;
                }
            }
            if (rank == 0) return KB_DATA_CORRUPT;
            rankHistory[currentChar] = (uint8_t)kb_bsr32((uint32_t)rank);
        }
        qlfc_mtf_update(MTFTable, rank, currentChar);

        avgRank = (avgRank * 124 + rank * 4) >> 7;
        rank = rank - 1;
        history = runHistory[currentChar];
        state = model_run_state(contextRank0, contextRun, rank, history);
        sp = &model->Run.StateModel[state]; cp = &model->Run.CharModel[currentChar]; pp = &model->Run.StaticModel;
        mixer = &model->mixerOfRun[currentChar];
        if (DEC_BIT(mixer, *cp, *sp, *pp)) {
            qc_update_bit1(sp, M_RUN_TS_TH1, M_RUN_TS_AR1); qc_update_bit1(cp, M_RUN_TC_TH1, M_RUN_TC_AR1); qc_update_bit1(pp, M_RUN_TP_TH1, M_RUN_TP_AR1);
            MX_UP1(mixer, M_RUN_TM_LR, M_RUN_TM_TH1, M_RUN_TM_AR1);
            sp = &model->Run.Exponent.StateModel[state][0]; cp = &model->Run.Exponent.CharModel[currentChar][0]; pp = &model->Run.Exponent.StaticModel[0];
            mixer = &model->mixerOfRunExponent[history < 1 ? 1 : history][1];
            int runSize = 1, bitRunSize = 1;
            for (;;) {
                if (DEC_BIT(mixer, *cp, *sp, *pp)) {
                    qc_update_bit1(sp, M_RUN_ES_TH1, M_RUN_ES_AR1); sp++;
                    qc_update_bit1(cp, M_RUN_EC_TH1, M_RUN_EC_AR1); cp++;
                    qc_update_bit1(pp, M_RUN_EP_TH1, M_RUN_EP_AR1); pp++;
                    MX_UP1(mixer, M_RUN_EM_LR, M_RUN_EM_TH1, M_RUN_EM_AR1);
                    bitRunSize++;
                    if (bitRunSize > 30) return KB_DATA_CORRUPT;
                    mixer = &model->mixerOfRunExponent[history < bitRunSize ? bitRunSize : history][bitRunSize];
                } else {
                    qc_update_bit0(sp, M_RUN_ES_TH0, M_RUN_ES_AR0); qc_update_bit0(cp, M_RUN_EC_TH0, M_RUN_EC_AR0); qc_update_bit0(pp, M_RUN_EP_TH0, M_RUN_EP_AR0);
                    MX_UP0(mixer, M_RUN_EM_LR, M_RUN_EM_TH0, M_RUN_EM_AR0);
                    break;
                }
            }
            runHistory[currentChar] = (uint8_t)((runHistory[currentChar] + 3 * bitRunSize + 3) >> 2);
            sp = &model->Run.Mantissa[bitRunSize].StateModel[state][0]; cp = &model->Run.Mantissa[bitRunSize].CharModel[currentChar][0]; pp = &model->Run.Mantissa[bitRunSize].StaticModel[0];
            mixer = &model->mixerOfRunMantissa[bitRunSize];
            for (int context = 1, bit = bitRunSize - 1; bit >= 0; --bit) {
                if (DEC_BIT(mixer, cp[context], sp[context], pp[context])) {
                    qc_update_bit1(&sp[context], M_RUN_MS_TH1, M_RUN_MS_AR1); qc_update_bit1(&cp[context], M_RUN_MC_TH1, M_RUN_MC_AR1); qc_update_bit1(&pp[context], M_RUN_MP_TH1, M_RUN_MP_AR1);
                    MX_UP1(mixer, M_RUN_MM_LR, M_RUN_MM_TH1, M_RUN_MM_AR1);
                    runSize += runSize + 1; if (bitRunSize <= 5) context += context + 1; else context++;
                } else {
                    qc_update_bit0(&sp[context], M_RUN_MS_TH0, M_RUN_MS_AR0); qc_update_bit0(&cp[context], M_RUN_MC_TH0, M_RUN_MC_AR0); qc_update_bit0(&pp[context], M_RUN_MP_TH0, M_RUN_MP_AR0);
                    MX_UP0(mixer, M_RUN_MM_LR, M_RUN_MM_TH0, M_RUN_MM_AR0);
                    runSize += runSize; if (bitRunSize <= 5) context += context; else context++;
                }
            }
            contextRank0 = ((contextRank0 << 1) | (rank == 0 ? 1 : 0)) & 0x7;
            contextRank4 = ((contextRank4 << 2) | (rank < 3 ? rank : 3)) & 0xff;
            contextRun = ((contextRun << 1) | (runSize < 3 ? 1 : 0)) & 0xf;
            if (!qlfc_put_run(output, &i, n, currentChar, runSize)) return KB_DATA_CORRUPT;
        } else {
            runHistory[currentChar] = (uint8_t)((runHistory[currentChar] + 2) >> 2);
            qc_update_bit0(sp, M_RUN_TS_TH0, M_RUN_TS_AR0); qc_update_bit0(cp, M_RUN_TC_TH0, M_RUN_TC_AR0); qc_update_bit0(pp, M_RUN_TP_TH0, M_RUN_TP_AR0);
            MX_UP0(mixer, M_RUN_TM_LR, M_RUN_TM_TH0, M_RUN_TM_AR0);
            contextRank0 = ((contextRank0 << 1) | (rank == 0 ? 1 : 0)) & 0x7;
            contextRank4 = ((contextRank4 << 2) | (rank < 3 ? rank : 3)) & 0xff;
            contextRun = ((contextRun << 1) | 1) & 0xf;
            output[i++] = (uint8_t)currentChar;
        }
    }
    return n;
}

/* ---- block driver (libbsc coder.cpp) ---- */

int kolmo_qlfc_num_blocks(int n) {
    static const int blocks[] = {128, 96, 64, 48, 32, 24, 16, 12, 8, 6, 4, 2};
    for (unsigned i = 0; i < sizeof(blocks) / sizeof(blocks[0]); i++)
        if (n >= blocks[i] * blocks[i] * 65536) return blocks[i];
    return 1;
}

/* Split at equal numbers of symbol changes (sampled every 32 bytes), as libbsc does. */
static void qlfc_split_blocks(const uint8_t *input, int n, int nBlocks, int *blockStart, int *blockSize) {
    int rankSize = 0;
    for (int i = 1; i < n; i += 32) rankSize += input[i] != input[i - 1];
    if (rankSize > nBlocks) {
        int blockRankSize = rankSize / nBlocks;
        blockStart[0] = 0; rankSize = 0;
        for (int id = 0, i = 1; i < n; i += 32) {
            rankSize += input[i] != input[i - 1];
            if (rankSize == blockRankSize) {
                rankSize = 0;
                blockSize[id] = i - blockStart[id];
                id++; blockStart[id] = i;
                if (id == nBlocks - 1) break;
            }
        }
        blockSize[nBlocks - 1] = n - blockStart[nBlocks - 1];
    } else {
        for (int p = 0; p < nBlocks; ++p) {
            blockStart[p] = (n / nBlocks) * p;
            blockSize[p] = (p != nBlocks - 1) ? n / nBlocks : n - (n / nBlocks) * (nBlocks - 1);
        }
    }
}

static int qlfc_encode_block(const uint8_t *in, uint8_t *out, uint8_t *buf, int n, int cap, int coder, qlfc_model *m, long *last) {
    return coder == KOLMO_QLFC_ADAPTIVE ? qlfc_adaptive_encode(in, out, buf, n, cap, m, last) : qlfc_static_encode(in, out, buf, n, cap, m, last);
}

typedef struct {
    const uint8_t *in; uint8_t *scratch; int coder;
    const int *start, *size; const int *soff; int *result; long *last; qlfc_model **models; uint8_t **bufs;
} qlfc_enc_job;

static void qlfc_enc_task(void *p, int b, int worker) {
    qlfc_enc_job *j = (qlfc_enc_job *)p;
    j->result[b] = qlfc_encode_block(j->in + j->start[b], j->scratch + j->soff[b], j->bufs[worker], j->size[b], j->size[b],
                                     j->coder, j->models[worker], &j->last[b]);
}

int kolmo_qlfc_compress(const uint8_t *in, uint8_t *out, int n, int coder, int threads) {
    return kolmo_qlfc_compress_min(in, out, n, coder, 0, threads);
}

int kolmo_qlfc_compress_min(const uint8_t *in, uint8_t *out, int n, int coder, int min_block, int threads) {
    if (coder != KOLMO_QLFC_STATIC && coder != KOLMO_QLFC_ADAPTIVE) return KB_BAD_PARAMETER;
    if (n < 1) return KB_NOT_COMPRESSIBLE;
    int nBlocks = kolmo_qlfc_num_blocks(n);
    if (min_block > 0 && n / min_block < nBlocks) nBlocks = n / min_block > 1 ? n / min_block : 1;
    if (nBlocks == 1) {
        qlfc_model *m = (qlfc_model *)malloc(sizeof(qlfc_model));
        uint8_t *buf = (uint8_t *)malloc((size_t)n);
        long last;
        int r = m && buf ? qlfc_encode_block(in, out + 1, buf, n, n - 1, coder, m, &last) : KB_NOT_ENOUGH_MEMORY;
        free(m); free(buf);
        if (r >= 0) { out[0] = 1; r += 1; }
        return r;
    }
    int start[256], size[256], soff[256], result[256]; long last[256];
    qlfc_split_blocks(in, n, nBlocks, start, size);
    int maxBlock = 0; long total = 0;
    for (int b = 0; b < nBlocks; b++) { soff[b] = (int)total; total += size[b] + 64; if (size[b] > maxBlock) maxBlock = size[b]; }
    /* + 64: the coder checks its capacity once per run, so the last run and the flush may pass it by a few bytes. */
    if (threads > nBlocks) threads = nBlocks;
    if (threads < 1) threads = 1;
    /* Each block is coded on its own into scratch with its own size as capacity (the libbsc serial coder resets the
       model per block). libbsc codes them in order with capacity min(size, n - written); its range coder's overflow
       check sees a non-decreasing position, so that run succeeds exactly when the last position checked here is below
       that capacity - 16, and then writes the same bytes. */
    uint8_t *scratch = (uint8_t *)malloc((size_t)total);
    qlfc_model *models[256] = {0}; uint8_t *bufs[256] = {0};
    int ok = scratch != NULL;
    for (int t = 0; t < threads && ok; t++) {
        models[t] = (qlfc_model *)malloc(sizeof(qlfc_model)); bufs[t] = (uint8_t *)malloc((size_t)maxBlock);
        ok = models[t] && bufs[t];
    }
    if (ok) {
        qlfc_enc_job j = {in, scratch, coder, start, size, soff, result, last, models, bufs};
        kolmo_par_for(threads, nBlocks, qlfc_enc_task, &j);
    }
    for (int t = 0; t < threads; t++) { free(models[t]); free(bufs[t]); }
    if (!ok) { free(scratch); return KB_NOT_ENOUGH_MEMORY; }

    int outputPtr = 1 + 8 * nBlocks;
    out[0] = (uint8_t)nBlocks;
    for (int b = 0; b < nBlocks; b++) {
        int inputSize = size[b], cap = inputSize; if (cap > n - outputPtr) cap = n - outputPtr;
        int r = result[b];
        if (r == KB_NOT_ENOUGH_MEMORY) { free(scratch); return r; }
        if (r >= 0 && last[b] < (long)cap - 16) memcpy(out + outputPtr, scratch + soff[b], (size_t)r);
        else {
            if (outputPtr + inputSize >= n) { outputPtr = KB_NOT_COMPRESSIBLE; break; }
            r = inputSize; memcpy(out + outputPtr, in + start[b], (size_t)inputSize);
        }
        kb_st32(out + 1 + 8 * b, (uint32_t)inputSize);
        kb_st32(out + 1 + 8 * b + 4, (uint32_t)r);
        outputPtr += r;
    }
    free(scratch);
    return outputPtr;
}

static int qlfc_decode_block(const uint8_t *in, const uint8_t *in_end, uint8_t *out, int cap, int coder, qlfc_model *m) {
    return coder == KOLMO_QLFC_ADAPTIVE ? qlfc_adaptive_decode(in, in_end, out, cap, m) : qlfc_static_decode(in, in_end, out, cap, m);
}

typedef struct {
    const uint8_t *in; uint8_t *out; int coder;
    const int *in_off, *in_size, *out_off, *out_size; int *result; qlfc_model **models;
} qlfc_dec_job;

static void qlfc_dec_task(void *p, int b, int worker) {
    qlfc_dec_job *j = (qlfc_dec_job *)p;
    int r;
    if (j->in_size[b] != j->out_size[b]) {
        if (!j->models[worker]) j->models[worker] = (qlfc_model *)malloc(sizeof(qlfc_model));
        r = j->models[worker] ? qlfc_decode_block(j->in + j->in_off[b], j->in + j->in_off[b] + j->in_size[b], j->out + j->out_off[b],
                                                  j->out_size[b], j->coder, j->models[worker]) : KB_NOT_ENOUGH_MEMORY;
    } else {
        memcpy(j->out + j->out_off[b], j->in + j->in_off[b], (size_t)j->in_size[b]); r = j->in_size[b];
    }
    if (r >= 0 && r != j->out_size[b]) r = KB_DATA_CORRUPT;
    j->result[b] = r;
}

int kolmo_qlfc_decompress(const uint8_t *in, int in_size, uint8_t *out, int out_cap, int coder, int threads) {
    if (coder != KOLMO_QLFC_STATIC && coder != KOLMO_QLFC_ADAPTIVE) return KB_DATA_CORRUPT;
    if (in_size < 1) return KB_UNEXPECTED_EOB;
    int nBlocks = in[0];
    if (nBlocks == 1) {
        qlfc_model *m = (qlfc_model *)malloc(sizeof(qlfc_model));
        if (!m) return KB_NOT_ENOUGH_MEMORY;
        int r = qlfc_decode_block(in + 1, in + in_size, out, out_cap, coder, m);
        free(m);
        return r;
    }
    if (nBlocks == 0 || in_size < 1 + 8 * nBlocks) return KB_DATA_CORRUPT;
    int in_off[256], insz[256], out_off[256], outsz[256], result[256];
    long ip = 1 + 8 * nBlocks, op = 0;
    for (int b = 0; b < nBlocks; b++) {
        int32_t o = (int32_t)kb_ld32(in + 1 + 8 * b), c = (int32_t)kb_ld32(in + 1 + 8 * b + 4);
        if (o < 0 || c < 0 || ip + c > in_size || op + o > out_cap) return KB_DATA_CORRUPT;
        in_off[b] = (int)ip; insz[b] = c; out_off[b] = (int)op; outsz[b] = o;
        ip += c; op += o;
    }
    if (threads > nBlocks) threads = nBlocks;
    if (threads < 1) threads = 1;
    qlfc_model *models[256] = {0};
    qlfc_dec_job j = {in, out, coder, in_off, insz, out_off, outsz, result, models};
    kolmo_par_for(threads, nBlocks, qlfc_dec_task, &j);
    for (int t = 0; t < threads; t++) free(models[t]);
    for (int b = 0; b < nBlocks; b++) if (result[b] < 0) return result[b];
    return (int)op;
}
