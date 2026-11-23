/* A minimal zstd HDF5 filter for the benchmark only: registered filter ID 32015 and cd_values[0] = level, as the
 * zstd filter shipped by hdf5plugin (HDF5Plugin-Zstandard), built against the system libzstd. Not installed and not
 * part of the Kolmo plugin. */
#include <stdlib.h>
#include <zstd.h>
#include "hdf5.h"
#include "H5PLextern.h"

static size_t zstd_filter(unsigned flags, size_t cd_nelmts, const unsigned cd_values[], size_t nbytes, size_t *buf_size, void **buf) {
    void *out;
    size_t r, cap;
    if (flags & H5Z_FLAG_REVERSE) {
        unsigned long long n = ZSTD_getFrameContentSize(*buf, nbytes);
        if (n == ZSTD_CONTENTSIZE_ERROR || n == ZSTD_CONTENTSIZE_UNKNOWN) return 0;
        cap = (size_t)n;
        if (!(out = malloc(cap ? cap : 1))) return 0;
        r = ZSTD_decompress(out, cap, *buf, nbytes);
    } else {
        int level = cd_nelmts > 0 ? (int)cd_values[0] : 3;
        cap = ZSTD_compressBound(nbytes);
        if (!(out = malloc(cap))) return 0;
        r = ZSTD_compress(out, cap, *buf, nbytes, level);
    }
    if (ZSTD_isError(r)) { free(out); return 0; }
    free(*buf);
    *buf = out;
    *buf_size = cap;
    return r;
}

static const H5Z_class2_t ZSTD_CLASS[1] = {{H5Z_CLASS_T_VERS, (H5Z_filter_t)32015, 1, 1, "zstd (benchmark copy)", NULL, NULL,
                                           (H5Z_func_t)zstd_filter}};
H5PL_type_t H5PLget_plugin_type(void) { return H5PL_TYPE_FILTER; }
const void *H5PLget_plugin_info(void) { return ZSTD_CLASS; }
