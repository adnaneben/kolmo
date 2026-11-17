/* gpu2 kernels: table front end, histograms, wide interleaved rANS (krw), formula search. Apache-2.0, Kolmo
 * contributors. The C references in gpu2_ref.c are the specification (gpu2.h); every kernel gives the same bytes.
 * Integer arithmetic only. Research code, not in the default build. */
#include <metal_stdlib>
using namespace metal;

#define LTILE 4096u

/* ================= generic scans (one threadgroup of 1024 threads) ================= */
/* exclusive prefix sums of v[0..n) into pre[0..n], pre[n] = total */
kernel void scan_u32(device const uint *v [[buffer(0)]], device uint *pre [[buffer(1)]], constant uint &n [[buffer(2)]],
                     uint tid [[thread_position_in_threadgroup]], uint lane [[thread_index_in_simdgroup]],
                     uint sg [[simdgroup_index_in_threadgroup]]) {
    threadgroup uint part[32];
    uint per = (n + 1023) / 1024, b = min(n, tid * per), e = min(n, b + per), s = 0;
    for (uint i = b; i < e; i++) s += v[i];
    uint inc = simd_prefix_inclusive_sum(s);
    if (lane == 31) part[sg] = inc;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (sg == 0) { uint p = part[lane]; part[lane] = simd_prefix_exclusive_sum(p); }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    uint acc = part[sg] + inc - s;
    for (uint i = b; i < e; i++) { pre[i] = acc; acc += v[i]; }
    if (tid == 1023) pre[n] = acc;
}
kernel void scan_u64(device const ulong *v [[buffer(0)]], device ulong *pre [[buffer(1)]], constant uint &n [[buffer(2)]],
                     uint tid [[thread_position_in_threadgroup]]) {
    threadgroup ulong part[1024];
    uint per = (n + 1023) / 1024, b = min(n, tid * per), e = min(n, b + per);
    ulong s = 0;
    for (uint i = b; i < e; i++) s += v[i];
    part[tid] = s;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (tid == 0) { ulong a = 0; for (uint i = 0; i < 1024; i++) { ulong t = part[i]; part[i] = a; a += t; } }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    ulong acc = part[tid];
    for (uint i = b; i < e; i++) { pre[i] = acc; acc += v[i]; }
    if (tid == 1023) pre[n] = acc;
}

/* ================= table front end ================= */
struct tb_args { uint n, rows, cols, mode, sep, tiles; };

/* Line index: one threadgroup of 256 per tile of 4096 bytes, 16 consecutive bytes per thread (coalesced; any
 * alignment). A newline q with q + 1 < n starts line 1 + (newlines before it). */
static inline uint tb_nl16(device const uchar *d, uint b, uint e) {   /* newlines in [b, e) */
    uint c = 0;
    for (uint q = b; q < e; q++) c += d[q] == '\n';
    return c;
}
kernel void tb_count(device const uchar *d [[buffer(0)]], device uint *cnt [[buffer(1)]], constant tb_args &A [[buffer(2)]],
                     uint tid [[thread_position_in_threadgroup]], uint t [[threadgroup_position_in_grid]],
                     uint lane [[thread_index_in_simdgroup]], uint sg [[simdgroup_index_in_threadgroup]]) {
    threadgroup uint part[8];
    uint lim = A.n - 1, b = min(lim, t * LTILE + tid * 16), e = min(lim, b + 16);
    uint c = simd_sum(tb_nl16(d, b, e));
    if (lane == 0) part[sg] = c;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (tid == 0) { uint s = 0; for (uint k = 0; k < 8; k++) s += part[k]; cnt[t] = s; }
}
kernel void tb_starts(device const uchar *d [[buffer(0)]], device const uint *pre [[buffer(1)]], device uint *ls [[buffer(2)]],
                      constant tb_args &A [[buffer(3)]], uint tid [[thread_position_in_threadgroup]], uint t [[threadgroup_position_in_grid]],
                      uint lane [[thread_index_in_simdgroup]], uint sg [[simdgroup_index_in_threadgroup]]) {
    threadgroup uint part[8];
    uint lim = A.n - 1, b = min(lim, t * LTILE + tid * 16), e = min(lim, b + 16);
    uint c = tb_nl16(d, b, e), x = simd_prefix_exclusive_sum(c);
    if (lane == 31) part[sg] = x + c;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    uint k = 1 + pre[t] + x;
    for (uint j = 0; j < sg; j++) k += part[j];
    if (t == 0 && tid == 0) ls[0] = 0;
    if (t == A.tiles - 1 && tid == 0) ls[pre[A.tiles] + 1] = d[A.n - 1] == '\n' ? A.n : A.n + 1;
    for (uint q = b; q < e; q++) if (d[q] == '\n') ls[k++] = q + 1;
}

static inline bool tb_is_sep(uint mode, uchar c, uchar sep) { return mode == 1 ? (c == ',' || c == ':') : c == sep; }

static inline uchar tb_parse(device const uchar *s, uint n, thread long &v) {
    uint i = 0; bool neg = false;
    v = 0;
    if (n == 0 || n > 40) return 0;
    if (s[0] == '-') { neg = true; i = 1; }
    uint is = i; ulong x = 0;
    while (i < n && s[i] >= '0' && s[i] <= '9') { x = x * 10 + (ulong)(s[i] - '0'); i++; }
    uint ni = i - is, dd = 0;
    if (ni == 0 || (ni > 1 && s[is] == '0')) return 0;
    if (i < n && s[i] == '.') {
        i++; uint fs = i;
        while (i < n && s[i] >= '0' && s[i] <= '9') { x = x * 10 + (ulong)(s[i] - '0'); i++; }
        dd = i - fs; if (dd == 0) return 0;
    }
    if (i != n || ni + dd > 18) return 0;
    if (neg && x == 0) return 0;
    v = neg ? -(long)x : (long)x;
    return (uchar)(0x80 | dd);
}

/* the fused pass: one thread per line */
kernel void tb_fields(device const uchar *d [[buffer(0)]], device const uint *ls [[buffer(1)]], device uint *off [[buffer(2)]],
                      device ushort *len [[buffer(3)]], device uint *hash [[buffer(4)]], device long *val [[buffer(5)]],
                      device uchar *fmt [[buffer(6)]], device uchar *rowflag [[buffer(7)]], constant tb_args &A [[buffer(8)]],
                      uint r [[thread_position_in_grid]]) {
    if (r >= A.rows) return;
    const uint R = A.rows, K = A.cols, mode = A.mode; const uchar sep = (uchar)A.sep;
    uint b = ls[r], e = ls[r + 1] - 1;
    if (mode == 1) { if (b < e && d[b] == '{') b++; if (e > b && d[e - 1] == '}') e--; }
    uint f = 0, fs = b, h = 2166136261u; bool q = false;
    for (uint i = b;; i++) {
        bool end = i >= e;
        uchar c = end ? 0 : d[i];
        if (end || (!q && tb_is_sep(mode, c, sep))) {
            if (f < K) {
                ulong o = (ulong)f * R + r; uint L = i - fs; long v;
                off[o] = fs; len[o] = (ushort)min(L, 65535u); hash[o] = h;
                fmt[o] = tb_parse(d + fs, L, v); val[o] = v;
            }
            f++;
            if (end) break;
            fs = i + 1; h = 2166136261u; continue;
        }
        h = (h ^ c) * 16777619u;
        if (q && mode == 1 && c == '\\' && i + 1 < e) { i++; h = (h ^ d[i]) * 16777619u; continue; }
        if (c == '"') q = !q;
    }
    rowflag[r] = f != K;
    for (; f < K; f++) {
        ulong o = (ulong)f * R + r;
        off[o] = e; len[o] = 0; hash[o] = 2166136261u; fmt[o] = 0; val[o] = 0;
    }
}

/* same-as-above flags and residuals: one thread per field */
kernel void tb_resid(device const uchar *d [[buffer(0)]], device const uint *off [[buffer(1)]], device const ushort *len [[buffer(2)]],
                     device const uint *hash [[buffer(3)]], device const long *val [[buffer(4)]], device const uchar *fmt [[buffer(5)]],
                     device uchar *fl [[buffer(6)]], device long *res [[buffer(7)]], constant tb_args &A [[buffer(8)]],
                     uint2 gid [[thread_position_in_grid]]) {
    uint r = gid.x, c = gid.y;
    if (r >= A.rows || c >= A.cols) return;
    ulong o = (ulong)c * A.rows + r;
    uchar fm = fmt[o], f = (uchar)((fm >> 7) | (fm & 31) << 3);
    long v = val[o], rv = v;
    if (r) {
        ulong p = o - 1;
        ushort L = len[o];
        if (L == len[p] && L < 65535 && hash[o] == hash[p]) {
            device const uchar *x = d + off[o], *y = d + off[p];
            bool eq = true;
            for (uint k = 0; k < L; k++) if (x[k] != y[k]) { eq = false; break; }
            if (eq) f |= 2;
        }
        if ((fm & 0x80) && fm == fmt[p]) { f |= 4; rv = (long)((ulong)v - (ulong)val[p]); }
    }
    fl[o] = f; res[o] = rv;
}

/* ================= histograms ================= */
static inline ulong lg2q16(device const uint *tab, ulong x) {
    if (!x) return 0;
    uint k = 63 - clz(x);
    uint m = (uint)(k >= 10 ? (x >> (k - 10)) & 1023 : (x << (10 - k)) & 1023);
    return ((ulong)k << 16) + tab[m];
}
/* one threadgroup of 256 per segment: eight sub-histograms in threadgroup memory against same-byte runs */
kernel void hist_segs(device const uchar *d [[buffer(0)]], device const ulong *off [[buffer(1)]], device uint *h [[buffer(2)]],
                      device ulong *bits [[buffer(3)]], device const uint *tab [[buffer(4)]],
                      uint tid [[thread_position_in_threadgroup]], uint s [[threadgroup_position_in_grid]],
                      uint sg [[simdgroup_index_in_threadgroup]]) {
    threadgroup atomic_uint th[8][256];
    threadgroup ulong part[256];
    for (uint k = 0; k < 8; k++) atomic_store_explicit(&th[k][tid], 0, memory_order_relaxed);
    threadgroup_barrier(mem_flags::mem_threadgroup);
    ulong b = off[s], m = off[s + 1] - b;
    device const uchar *p = d + b;
    ulong head = min(m, (ulong)((4 - (b & 3)) & 3));
    if (tid < head) atomic_fetch_add_explicit(&th[sg][p[tid]], 1, memory_order_relaxed);
    device const uint *w = (device const uint *)(p + head);
    ulong nw = (m - head) / 4;
    for (ulong i = tid; i < nw; i += 256) {
        uint x = w[i];
        atomic_fetch_add_explicit(&th[sg][x & 255], 1, memory_order_relaxed);
        atomic_fetch_add_explicit(&th[sg][(x >> 8) & 255], 1, memory_order_relaxed);
        atomic_fetch_add_explicit(&th[sg][(x >> 16) & 255], 1, memory_order_relaxed);
        atomic_fetch_add_explicit(&th[sg][x >> 24], 1, memory_order_relaxed);
    }
    ulong tail = head + nw * 4;
    if (tail + tid < m) atomic_fetch_add_explicit(&th[sg][p[tail + tid]], 1, memory_order_relaxed);
    threadgroup_barrier(mem_flags::mem_threadgroup);
    uint c = 0;
    for (uint k = 0; k < 8; k++) c += atomic_load_explicit(&th[k][tid], memory_order_relaxed);
    h[(ulong)s * 256 + tid] = c;
    part[tid] = c ? (ulong)c * (lg2q16(tab, m) - lg2q16(tab, c)) : 0;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    for (uint st = 128; st > 0; st >>= 1) { if (tid < st) part[tid] += part[tid + st]; threadgroup_barrier(mem_flags::mem_threadgroup); }
    if (tid == 0) bits[s] = part[0];
}
/* order-1: G partial tables in device memory (one per threadgroup group), then a sum per bin */
kernel void hist_o1_part(device const uchar *d [[buffer(0)]], device atomic_uint *parts [[buffer(1)]], constant ulong &n [[buffer(2)]],
                         constant uint &G [[buffer(3)]], uint tid [[thread_position_in_threadgroup]], uint g [[threadgroup_position_in_grid]],
                         uint tgs [[threadgroups_per_grid]]) {
    ulong per = (n + tgs - 1) / tgs, b = (ulong)g * per, e = min(n, b + per);
    device atomic_uint *tb = parts + (ulong)(g % G) * 65536;
    for (ulong i = max(b, (ulong)1) + tid; i < e; i += 256)
        atomic_fetch_add_explicit(&tb[(uint)d[i - 1] << 8 | d[i]], 1, memory_order_relaxed);
}
kernel void hist_o1_sum(device const uint *parts [[buffer(0)]], device uint *h [[buffer(1)]], constant uint &G [[buffer(2)]],
                        uint i [[thread_position_in_grid]]) {
    uint s = 0;
    for (uint g = 0; g < G; g++) s += parts[(ulong)g * 65536 + i];
    h[i] = s;
}

/* ================= krw: wide interleaved rANS ================= */
#define KRW_M 4096u
#define KRW_LOW 65536u
kernel void krw_norm_k(device const uint *h [[buffer(0)]], device const ulong *off [[buffer(1)]], device ushort *ftab [[buffer(2)]],
                       constant uint &nseg [[buffer(3)]], uint s [[thread_position_in_grid]]) {
    if (s >= nseg) return;
    device const uint *cnt = h + (ulong)s * 256; device ushort *f = ftab + (ulong)s * 256;
    ulong n = off[s + 1] - off[s];
    for (uint k = 0; k < 256; k++) f[k] = 0;
    if (!n) return;
    uint sum = 0; int mx = -1;
    for (uint k = 0; k < 256; k++) {
        uint c = cnt[k]; if (!c) continue;
        uint v = (uint)(((ulong)c * KRW_M) / n);
        uint fv = v ? v : 1; f[k] = (ushort)fv; sum += fv;
        if (mx < 0 || c > cnt[mx]) mx = (int)k;
    }
    if (sum < KRW_M) { f[mx] = (ushort)(f[mx] + (KRW_M - sum)); return; }
    if (sum > KRW_M) { uint dd = sum - KRW_M, r = f[mx] - 1u; if (dd > r) dd = r; f[mx] = (ushort)(f[mx] - dd); sum -= dd; }
    for (uint k = 0; k < 256 && sum > KRW_M; k++)
        if (f[k] > 1) { uint dd = sum - KRW_M, r = f[k] - 1u; if (dd > r) dd = r; f[k] = (ushort)(f[k] - dd); sum -= dd; }
}
struct krw_args { uint nseg, L, cap; };
/* one threadgroup (L threads) per segment; lane words go to scratch, interleaved by lane, written backwards */
kernel void krw_enc(device const uchar *d [[buffer(0)]], device const ulong *off [[buffer(1)]], device const ushort *ftab [[buffer(2)]],
                    device ushort *scratch [[buffer(3)]], device uint *st [[buffer(4)]], device uint *cnt [[buffer(5)]],
                    constant krw_args &A [[buffer(6)]], uint l [[thread_position_in_threadgroup]], uint s [[threadgroup_position_in_grid]]) {
    threadgroup uint F[256], C[256], M[256];
    const uint L = A.L;
    for (uint k = l; k < 256; k += L) { uint f = ftab[(ulong)s * 256 + k]; F[k] = f; M[k] = f > 1 ? (uint)(0x100000000ul / f) : 0; }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (l == 0) { uint c = 0; for (uint k = 0; k < 256; k++) { C[k] = c; c += F[k]; } }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    ulong b = off[s]; uint m = (uint)(off[s + 1] - b);
    uint cntl = l < m ? (m - l + L - 1) / L : 0, x = KRW_LOW, k = 0;
    device ushort *sc = scratch + (ulong)s * A.cap * L + l;
    uint e = A.cap;
    for (uint j = cntl; j-- > 0;) {
        uchar sym = d[b + (ulong)j * L + l]; uint fs = F[sym];
        if (fs < KRW_M && x >= (fs << 20)) { e--; sc[(ulong)e * L] = (ushort)x; x >>= 16; k++; }
        /* x / fs without a division: q = mulhi(x, floor(2^32 / fs)) is at most 2 below the quotient */
        uint q = fs > 1 ? mulhi(x, M[sym]) : x, r = x - q * fs;
        while (r >= fs) { q++; r -= fs; }
        x = (q << 12) + r + C[sym];
    }
    st[(ulong)s * L + l] = x; cnt[(ulong)s * L + l] = k;
}
/* per segment: prefix of lane counts, coded size */
kernel void krw_sizes(device const uint *cnt [[buffer(0)]], device uint *lpre [[buffer(1)]], device ulong *ssz [[buffer(2)]],
                      constant krw_args &A [[buffer(3)]], uint s [[thread_position_in_grid]]) {
    if (s >= A.nseg) return;
    uint a = 0;
    for (uint l = 0; l < A.L; l++) { lpre[(ulong)s * A.L + l] = a; a += cnt[(ulong)s * A.L + l]; }
    ssz[s] = 512 + 8 * (ulong)A.L + 2 * (ulong)a;
}
static inline void put32(device ushort *p, uint v) { p[0] = (ushort)v; p[1] = (ushort)(v >> 16); }
kernel void krw_pack(device const ushort *ftab [[buffer(0)]], device const ushort *scratch [[buffer(1)]], device const uint *st [[buffer(2)]],
                     device const uint *cnt [[buffer(3)]], device const uint *lpre [[buffer(4)]], device const ulong *sout [[buffer(5)]],
                     device uchar *out [[buffer(6)]], constant krw_args &A [[buffer(7)]],
                     uint l [[thread_position_in_threadgroup]], uint s [[threadgroup_position_in_grid]]) {
    const uint L = A.L;
    device ushort *o = (device ushort *)(out + sout[s]);
    for (uint k = l; k < 256; k += L) o[k] = ftab[(ulong)s * 256 + k];
    ulong i = (ulong)s * L + l;
    put32(o + 256 + 2 * l, st[i]); put32(o + 256 + 2 * L + 2 * l, cnt[i]);
    device ushort *w = o + 256 + 4 * L + lpre[i];
    device const ushort *sc = scratch + (ulong)s * A.cap * L + l;
    uint k = cnt[i], e = A.cap - k;
    for (uint j = 0; j < k; j++) w[j] = sc[(ulong)(e + j) * L];
}
/* decode: one threadgroup (L threads) per segment */
kernel void krw_dec(device const uchar *code [[buffer(0)]], device const ulong *sout [[buffer(1)]], device const ulong *off [[buffer(2)]],
                    device uchar *dst [[buffer(3)]], constant krw_args &A [[buffer(4)]],
                    uint l [[thread_position_in_threadgroup]], uint s [[threadgroup_position_in_grid]]) {
    threadgroup ushort F[256], C[256];
    threadgroup uchar SYM[KRW_M];
    threadgroup uint LP[1024];
    const uint L = A.L;
    device const ushort *o = (device const ushort *)(code + sout[s]);
    for (uint k = l; k < 256; k += L) F[k] = o[k];
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (l == 0) { uint c = 0; for (uint k = 0; k < 256; k++) { C[k] = (ushort)c; c += F[k]; } }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    for (uint k = l; k < 256; k += L) { uint c = C[k], f = F[k]; for (uint q = 0; q < f && c + q < KRW_M; q++) SYM[c + q] = (uchar)k; }
    uint x = (uint)o[256 + 2 * l] | (uint)o[257 + 2 * l] << 16;
    uint kk = (uint)o[256 + 2 * L + 2 * l] | (uint)o[257 + 2 * L + 2 * l] << 16;
    LP[l] = kk;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (l == 0) { uint a = 0; for (uint q = 0; q < L; q++) { uint t = LP[q]; LP[q] = a; a += t; } }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    device const ushort *w = o + 256 + 4 * L + LP[l];
    ulong b = off[s]; uint m = (uint)(off[s + 1] - b);
    uint cntl = l < m ? (m - l + L - 1) / L : 0, p = 0;
    for (uint j = 0; j < cntl; j++) {
        uint slot = x & (KRW_M - 1); uchar sym = SYM[slot];
        x = (uint)F[sym] * (x >> 12) + slot - C[sym];
        if (x < KRW_LOW && p < kk) x = x << 16 | w[p++];
        dst[b + (ulong)j * L + l] = sym;
    }
}

/* ================= formula search (GR6+) ================= */
constant long P10[19] = {1L, 10L, 100L, 1000L, 10000L, 100000L, 1000000L, 10000000L, 100000000L, 1000000000L,
    10000000000L, 100000000000L, 1000000000000L, 10000000000000L, 100000000000000L, 1000000000000000L,
    10000000000000000L, 100000000000000000L, 1000000000000000000L};
constant int KE[5] = {0, 2, 3, 6, 9};
struct u128 { ulong hi, lo; };
static inline u128 umul(ulong a, ulong b) {
    ulong a0 = a & 0xFFFFFFFFul, a1 = a >> 32, b0 = b & 0xFFFFFFFFul, b1 = b >> 32;
    ulong p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    ulong mid = (p00 >> 32) + (p01 & 0xFFFFFFFFul) + (p10 & 0xFFFFFFFFul);
    u128 r; r.hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32); r.lo = (mid << 32) | (p00 & 0xFFFFFFFFul); return r;
}
static inline u128 uadd(u128 a, u128 b) { u128 r; r.lo = a.lo + b.lo; r.hi = a.hi + b.hi + (r.lo < a.lo ? 1ul : 0ul); return r; }
static inline u128 usub(u128 a, u128 b) { u128 r; r.hi = a.hi - b.hi - (a.lo < b.lo ? 1ul : 0ul); r.lo = a.lo - b.lo; return r; }
static inline bool uge(u128 a, u128 b) { return a.hi > b.hi || (a.hi == b.hi && a.lo >= b.lo); }
static inline u128 ushl(u128 a, int s) { if (!s) return a; u128 r; r.hi = a.hi << s | a.lo >> (64 - s); r.lo = a.lo << s; return r; }
static inline bool udiv(u128 a, u128 b, thread ulong &q) {
    if (!a.hi && !b.hi) { q = a.lo / b.lo; return q <= (1ul << 62); }
    if (b.hi < 2 && uge(a, ushl(b, 63))) return false;
    ulong r = 0;
    for (int i = 62; i >= 0; i--) {
        if (i && (b.hi >> (64 - i))) continue;
        u128 t = ushl(b, i);
        if (uge(a, t)) { a = usub(a, t); r |= 1ul << i; }
    }
    q = r; return r <= (1ul << 62);
}
static inline bool fratio(long va, int da, long vb, int db, int ke, int dt, thread long &out) {
    int e = db + dt + ke; if (vb == 0 || e > 18 || da > 18 || e < 0) return false;
    if (va > (1L << 60) || va < -(1L << 60) || vb > (1L << 60) || vb < -(1L << 60)) return false;
    bool neg = (va < 0) != (vb < 0) && va != 0;
    ulong aa = va < 0 ? (ulong)(-va) : (ulong)va, ab = vb < 0 ? (ulong)(-vb) : (ulong)vb;
    u128 num = umul(aa, (ulong)P10[e]), den = umul(ab, (ulong)P10[da]);
    ulong q;
    if (!udiv(uadd(ushl(num, 1), den), ushl(den, 1), q)) return false;
    out = neg ? -(long)q : (long)q; return true;
}
static inline bool frescale(long v, int from, int to, thread long &out) {
    if (to >= from) { int e = to - from; if (e > 18) return false; long lim = 0x7FFFFFFFFFFFFFFFL / P10[e] / 2; if (v > lim || v < -lim) return false; out = v * P10[e]; return true; }
    int e = from - to; if (e > 18 || v == (-0x7FFFFFFFFFFFFFFFL - 1)) return false; long q = P10[e], a = v < 0 ? -v : v, r = (a + q / 2) / q; out = v < 0 ? -r : r; return true;
}
static inline bool fpred(int type, int ke, long va, int da, bool okl, long vla, int dla, long vb, int db, bool oklt,
                         long vlt, int dlt, int dt, thread long &out) {
    switch (type) {
    case 0: return fratio(va, da, vb, db, ke, dt, out);
    case 1: return okl && da == dla && frescale((long)((ulong)va - (ulong)vla), da, dt, out);
    case 2: return okl && da == dla && fratio((long)((ulong)va - (ulong)vla), da, vla, da, 2, dt, out);
    case 3: { long r; if (!oklt || dlt != dt || !frescale(va, da, dt, r)) return false; out = (long)((ulong)vlt + (ulong)r); return true; }
    case 4: return frescale(va, da, dt, out);
    case 5: {
        if (va > (1L << 40) || va < -(1L << 40) || vb > (1L << 40) || vb < -(1L << 40)) return false;
        int e = da + db + ke - dt; if (e < 0 || e > 18) return false;
        bool neg = (va < 0) != (vb < 0) && va != 0 && vb != 0;
        ulong aa = va < 0 ? (ulong)(-va) : (ulong)va, ab = vb < 0 ? (ulong)(-vb) : (ulong)vb, q = (ulong)P10[e], r;
        u128 m = umul(aa, ab), h, qq; h.hi = 0; h.lo = q / 2; qq.hi = 0; qq.lo = q;
        if (!udiv(uadd(m, h), qq, r)) return false;
        out = neg ? -(long)r : (long)r; return true; }
    }
    return false;
}
struct fm_args { uint R, rows, lim, nc; };
struct kg2_cand { ushort t, a, b, tq; };
struct kg2_score { int hit, seen; };
#define FV(c, r) V[(ulong)(c) * R + (r)]
#define FD(c, r) (int)D[(ulong)(c) * R + (r)]
#define FOK(c, r) OK[(ulong)(c) * R + (r)]
static inline bool fhit(device const long *V, device const uchar *D, device const uchar *OK, uint R, int ty, int t, int a, int b, int ke, uint r) {
    long p;
    if (!fpred(ty, ke, FV(a, r), FD(a, r), FOK(a, r - 1) != 0, FV(a, r - 1), FD(a, r - 1), FV(b, r), FD(b, r),
               FOK(t, r - 1) != 0, FV(t, r - 1), FD(t, r - 1), FD(t, r), p)) return false;
    long v = FV(t, r);
    return p - v <= 2 && v - p <= 2;
}
kernel void fm_screen(device const long *V [[buffer(0)]], device const uchar *D [[buffer(1)]], device const uchar *OK [[buffer(2)]],
                      device const kg2_cand *cand [[buffer(3)]], device kg2_score *out [[buffer(4)]], constant fm_args &A [[buffer(5)]],
                      uint i [[thread_position_in_grid]]) {
    if (i >= A.nc) return;
    const uint R = A.R;
    kg2_cand c = cand[i];
    int t = c.t, a = c.a, b = c.b, ty = c.tq >> 8, ke = KE[c.tq & 255];
    int hit = 0, miss = 0, seen = 0;
    for (uint r = 1; r < A.rows && r < 3000 && seen < 64; r++) {
        if (!FOK(t, r) || !FOK(a, r) || !FOK(b, r)) continue;
        seen++;
        if (fhit(V, D, OK, R, ty, t, a, b, ke, r)) hit++;
        else if (++miss > 24 || (miss >= 6 && hit * 2 < miss)) break;
    }
    bool fail = seen < 16 || miss > 24 || hit * 2 < miss || hit < seen / 2;
    kg2_score s; s.hit = hit; s.seen = fail ? -1 : seen; out[i] = s;
}
kernel void fm_score(device const long *V [[buffer(0)]], device const uchar *D [[buffer(1)]], device const uchar *OK [[buffer(2)]],
                     device const kg2_cand *cand [[buffer(3)]], device kg2_score *out [[buffer(4)]], constant fm_args &A [[buffer(5)]],
                     device const kg2_score *bound [[buffer(6)]], uint i [[thread_position_in_grid]]) {
    if (i >= A.nc) return;
    const uint R = A.R, lim = A.lim;
    kg2_cand c = cand[i];
    int t = c.t, a = c.a, b = c.b, ty = c.tq >> 8, ke = KE[c.tq & 255];
    long bh = bound[t].hit, bs = bound[t].seen;
    int hit = 0, seen = 0; bool cut = false;
    for (uint r = 1; r < lim; r++) {
        if ((r & 63) == 0) {
            long rem = (long)(lim - r);
            if (2 * hit + rem <= seen) break;
            if (bs > 0 && (hit + rem) * bs < bh * (seen + rem)) { cut = true; break; }
        }
        if (!FOK(t, r) || !FOK(a, r) || !FOK(b, r)) continue;
        seen++;
        hit += fhit(V, D, OK, R, ty, t, a, b, ke, r) ? 1 : 0;
    }
    kg2_score s; s.hit = cut ? -1 : hit; s.seen = cut ? -1 : seen; out[i] = s;
}
