/* KRC1 tile payloads are independent. Prefix sums locate output ranges.
 * CRC32C is linear over GF(2), enabling exact concatenation of region CRCs. */
typedef struct {
    size_t first, tiles, raw_bytes, offset, coded_bytes;
    uint32_t raw_crc, coded_crc;
} krc_region;

typedef struct {
    const uint8_t *src;
    uint8_t *dst;
    const krc_plan *plans;
    krc_region *regions;
    unsigned count;
    atomic_uint next;
    atomic_int error;
    int decoding;
} krc_regions;

static uint32_t zero_bytes[64][32];
static pthread_once_t crc_once = PTHREAD_ONCE_INIT;

static uint32_t apply_matrix(const uint32_t *matrix, uint32_t x) {
    uint32_t result = 0;
    while (x) {
        unsigned bit = (unsigned)__builtin_ctz(x);
        result ^= matrix[bit];
        x &= x - 1;
    }
    return result;
}

static void crc_matrices(void) {
    for (unsigned i = 0; i < 32; ++i) {
        uint32_t c = UINT32_C(1) << i;
        for (unsigned b = 0; b < 8; ++b)
            c = (c >> 1) ^ ((0u - (c & 1)) & UINT32_C(0x82f63b78));
        zero_bytes[0][i] = c;
    }
    for (unsigned power = 1; power < 64; ++power)
        for (unsigned i = 0; i < 32; ++i)
            zero_bytes[power][i] = apply_matrix(zero_bytes[power - 1], zero_bytes[power - 1][i]);
}

static uint32_t append_crc(uint32_t before, uint32_t after, size_t length) {
    for (unsigned p = 0; length; ++p, length >>= 1)
        if (length & 1) before = apply_matrix(zero_bytes[p], before);
    return before ^ after;
}

static unsigned cpu_limit(int requested) {
    if (requested <= 0) {
        long n = sysconf(_SC_NPROCESSORS_ONLN);
        requested = n > 256 ? 256 : n > 0 ? (int)n : 1;
    }
    return requested > 256 ? 256 : (unsigned)requested;
}

static int plan_size(krc_plan p, unsigned words, size_t *bytes) {
    if (!valid_plan(p) || p.exceptions > words) return 0;
    unsigned raw = 4 * words;
    unsigned coded = (words * p.width + 7) / 8 + 5 * p.exceptions;
    *bytes = 6 + (!p.mode || coded >= raw ? raw : coded);
    return 1;
}

static int locate_regions(krc_regions *job, size_t raw, size_t frame, size_t *body) {
    size_t tiles = krc_plan_count(raw), words = raw / 4, pos = 32;
    size_t base = tiles / job->count, remainder = tiles % job->count;
    size_t first = 0;
    for (unsigned i = 0; i < job->count; ++i) {
        krc_region *region = &job->regions[i];
        size_t count = base + (i < remainder);
        region->first = first;
        region->tiles = count;
        size_t bytes = raw - raw % 4 - first * 1024;
        if (count < bytes / 1024 || (count == bytes / 1024 && bytes % 1024)) bytes = count * 1024;
        region->raw_bytes = bytes;
        region->offset = pos;
        for (size_t j = first; j < first + count; ++j) {
            size_t remaining = words - j * 256;
            unsigned nw = remaining < 256 ? (unsigned)remaining : 256;
            krc_plan plan;
            if (job->decoding) {
                if (frame - pos < 6) return 0;
                const uint8_t *p = job->src + pos;
                if (p[3]) return 0;
                plan = (krc_plan){p[0],p[1],p[2],(uint32_t)p[4] | (uint32_t)p[5]<<8};
            } else plan = job->plans[j];
            size_t size;
            if (!plan_size(plan, nw, &size) || size > frame - pos) return 0;
            if (job->decoding && plan.mode && size == 6 + nw * 4) return 0;
            pos += size;
        }
        region->coded_bytes = pos - region->offset;
        first += count;
    }
    *body = pos;
    return !job->decoding || frame - pos == raw % 4;
}

static void *process_regions(void *arg) {
    krc_regions *job = arg;
    for (;;) {
        unsigned index = atomic_fetch_add_explicit(&job->next,1,memory_order_relaxed);
        if (index >= job->count) return NULL;
        krc_region *r = &job->regions[index];
        const uint8_t *raw, *coded;
        int64_t used;
        if (job->decoding) {
            uint8_t *dst = job->dst + r->first * 1024;
            coded = job->src + r->offset;
            used = decode_tiles(coded,r->coded_bytes,dst,r->raw_bytes/4,r->tiles);
            raw = dst;
        } else {
            uint8_t *dst = job->dst + r->offset;
            raw = job->src + r->first * 1024;
            used = encode_tiles(raw,r->raw_bytes/4,job->plans+r->first,r->tiles,dst);
            coded = dst;
        }
        if (used < 0 || (size_t)used != r->coded_bytes) {
            atomic_store(&job->error,job->decoding ? KRC_ERR_CORRUPT : KRC_ERR_PLAN);
            return NULL;
        }
        r->raw_crc = crc(raw,r->raw_bytes);
        r->coded_crc = crc(coded,r->coded_bytes);
    }
}

static int64_t parallel_frame(const uint8_t *src, size_t raw, size_t frame, const krc_plan *plans,
                              uint8_t *dst, unsigned threads, int decoding) {
    size_t tiles = krc_plan_count(raw);
    unsigned regions = threads * 4;
    if (regions > 256) regions = 256;
    if (tiles < regions) regions = (unsigned)tiles;
    krc_region data[256];
    krc_regions job = { .src=src,.dst=dst,.plans=plans,.regions=data,.count=regions,.decoding=decoding };
    atomic_init(&job.next,0);atomic_init(&job.error,0);
    size_t tail_pos;
    if (!locate_regions(&job,raw,frame,&tail_pos)) return decoding ? KRC_ERR_CORRUPT : KRC_ERR_PLAN;
    pthread_t helpers[255]; unsigned started = 0;
    for (unsigned i = 1; i < threads && i < regions; ++i) {
        if (pthread_create(&helpers[started],NULL,process_regions,&job)) break;
        ++started;
    }
    process_regions(&job);
    for (unsigned i = 0; i < started; ++i) pthread_join(helpers[i],NULL);
    int error = atomic_load(&job.error);
    if (error) return error;
    pthread_once(&crc_once,crc_matrices);
    uint32_t raw_crc = 0, coded_crc = 0;
    for (unsigned i = 0; i < regions; ++i) {
        raw_crc = append_crc(raw_crc,data[i].raw_crc,data[i].raw_bytes);
        coded_crc = append_crc(coded_crc,data[i].coded_crc,data[i].coded_bytes);
    }
    size_t tail = raw % 4;
    if (tail) {
        const uint8_t *p = decoding ? src + tail_pos : src + raw - tail;
        memcpy(decoding ? dst + raw - tail : dst + tail_pos,p,tail);
        uint32_t partial = crc(p,tail);
        raw_crc = append_crc(raw_crc,partial,tail);
        coded_crc = append_crc(coded_crc,partial,tail);
    }
    if (decoding)
        return raw_crc == rd32(src+20) && coded_crc == rd32(src+24) ? (int64_t)raw : KRC_ERR_CORRUPT;
    memcpy(dst,"KRC1",4); dst[4]=1; dst[5]=0; dst[6]=8; dst[7]=0;
    wr64(dst+8,raw); wr32(dst+16,(uint32_t)tiles); wr32(dst+20,raw_crc);
    wr32(dst+24,coded_crc); wr32(dst+28,crc(dst,28));
    return (int64_t)(tail_pos + tail);
}

int64_t krc_encode_threads(const uint8_t *src, size_t n, const krc_plan *plans, size_t count,
                           uint8_t *dst, size_t cap, int requested) {
    unsigned threads = cpu_limit(requested);
    if (threads == 1 || n < (1u << 20)) return krc_encode(src,n,plans,count,dst,cap);
    if (!krc_bound(n) || cap < krc_bound(n) || !dst) return KRC_ERR_CAPACITY;
    if (!src || count < krc_plan_count(n) || !plans) return KRC_ERR_PLAN;
    return parallel_frame(src,n,cap,plans,dst,threads,0);
}

int64_t krc_decode_threads(const uint8_t *src, size_t n, uint8_t *dst, size_t cap, int requested) {
    unsigned threads = cpu_limit(requested);
    int64_t raw = krc_original_size(src,n);
    if (raw < 0) return raw;
    if (threads == 1 || raw < (1u << 20)) return krc_decompress(src,n,dst,cap);
    if ((uint64_t)raw > cap || !dst) return KRC_ERR_CAPACITY;
    return parallel_frame(src,(size_t)raw,n,NULL,dst,threads,1);
}
