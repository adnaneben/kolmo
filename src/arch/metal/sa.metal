/* Suffix array and BWT on the GPU: prefix doubling over a stable LSD radix sort. Apache-2.0, Kolmo contributors.
 *
 * The C reference is libsais (the suffix array is unique, so the GPU result must equal it bit for bit); see
 * sa_metal.m for the host side and the algorithm, tests/test_gpu_sa.c for the comparison.
 *
 * Kernels are compiled at run time from this source (the command line tools ship no offline Metal compiler). */
#include <metal_stdlib>
using namespace metal;

#define TG   256u                 /* threads per threadgroup */
#define IPT  16u                  /* items per thread in the scan kernels */
#define TILE (TG * IPT)           /* elements per threadgroup in the scan kernels */
#define SIPT 8u                   /* items per thread in the sort kernels */
#define STILE (TG * SIPT)         /* elements per threadgroup in the sort kernels (the tile is staged in threadgroup memory) */
#define NSG  (TG / 32u)           /* simdgroups per threadgroup (Apple GPUs run 32 lanes) */

/* ------------------------------------------------------------------ radix sort: 64-bit keys, 32-bit values */

/* digit peers: the lanes of the simdgroup whose digit equals this lane's (eight ballots, one per digit bit) */
static ulong peers_of(uint d, bool valid) {
    ulong peers = (ulong)(simd_vote::vote_t)simd_ballot(valid);
    for (uint b = 0; b < 8; b++) {
        bool bit = (d >> b) & 1u;
        ulong v = (ulong)(simd_vote::vote_t)simd_ballot(bit);
        peers &= bit ? v : ~v;
    }
    return peers;
}

/* per-tile digit counts, written digit-major: counts[d * ntiles + tile]. Lanes with the same digit are counted
 * together (one atomic per distinct digit and simdgroup): on sorted or low-entropy keys most lanes share a digit. */
kernel void rs_hist(device const ulong *keys [[buffer(0)]], device uint *counts [[buffer(1)]],
                    constant uint &n [[buffer(2)]], constant uint &shift [[buffer(3)]], constant uint &ntiles [[buffer(4)]],
                    uint tid [[thread_position_in_threadgroup]], uint tg [[threadgroup_position_in_grid]],
                    uint lane [[thread_index_in_simdgroup]], uint sg [[simdgroup_index_in_threadgroup]]) {
    threadgroup atomic_uint h[256];
    atomic_store_explicit(&h[tid], 0u, memory_order_relaxed);
    threadgroup_barrier(mem_flags::mem_threadgroup);
    const ulong below = (1ul << lane) - 1ul;
    const uint first = tg * STILE + sg * (STILE / NSG);
    for (uint k = 0; k < SIPT; k++) {
        uint i = first + k * 32u + lane;
        bool valid = i < n;
        uint d = valid ? (uint)(keys[i] >> shift) & 255u : 0u;
        ulong peers = peers_of(d, valid);
        if (valid && popcount(peers & below) == 0) atomic_fetch_add_explicit(&h[d], popcount(peers), memory_order_relaxed);
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    counts[tid * ntiles + tg] = atomic_load_explicit(&h[tid], memory_order_relaxed);
}

/* stable scatter of one tile. Each simdgroup takes a contiguous eighth of the tile, 32 elements at a time, and ranks
 * them with ballots against its own digit counters (no threadgroup barrier per round). The tile is then sorted by
 * digit in threadgroup memory and written out in that order, so neighbouring threads write neighbouring addresses.
 * offs holds the exclusive scan of rs_hist's counts. */
kernel void rs_scatter(device const ulong *kin [[buffer(0)]], device const uint *vin [[buffer(1)]],
                       device ulong *kout [[buffer(2)]], device uint *vout [[buffer(3)]],
                       device const uint *offs [[buffer(4)]],
                       constant uint &n [[buffer(5)]], constant uint &shift [[buffer(6)]], constant uint &ntiles [[buffer(7)]],
                       uint tid [[thread_position_in_threadgroup]], uint tg [[threadgroup_position_in_grid]],
                       uint lane [[thread_index_in_simdgroup]], uint sg [[simdgroup_index_in_threadgroup]]) {
    threadgroup ushort cnt[NSG][256];
    threadgroup uint gbase[256], dsum[NSG];
    threadgroup ulong tk[STILE];
    threadgroup uint tv[STILE];
    for (uint s = 0; s < NSG; s++) cnt[s][tid] = 0;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    const ulong below = (1ul << lane) - 1ul;
    const uint first = tg * STILE + sg * (STILE / NSG);
    const uint count = min(STILE, n - tg * STILE);
    ulong key[SIPT]; uint val[SIPT]; ushort rk[SIPT];
    for (uint k = 0; k < SIPT; k++) {
        uint i = first + k * 32u + lane;
        bool valid = i < n;
        key[k] = valid ? kin[i] : 0ul; val[k] = valid ? vin[i] : 0u;
        uint d = (uint)(key[k] >> shift) & 255u;
        ulong peers = peers_of(d, valid);
        uint before = popcount(peers & below), c = cnt[sg][d];
        rk[k] = (ushort)(c + before);
        simdgroup_barrier(mem_flags::mem_threadgroup);
        if (valid && before == 0) cnt[sg][d] = (ushort)(c + popcount(peers));
        simdgroup_barrier(mem_flags::mem_threadgroup);
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    /* thread tid owns digit tid: tile-local start of the digit (scan over digits), then of each simdgroup's part */
    uint tot = 0;
    for (uint s = 0; s < NSG; s++) tot += cnt[s][tid];
    uint ex = simd_prefix_exclusive_sum(tot);
    if (lane == 31) dsum[sg] = ex + tot;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    uint start = ex;
    for (uint g = 0; g < sg; g++) start += dsum[g];
    gbase[tid] = offs[tid * ntiles + tg] - start;
    uint run = start;
    for (uint s = 0; s < NSG; s++) { uint c = cnt[s][tid]; cnt[s][tid] = (ushort)run; run += c; }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    for (uint k = 0; k < SIPT; k++) {
        uint i = first + k * 32u + lane;
        if (i < n) {
            uint lp = cnt[sg][(uint)(key[k] >> shift) & 255u] + rk[k];
            tk[lp] = key[k]; tv[lp] = val[k];
        }
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    for (uint j = tid; j < count; j += TG) {
        ulong kk = tk[j];
        uint dst = gbase[(uint)(kk >> shift) & 255u] + j;
        kout[dst] = kk; vout[dst] = tv[j];
    }
}

/* ------------------------------------------------------------------ exclusive scan of u32 (three phases) */

static uint tile_sum(device const uint *a, uint n, uint start, uint tid, uint lane, uint sg, threadgroup uint *sums) {
    uint s = 0, i0 = start + tid * IPT;
    for (uint k = 0; k < IPT; k++) if (i0 + k < n) s += a[i0 + k];
    s = simd_sum(s);
    if (lane == 0) sums[sg] = s;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    uint t = 0;
    for (uint g = 0; g < NSG; g++) t += sums[g];
    return t;
}

kernel void scan_reduce(device const uint *a [[buffer(0)]], device uint *part [[buffer(1)]], constant uint &n [[buffer(2)]],
                        uint tid [[thread_position_in_threadgroup]], uint tg [[threadgroup_position_in_grid]],
                        uint lane [[thread_index_in_simdgroup]], uint sg [[simdgroup_index_in_threadgroup]]) {
    threadgroup uint sums[NSG];
    uint t = tile_sum(a, n, tg * TILE, tid, lane, sg, sums);
    if (tid == 0) part[tg] = t;
}

/* one threadgroup of 1024 threads: exclusive scan of the m tile sums in place; part[m] receives the total */
kernel void scan_partials(device uint *part [[buffer(0)]], constant uint &m [[buffer(1)]],
                          uint tid [[thread_position_in_threadgroup]], uint lane [[thread_index_in_simdgroup]],
                          uint sg [[simdgroup_index_in_threadgroup]]) {
    threadgroup uint sums[32];
    uint per = (m + 1023u) / 1024u, i0 = tid * per, s = 0;
    for (uint k = 0; k < per; k++) if (i0 + k < m) s += part[i0 + k];
    uint ex = simd_prefix_exclusive_sum(s);
    if (lane == 31) sums[sg] = ex + s;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    if (sg == 0) { uint v = sums[lane]; uint e = simd_prefix_exclusive_sum(v); sums[lane] = e; }
    threadgroup_barrier(mem_flags::mem_threadgroup);
    uint run = sums[sg] + ex;
    for (uint k = 0; k < per; k++) if (i0 + k < m) { uint v = part[i0 + k]; part[i0 + k] = run; run += v; }
    if (tid == 1023u) part[m] = run;
}

kernel void scan_down(device const uint *a [[buffer(0)]], device uint *out [[buffer(1)]], device const uint *part [[buffer(2)]],
                      constant uint &n [[buffer(3)]],
                      uint tid [[thread_position_in_threadgroup]], uint tg [[threadgroup_position_in_grid]],
                      uint lane [[thread_index_in_simdgroup]], uint sg [[simdgroup_index_in_threadgroup]]) {
    threadgroup uint sums[NSG];
    uint i0 = tg * TILE + tid * IPT, v[IPT], s = 0;
    for (uint k = 0; k < IPT; k++) { v[k] = i0 + k < n ? a[i0 + k] : 0u; s += v[k]; }
    uint ex = simd_prefix_exclusive_sum(s);
    if (lane == 31) sums[sg] = ex + s;
    threadgroup_barrier(mem_flags::mem_threadgroup);
    uint run = part[tg] + ex;
    for (uint g = 0; g < sg; g++) run += sums[g];
    for (uint k = 0; k < IPT; k++) if (i0 + k < n) { out[i0 + k] = run; run += v[k]; }
}

/* ------------------------------------------------------------------ prefix doubling */

/* which key bits vary: OR and AND of all keys, as four 32-bit words (or lo, or hi, and lo, and hi) */
static void key_bits(bool valid, ulong key, device atomic_uint *orand) {
    uint lo = (uint)key, hi = (uint)(key >> 32);
    uint olo = simd_or(valid ? lo : 0u), ohi = simd_or(valid ? hi : 0u);
    uint alo = simd_and(valid ? lo : ~0u), ahi = simd_and(valid ? hi : ~0u);
    if (simd_is_first()) {
        atomic_fetch_or_explicit(&orand[0], olo, memory_order_relaxed); atomic_fetch_or_explicit(&orand[1], ohi, memory_order_relaxed);
        atomic_fetch_and_explicit(&orand[2], alo, memory_order_relaxed); atomic_fetch_and_explicit(&orand[3], ahi, memory_order_relaxed);
    }
}

/* initial keys: the first eight bytes, big-endian, zero padded past the end (t has 8 readable bytes past n) */
kernel void sa_init_keys(device const uchar *t [[buffer(0)]], device ulong *k [[buffer(1)]], device uint *v [[buffer(2)]],
                         device atomic_uint *orand [[buffer(3)]], device uint *pos [[buffer(4)]], constant uint &n [[buffer(5)]],
                         uint i [[thread_position_in_grid]]) {
    bool valid = i < n;
    ulong x = 0;
    if (valid) {
        for (uint j = 0; j < 8; j++) x = (x << 8) | (i + j < n ? (ulong)t[i + j] : 0ul);
        k[i] = x; v[i] = i; pos[i] = i;
    }
    key_bits(valid, x, orand);
}

/* after a sort of m active elements (keys k, suffixes v), whose SA positions are pos[0..m): write the suffixes into
 * the suffix array and flag the first element of each new group */
kernel void sa_apply(device const ulong *k [[buffer(0)]], device const uint *v [[buffer(1)]], device const uint *pos [[buffer(2)]],
                     device uint *sa [[buffer(3)]], device uint *flag [[buffer(4)]], constant uint &m [[buffer(5)]],
                     uint x [[thread_position_in_grid]]) {
    if (x >= m) return;
    sa[pos[x]] = v[x];
    flag[x] = x == 0 || k[x] != k[x - 1] ? 1u : 0u;
}

/* group heads: head[g] = SA position of the first element of group g (ex: exclusive scan of flag) */
kernel void sa_heads(device const uint *flag [[buffer(0)]], device const uint *ex [[buffer(1)]], device const uint *pos [[buffer(2)]],
                     device uint *head [[buffer(3)]], constant uint &m [[buffer(4)]], uint x [[thread_position_in_grid]]) {
    if (x >= m || !flag[x]) return;
    head[ex[x]] = pos[x];
}

/* new ranks (the group head's position) into isa; keep[x] = 1 for elements of groups of two or more */
kernel void sa_rank(device const uint *flag [[buffer(0)]], device const uint *ex [[buffer(1)]], device const uint *head [[buffer(2)]],
                    device const uint *v [[buffer(3)]], device uint *isa [[buffer(4)]], device uint *keep [[buffer(5)]],
                    constant uint &m [[buffer(6)]], uint x [[thread_position_in_grid]]) {
    if (x >= m) return;
    uint f = flag[x];
    isa[v[x]] = head[ex[x] + f - 1u];
    bool single = f && (x + 1 == m || flag[x + 1]);
    keep[x] = single ? 0u : 1u;
}

/* compaction of the SA positions of the elements still in groups of two or more */
kernel void sa_compact(device const uint *keep [[buffer(0)]], device const uint *ex [[buffer(1)]], device const uint *pos [[buffer(2)]],
                       device uint *npos [[buffer(3)]], constant uint &m [[buffer(4)]], uint x [[thread_position_in_grid]]) {
    if (x >= m || !keep[x]) return;
    npos[ex[x]] = pos[x];
}

/* keys of the next doubling step for the m active elements: (rank of i, rank of i + h); past the end the second
 * part is below every rank and decreasing with i + h (a shorter suffix sorts first) */
kernel void sa_keys(device const uint *pos [[buffer(0)]], device const uint *sa [[buffer(1)]], device const uint *isa [[buffer(2)]],
                    device ulong *k [[buffer(3)]], device uint *v [[buffer(4)]], device atomic_uint *orand [[buffer(5)]],
                    device const uint *mp [[buffer(6)]], constant uint &n [[buffer(7)]], constant uint &h [[buffer(8)]],
                    constant uint &sbits [[buffer(9)]], uint x [[thread_position_in_grid]]) {
    ulong key = 0; bool valid = x < mp[0];
    if (valid) {
        uint i = sa[pos[x]];
        ulong q = (ulong)i + h;
        uint sec = q < n ? isa[(uint)q] + h : n - 1u - i;
        key = ((ulong)isa[i] << sbits) | sec;
        k[x] = key; v[x] = i;
    }
    key_bits(valid, key, orand);
}

/* copy one value (the total of a scan) to a small result buffer */
kernel void copy_total(device const uint *part [[buffer(0)]], device uint *dst [[buffer(1)]], constant uint &at [[buffer(2)]],
                       uint x [[thread_position_in_grid]]) {
    if (x == 0) dst[0] = part[at];
}

/* the BWT in libsais' convention: u[0] = t[n-1], then t[sa[p] - 1] for every p except p0 (where sa[p0] = 0) */
kernel void sa_bwt(device const uchar *t [[buffer(0)]], device const uint *sa [[buffer(1)]], device uchar *u [[buffer(2)]],
                   constant uint &n [[buffer(3)]], constant uint &p0 [[buffer(4)]], uint p [[thread_position_in_grid]]) {
    if (p >= n) return;
    if (p == 0) u[0] = t[n - 1];
    if (p == p0) return;
    u[p < p0 ? p + 1 : p] = t[sa[p] - 1];
}
