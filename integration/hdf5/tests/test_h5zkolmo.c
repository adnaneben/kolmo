/* Round-trip test of the Kolmo HDF5 filter through the HDF5 C API, with the plugin loaded dynamically from
 * HDF5_PLUGIN_PATH (as users load it): chunked datasets of several types, shapes and levels are written, the file is
 * closed and reopened, and every dataset is read back and compared. Also checks the client data set_local stores, an
 * invalid level, incompressible data, and that a damaged chunk is an error and not a crash.
 *
 *   HDF5_PLUGIN_PATH=build/plugin ./build/test_h5zkolmo [file.h5]
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "hdf5.h"
#include "../H5Zkolmo.h"

static int failures = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static uint64_t rng = 0x9E3779B97F4A7C15ull;
static uint64_t next(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

typedef struct { int32_t id; float t; double v; int16_t q; } rec_t;   /* compound record, 24 bytes with padding */

typedef struct {
    const char *name;
    hid_t mem_type;      /* native type, also used as file type */
    size_t esize;
    int rank;
    hsize_t dims[3], chunk[3];
    unsigned level;      /* 0 = default */
    int compressible;
} case_t;

static void fill(const case_t *c, void *buf, size_t n) {
    switch (c->esize) {
    case 1: { uint8_t *b = buf; for (size_t i = 0; i < n; i++) b[i] = (uint8_t)(128 + 60 * sin(i * 0.01) + (next() & 3)); } break;
    case 2: { int16_t *b = buf; for (size_t i = 0; i < n; i++) b[i] = (int16_t)(800 * sin(i * 0.003) + (int)(next() % 17)); } break;
    case 4:
        if (H5Tequal(c->mem_type, H5T_NATIVE_FLOAT) > 0) {
            float *b = buf;
            for (size_t i = 0; i < n; i++) b[i] = (float)(15.0 + 10.0 * sin(i * 0.002) * cos(i * 0.00031)) + (float)(next() % 100) * 1e-3f;
        } else {
            int32_t *b = buf;
            if (c->compressible) for (size_t i = 0; i < n; i++) b[i] = (int32_t)(i / 7) * 3 + (int32_t)(next() % 5);
            else for (size_t i = 0; i < n; i++) b[i] = (int32_t)next();
        }
        break;
    case 8:
        if (H5Tequal(c->mem_type, H5T_NATIVE_DOUBLE) > 0) {
            double *b = buf; for (size_t i = 0; i < n; i++) b[i] = round((1000.0 + i * 0.25 + 30 * sin(i * 0.01)) * 100) / 100;
        } else {
            int64_t *b = buf; for (size_t i = 0; i < n; i++) b[i] = 1700000000000ll + (int64_t)i * 1000 + (int64_t)(next() % 50);
        }
        break;
    default: {
        rec_t *b = buf;
        memset(buf, 0, n * sizeof(rec_t));
        for (size_t i = 0; i < n; i++) { b[i].id = (int32_t)i; b[i].t = (float)(i * 0.5); b[i].v = sin(i * 0.01); b[i].q = (int16_t)(i % 100); }
    }
    }
}

static hid_t make_rec_type(void) {
    hid_t t = H5Tcreate(H5T_COMPOUND, sizeof(rec_t));
    H5Tinsert(t, "id", HOFFSET(rec_t, id), H5T_NATIVE_INT32);
    H5Tinsert(t, "t", HOFFSET(rec_t, t), H5T_NATIVE_FLOAT);
    H5Tinsert(t, "v", HOFFSET(rec_t, v), H5T_NATIVE_DOUBLE);
    H5Tinsert(t, "q", HOFFSET(rec_t, q), H5T_NATIVE_INT16);
    return t;
}

static size_t count(const case_t *c) { size_t n = 1; for (int i = 0; i < c->rank; i++) n *= c->dims[i]; return n; }

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "build/test_h5zkolmo.h5";
    hid_t rec = make_rec_type();
    case_t cases[] = {
        {"u8_image_2d",    H5T_NATIVE_UINT8,  1, 2, {300, 517},      {128, 128},    0, 1},
        {"i16_sensor_1d",  H5T_NATIVE_INT16,  2, 1, {1000003},       {262144},      1, 1},
        {"i32_counts_1d",  H5T_NATIVE_INT32,  4, 1, {400000},        {100000},      5, 1},
        {"i32_random_1d",  H5T_NATIVE_INT32,  4, 1, {100000},        {65536},       3, 0},
        {"f32_field_3d",   H5T_NATIVE_FLOAT,  4, 3, {40, 64, 100},   {10, 64, 100}, 3, 1},
        {"f64_series_2d",  H5T_NATIVE_DOUBLE, 8, 2, {1000, 97},      {256, 97},     9, 1},
        {"i64_stamps_1d",  H5T_NATIVE_INT64,  8, 1, {200000},        {50000},       7, 1},
        {"rec_compound",   rec,               sizeof(rec_t), 1, {50000}, {16384},   3, 1},
        {"f64_ultra_small",H5T_NATIVE_DOUBLE, 8, 1, {8192},          {8192},       10, 1},
    };
    const int ncases = (int)(sizeof cases / sizeof cases[0]);
    void *data[sizeof cases / sizeof cases[0]];

    if (!getenv("HDF5_PLUGIN_PATH")) printf("note: HDF5_PLUGIN_PATH is not set; the plugin must be in HDF5's default plugin directory\n");

    /* write */
    hid_t f = H5Fcreate(path, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    CHECK(f >= 0, "cannot create %s", path);
    if (f < 0) return 1;
    for (int k = 0; k < ncases; k++) {
        case_t *c = &cases[k];
        size_t n = count(c);
        data[k] = malloc(n * c->esize);
        fill(c, data[k], n);
        hid_t sp = H5Screate_simple(c->rank, c->dims, NULL);
        hid_t dcpl = H5Pcreate(H5P_DATASET_CREATE);
        H5Pset_chunk(dcpl, c->rank, c->chunk);
        unsigned cd[1] = {c->level};
        herr_t e = H5Pset_filter(dcpl, H5Z_FILTER_KOLMO, H5Z_FLAG_MANDATORY, c->level ? 1 : 0, cd);
        CHECK(e >= 0, "%s: H5Pset_filter", c->name);
        hid_t d = H5Dcreate2(f, c->name, c->mem_type, sp, H5P_DEFAULT, dcpl, H5P_DEFAULT);
        CHECK(d >= 0, "%s: H5Dcreate2", c->name);
        CHECK(H5Dwrite(d, c->mem_type, H5S_ALL, H5S_ALL, H5P_DEFAULT, data[k]) >= 0, "%s: H5Dwrite", c->name);
        H5Dclose(d); H5Pclose(dcpl); H5Sclose(sp);
    }
    CHECK(H5Zfilter_avail(H5Z_FILTER_KOLMO) > 0, "filter %d not available after use", H5Z_FILTER_KOLMO);

    /* an invalid level is refused when the dataset is created */
    {
        hsize_t dims[1] = {1000}, ch[1] = {100};
        hid_t sp = H5Screate_simple(1, dims, NULL), dcpl = H5Pcreate(H5P_DATASET_CREATE);
        unsigned cd[1] = {11};
        H5Pset_chunk(dcpl, 1, ch);
        H5Pset_filter(dcpl, H5Z_FILTER_KOLMO, H5Z_FLAG_MANDATORY, 1, cd);
        H5E_BEGIN_TRY { hid_t d = H5Dcreate2(f, "bad_level", H5T_NATIVE_INT, sp, H5P_DEFAULT, dcpl, H5P_DEFAULT);
                        CHECK(d < 0, "level 11 accepted"); if (d >= 0) H5Dclose(d); } H5E_END_TRY
        H5Pclose(dcpl); H5Sclose(sp);
    }
    /* a damaged chunk, written raw past the filter: reading it must fail cleanly */
    {
        hsize_t dims[1] = {4096}, ch[1] = {4096}, off[1] = {0};
        hid_t sp = H5Screate_simple(1, dims, NULL), dcpl = H5Pcreate(H5P_DATASET_CREATE);
        H5Pset_chunk(dcpl, 1, ch);
        H5Pset_filter(dcpl, H5Z_FILTER_KOLMO, H5Z_FLAG_MANDATORY, 0, NULL);
        hid_t d = H5Dcreate2(f, "damaged", H5T_NATIVE_INT, sp, H5P_DEFAULT, dcpl, H5P_DEFAULT);
        uint8_t junk[300];
        for (size_t i = 0; i < sizeof junk; i++) junk[i] = (uint8_t)next();
        memcpy(junk, "CZN1", 4);
        CHECK(H5Dwrite_chunk(d, H5P_DEFAULT, 0, off, sizeof junk, junk) >= 0, "H5Dwrite_chunk");
        H5Dclose(d); H5Pclose(dcpl); H5Sclose(sp);
    }
    H5Fclose(f);

    /* read back */
    f = H5Fopen(path, H5F_ACC_RDONLY, H5P_DEFAULT);
    CHECK(f >= 0, "cannot reopen %s", path);
    for (int k = 0; k < ncases; k++) {
        case_t *c = &cases[k];
        size_t n = count(c), bytes = n * c->esize;
        hid_t d = H5Dopen2(f, c->name, H5P_DEFAULT);
        hid_t dcpl = H5Dget_create_plist(d);
        unsigned flags, cd[8] = {0};
        size_t nel = 8;
        CHECK(H5Pget_filter_by_id2(dcpl, H5Z_FILTER_KOLMO, &flags, &nel, cd, 0, NULL, NULL) >= 0, "%s: filter not on dataset", c->name);
        CHECK(nel == H5Z_KOLMO_CD_NELMTS, "%s: %zu client values", c->name, nel);
        CHECK(cd[H5Z_KOLMO_CD_LEVEL] == c->level, "%s: level %u", c->name, cd[0]);
        CHECK(cd[H5Z_KOLMO_CD_ELEMSIZE] == c->esize, "%s: element size %u, want %zu", c->name, cd[1], c->esize);
        CHECK(cd[H5Z_KOLMO_CD_VERSION] == H5Z_KOLMO_CD_LAYOUT, "%s: layout %u", c->name, cd[3]);
        void *back = malloc(bytes);
        memset(back, 0xA5, bytes);
        CHECK(H5Dread(d, c->mem_type, H5S_ALL, H5S_ALL, H5P_DEFAULT, back) >= 0, "%s: H5Dread", c->name);
        CHECK(memcmp(back, data[k], bytes) == 0, "%s: data differ after round trip", c->name);
        hsize_t stored = H5Dget_storage_size(d);
        if (c->compressible) CHECK(stored < bytes, "%s: stored %llu of %zu bytes", c->name, (unsigned long long)stored, bytes);
        printf("ok  %-16s level %-2u esize %-2zu  %9zu -> %8llu bytes  (%.2fx)\n", c->name, c->level ? c->level : 3,
               c->esize, bytes, (unsigned long long)stored, stored ? (double)bytes / stored : 0.0);
        free(back); free(data[k]);
        H5Pclose(dcpl); H5Dclose(d);
    }
    {
        hid_t d = H5Dopen2(f, "damaged", H5P_DEFAULT);
        int *back = malloc(4096 * sizeof(int));
        herr_t e;
        H5E_BEGIN_TRY { e = H5Dread(d, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, back); } H5E_END_TRY
        CHECK(e < 0, "damaged chunk read without error");
        if (e < 0) printf("ok  damaged chunk is a read error\n");
        free(back); H5Dclose(d);
    }
    H5Fclose(f);
    H5Tclose(rec);
    printf(failures ? "FAILED: %d check(s)\n" : "all HDF5 filter tests passed\n", failures);
    return failures != 0;
}
