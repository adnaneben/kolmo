/* H5Zkolmo: HDF5 filter plugin for the Kolmo compressor.
 *
 * Licence: Apache-2.0.
 *
 * FILTER ID: PLACEHOLDER. 480 lies in the range The HDF Group reserves for testing (256-511), which is never
 * assigned to anyone, so files written with it must not be shared: another tool may use the same number for another
 * filter. A permanent ID is requested in FILTER-ID-REQUEST.md; when it is granted, change H5Z_FILTER_KOLMO below,
 * and only that. Test files written with 480 can then still be read by a build made with -DH5Z_FILTER_KOLMO=480,
 * and converted with h5repack (one plugin library registers one ID).
 *
 * Client data (cd_values), all optional; HDF5 stores them in the file with the dataset:
 *   cd_values[0]  level: 1 (fastest) .. 9 (smallest), 10 = ultra; 0 or absent = Kolmo's default (3).
 *   cd_values[1]  element size in bytes. 0 or absent = filled in when the dataset is created (set_local) from the
 *                 dataset's datatype (H5Tget_size: 8 for float64, the whole record for a compound type).
 *   cd_values[2]  threads per chunk: 0 or absent = 1. HDF5 calls a filter once per chunk; give more only for large
 *                 chunks (several MiB), and never when the application already reads chunks in parallel.
 *   cd_values[3]  cd_values layout version, written by set_local (currently 1).
 *
 * The decoder needs none of them: a Kolmo frame describes itself, so any build of this filter reads chunks written
 * with any level, element size or thread count.
 */
#ifndef H5Z_KOLMO_H
#define H5Z_KOLMO_H

#ifdef __cplusplus
extern "C" {
#endif

#define H5Z_FILTER_KOLMO_TESTING 480   /* PLACEHOLDER from the testing range 256-511: not registered */
#ifndef H5Z_FILTER_KOLMO
#define H5Z_FILTER_KOLMO H5Z_FILTER_KOLMO_TESTING
#endif

#define H5Z_KOLMO_CD_LEVEL    0
#define H5Z_KOLMO_CD_ELEMSIZE 1
#define H5Z_KOLMO_CD_THREADS  2
#define H5Z_KOLMO_CD_VERSION  3
#define H5Z_KOLMO_CD_NELMTS   4
#define H5Z_KOLMO_CD_LAYOUT   1

/* For applications that link the filter in rather than loading it from HDF5_PLUGIN_PATH: registers it with the HDF5
 * library the application uses. Returns a non-negative value on success (as H5Zregister). */
int H5Z_kolmo_register(void);

#ifdef __cplusplus
}
#endif
#endif
