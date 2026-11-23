/* H5Zkolmo: HDF5 filter plugin for the Kolmo compressor (see H5Zkolmo.h for the filter ID and client data).
 *
 * Built as a dynamically loadable plugin (H5PLget_plugin_type / H5PLget_plugin_info), found by HDF5 through
 * HDF5_PLUGIN_PATH. On macOS the plugin does not link libhdf5: its HDF5 symbols resolve to the HDF5 library already
 * loaded in the process (the C library of an application, or the one inside h5py). On Linux it links the libhdf5 it
 * was built against, because Python loads h5py's HDF5 privately (RTLD_LOCAL); see the Makefile.
 *
 * Licence: Apache-2.0.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hdf5.h"
#include "H5PLextern.h"

#include "kolmo.h"
#include "H5Zkolmo.h"

#define KOLMO_H5_MAX_CHUNK ((uint64_t)UINT32_MAX)   /* HDF5 chunks are at most 4 GiB */

#define PUSH_ERR(func, minor, msg) H5Epush2(H5E_DEFAULT, __FILE__, func, __LINE__, H5E_ERR_CLS, H5E_PLINE, minor, msg)

static int level_ok(unsigned level) {
    kolmo_params p;
    memset(&p, 0, sizeof p);
    return level == 0 || kolmo_params_for_level((int)level, &p) == 0;
}

/* Any datatype and dataspace: Kolmo is lossless on bytes. Rejects only a level Kolmo does not have, so that the error
 * comes when the dataset is created rather than at the first write. */
static htri_t H5Z_kolmo_can_apply(hid_t dcpl_id, hid_t type_id, hid_t space_id) {
    unsigned flags = 0, cd[H5Z_KOLMO_CD_NELMTS] = {0};
    size_t n = H5Z_KOLMO_CD_NELMTS;
    (void)type_id; (void)space_id;
    if (H5Pget_filter_by_id2(dcpl_id, H5Z_FILTER_KOLMO, &flags, &n, cd, 0, NULL, NULL) < 0) return 1;
    if (n > H5Z_KOLMO_CD_LEVEL && !level_ok(cd[H5Z_KOLMO_CD_LEVEL])) {
        PUSH_ERR("H5Z_kolmo_can_apply", H5E_BADVALUE, "kolmo: level must be 1-9, 10 (ultra) or 0 (default)");
        return 0;
    }
    return 1;
}

/* Fills in the element size from the dataset's datatype, the defaults, and the layout version. */
static herr_t H5Z_kolmo_set_local(hid_t dcpl_id, hid_t type_id, hid_t space_id) {
    unsigned flags = 0, cd[H5Z_KOLMO_CD_NELMTS] = {0};
    size_t n = H5Z_KOLMO_CD_NELMTS;
    (void)space_id;
    if (H5Pget_filter_by_id2(dcpl_id, H5Z_FILTER_KOLMO, &flags, &n, cd, 0, NULL, NULL) < 0) {
        PUSH_ERR("H5Z_kolmo_set_local", H5E_CANTGET, "kolmo: cannot read the filter's client data");
        return -1;
    }
    for (size_t i = n; i < H5Z_KOLMO_CD_NELMTS; i++) cd[i] = 0;   /* absent values are defaults */
    if (cd[H5Z_KOLMO_CD_ELEMSIZE] == 0) {
        size_t es = H5Tget_size(type_id);
        cd[H5Z_KOLMO_CD_ELEMSIZE] = es > 0 && es <= UINT32_MAX ? (unsigned)es : 0;
    }
    cd[H5Z_KOLMO_CD_VERSION] = H5Z_KOLMO_CD_LAYOUT;
    if (H5Pmodify_filter(dcpl_id, H5Z_FILTER_KOLMO, flags, H5Z_KOLMO_CD_NELMTS, cd) < 0) {
        PUSH_ERR("H5Z_kolmo_set_local", H5E_CANTSET, "kolmo: cannot store the filter's client data");
        return -1;
    }
    return 0;
}

static size_t H5Z_kolmo_filter(unsigned flags, size_t cd_nelmts, const unsigned cd_values[], size_t nbytes,
                               size_t *buf_size, void **buf) {
    const uint8_t *in = (const uint8_t *)*buf;
    uint8_t *out = NULL;
    int64_t r;
    size_t cap;

    if (flags & H5Z_FLAG_REVERSE) {                      /* decode: the frame says everything */
        kolmo_decode_limits lim;
        int64_t claim;
        memset(&lim, 0, sizeof lim);
        lim.max_output_bytes = KOLMO_H5_MAX_CHUNK;
        if ((r = kolmo_check_frame(in, nbytes, &lim)) < 0 || (claim = kolmo_original_size(in, nbytes)) < 0) {
            PUSH_ERR("H5Z_kolmo_filter", H5E_CANTFILTER, "kolmo: chunk is not a valid Kolmo frame");
            return 0;
        }
        cap = (size_t)claim;
        if (!(out = (uint8_t *)H5allocate_memory(cap ? cap : 1, 0))) {
            PUSH_ERR("H5Z_kolmo_filter", H5E_CANTALLOC, "kolmo: out of memory");
            return 0;
        }
        r = kolmo_decompress_limited(in, nbytes, out, cap, 1, &lim);
        if (r < 0 || (size_t)r != cap) {
            H5free_memory(out);
            PUSH_ERR("H5Z_kolmo_filter", H5E_CANTFILTER, "kolmo: cannot decode chunk");
            return 0;
        }
    } else {
        kolmo_params p;
        unsigned level = cd_nelmts > H5Z_KOLMO_CD_LEVEL ? cd_values[H5Z_KOLMO_CD_LEVEL] : 0;
        unsigned threads = cd_nelmts > H5Z_KOLMO_CD_THREADS ? cd_values[H5Z_KOLMO_CD_THREADS] : 0;
        memset(&p, 0, sizeof p);
        if (kolmo_params_for_level(level ? (int)level : KOLMO_LEVEL_DEFAULT, &p) != 0) {
            PUSH_ERR("H5Z_kolmo_filter", H5E_BADVALUE, "kolmo: invalid level");
            return 0;
        }
        p.threads = threads ? (int)threads : 1;
        /* The element size (cd_values[1]) is recorded with the dataset, but this version of Kolmo has no way to take
         * it as a hint: its planner finds the word width of numeric data itself (byte-plane shuffle trials over the
         * widths 1-8). It is kept so that files written now carry it for a later Kolmo that accepts the hint. */
        cap = kolmo_bound(nbytes, &p);
        if (!(out = (uint8_t *)H5allocate_memory(cap, 0))) {
            PUSH_ERR("H5Z_kolmo_filter", H5E_CANTALLOC, "kolmo: out of memory");
            return 0;
        }
        r = kolmo_compress(in, nbytes, out, cap, &p);
        if (r < 0) {
            H5free_memory(out);
            PUSH_ERR("H5Z_kolmo_filter", H5E_CANTFILTER, "kolmo: compression failed");
            return 0;
        }
    }
    H5free_memory(*buf);
    *buf = out;
    *buf_size = cap;
    return (size_t)r;
}

static const H5Z_class2_t H5Z_KOLMO_CLASS[1] = {{
    H5Z_CLASS_T_VERS,
    (H5Z_filter_t)H5Z_FILTER_KOLMO,
    1, 1,                                                /* encoder and decoder present */
    "kolmo; see https://github.com/adnaneben/kolmo",
    H5Z_kolmo_can_apply,
    H5Z_kolmo_set_local,
    (H5Z_func_t)H5Z_kolmo_filter,
}};

H5PL_type_t H5PLget_plugin_type(void) { return H5PL_TYPE_FILTER; }
const void *H5PLget_plugin_info(void) { return H5Z_KOLMO_CLASS; }

int H5Z_kolmo_register(void) { return (int)H5Zregister(H5Z_KOLMO_CLASS); }
