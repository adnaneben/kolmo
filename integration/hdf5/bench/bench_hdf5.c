/* Single-thread benchmark of HDF5 compression filters: Kolmo (this plugin) against deflate (gzip), szip, zstd
 * (filter 32015, as in hdf5plugin) and the HDF5 shuffle filter in front of them.
 *
 *   HDF5_PLUGIN_PATH=build/plugin:build/bench-plugins ./build/bench_hdf5 [-r runs] [-c chunk_bytes] [input ...]
 *
 * Inputs as in ../../common/bench_data.h ("path:type[:skip]" or "synth:NAME"). Each input is written as a 1-D chunked
 * dataset (1 MiB chunks by default) to an in-memory HDF5 file (core driver: no disk I/O in the timings), with the
 * chunk cache off so that every chunk passes through the filter on write and on read. Times are the best of `runs`
 * whole-dataset H5Dwrite and H5Dread calls; speeds are uncompressed MB/s (10^6 bytes). One thread: HDF5 runs filters
 * on the calling thread, and the Kolmo filter is used with its default of 1 thread per chunk.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hdf5.h"
#include "../H5Zkolmo.h"
#include "../../common/bench_data.h"

#define F_ZSTD 32015

typedef struct { const char *label; int shuffle; int filter; unsigned cd; } config;

static const config CONFIGS[] = {
    {"gzip 1",            0, H5Z_FILTER_DEFLATE, 1},
    {"gzip 6",            0, H5Z_FILTER_DEFLATE, 6},
    {"shuffle + gzip 6",  1, H5Z_FILTER_DEFLATE, 6},
    {"szip nn",           0, H5Z_FILTER_SZIP,    0},
    {"zstd 3",            0, F_ZSTD,             3},
    {"shuffle + zstd 3",  1, F_ZSTD,             3},
    {"shuffle + zstd 9",  1, F_ZSTD,             9},
    {"shuffle + zstd 19", 1, F_ZSTD,             19},
    {"kolmo 1",           0, H5Z_FILTER_KOLMO,   1},
    {"kolmo 3",           0, H5Z_FILTER_KOLMO,   3},
    {"kolmo 5",           0, H5Z_FILTER_KOLMO,   5},
    {"kolmo 7",           0, H5Z_FILTER_KOLMO,   7},
    {"shuffle + kolmo 3", 1, H5Z_FILTER_KOLMO,   3},
};

static hid_t h5type(const char *t) {
    if (!strcmp(t, "u8")) return H5T_NATIVE_UINT8;
    if (!strcmp(t, "i16")) return H5T_NATIVE_INT16;
    if (!strcmp(t, "i32")) return H5T_NATIVE_INT32;
    if (!strcmp(t, "i64")) return H5T_NATIVE_INT64;
    if (!strcmp(t, "f32")) return H5T_NATIVE_FLOAT;
    return H5T_NATIVE_DOUBLE;
}

int main(int argc, char **argv) {
    int runs = 3;
    size_t chunk_bytes = 1 << 20;
    const char *defaults[] = {"synth:f32_grid", "synth:i64_ticks", "synth:i16_wave"};
    const char **inputs = defaults;
    int ninputs = 3, a = 1;
    for (; a < argc && argv[a][0] == '-'; a++) {
        if (!strcmp(argv[a], "-r") && a + 1 < argc) runs = atoi(argv[++a]);
        else if (!strcmp(argv[a], "-c") && a + 1 < argc) chunk_bytes = (size_t)atol(argv[++a]);
        else { fprintf(stderr, "usage: %s [-r runs] [-c chunk_bytes] [path:type[:skip] | synth:NAME ...]\n", argv[0]); return 2; }
    }
    if (a < argc) { inputs = (const char **)(argv + a); ninputs = argc - a; }

    printf("%s; filters on one thread, %zu-byte chunks, best of %d; MB/s of uncompressed data\n\n",
           H5_VERS_INFO, chunk_bytes, runs);
    for (int k = 0; k < ninputs; k++) {
        bench_input in;
        if (bench_load(inputs[k], &in) != 0) continue;
        hid_t type = h5type(in.type);
        hsize_t n = in.bytes / in.esize, chunk = chunk_bytes / in.esize;
        if (chunk > n) chunk = n;
        uint8_t *back = malloc(in.bytes);
        printf("### %s (%s, %.2f MB)\n\n| filter | ratio | compress MB/s | decompress MB/s |\n|---|---:|---:|---:|\n",
               in.name, in.type, in.bytes / 1e6);
        for (size_t c = 0; c < sizeof CONFIGS / sizeof CONFIGS[0]; c++) {
            const config *cf = &CONFIGS[c];
            double best_w = 1e30, best_r = 1e30;
            hsize_t stored = 0;
            int ok = 1;
            for (int r = 0; r < runs && ok; r++) {
                hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
                H5Pset_fapl_core(fapl, 64 << 20, 0);
                hid_t f = H5Fcreate("bench.h5", H5F_ACC_TRUNC, H5P_DEFAULT, fapl);
                hid_t sp = H5Screate_simple(1, &n, NULL);
                hid_t dcpl = H5Pcreate(H5P_DATASET_CREATE), dapl = H5Pcreate(H5P_DATASET_ACCESS);
                H5Pset_chunk(dcpl, 1, &chunk);
                H5Pset_chunk_cache(dapl, 0, 0, 1.0);
                if (cf->shuffle) H5Pset_shuffle(dcpl);
                herr_t e;
                if (cf->filter == H5Z_FILTER_DEFLATE) e = H5Pset_deflate(dcpl, cf->cd);
                else if (cf->filter == H5Z_FILTER_SZIP) e = H5Pset_szip(dcpl, H5_SZIP_NN_OPTION_MASK, 32);
                else e = H5Pset_filter(dcpl, cf->filter, H5Z_FLAG_MANDATORY, 1, &cf->cd);
                hid_t d = -1;
                if (e >= 0) d = H5Dcreate2(f, "d", type, sp, H5P_DEFAULT, dcpl, dapl);
                if (d < 0) { ok = 0; }
                else {
                    double t0 = bench_now();
                    e = H5Dwrite(d, type, H5S_ALL, H5S_ALL, H5P_DEFAULT, in.data);
                    double t1 = bench_now();
                    memset(back, 0, in.bytes);
                    herr_t e2 = H5Dread(d, type, H5S_ALL, H5S_ALL, H5P_DEFAULT, back);
                    double t2 = bench_now();
                    if (e < 0 || e2 < 0 || memcmp(back, in.data, in.bytes) != 0) ok = 0;
                    stored = H5Dget_storage_size(d);
                    if (t1 - t0 < best_w) best_w = t1 - t0;
                    if (t2 - t1 < best_r) best_r = t2 - t1;
                    H5Dclose(d);
                }
                H5Pclose(dcpl); H5Pclose(dapl); H5Sclose(sp); H5Fclose(f); H5Pclose(fapl);
            }
            if (!ok) { printf("| %s | (unavailable or failed) | | |\n", cf->label); continue; }
            printf("| %s | %.2f | %.0f | %.0f |\n", cf->label, (double)in.bytes / stored, in.bytes / best_w / 1e6, in.bytes / best_r / 1e6);
            fflush(stdout);
        }
        printf("\n");
        free(back); free(in.data);
    }
    return 0;
}
