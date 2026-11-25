#include <blosc2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "blosc2_kolmo.h"

/* Run as two processes to verify that the private codec ID survives on disk. */
int main(int argc, char **argv) {
    if (argc != 3) return 2;
    int writing = !strcmp(argv[1], "write");
    if (!writing && strcmp(argv[1], "read")) return 2;
    blosc2_init();
    if (blosc2_kolmo_register() != 0) return 1;

    blosc2_cparams params = BLOSC2_CPARAMS_DEFAULTS;
    params.compcode = BLOSC_CODEC_KOLMO_USER;
    params.typesize = 4;
    params.clevel = 3;
    params.filters[BLOSC2_MAX_FILTERS - 1] = BLOSC_NOSHUFFLE;

    blosc2_storage storage = BLOSC2_STORAGE_DEFAULTS;
    storage.contiguous = true;
    storage.urlpath = argv[2];
    storage.cparams = &params;
    blosc2_schunk *s = writing ? blosc2_schunk_new(&storage) : blosc2_schunk_open(argv[2]);
    if (!s || s->compcode != BLOSC_CODEC_KOLMO_USER) return 1;

    uint32_t original[8192], restored[8192];
    for (int chunk = 0; chunk < 6; chunk++) {
        for (size_t i = 0; i < 8192; i++) original[i] = chunk * 8192 + i;
        if (writing) {
            if (blosc2_schunk_append_buffer(s, original, sizeof(original)) != chunk + 1) return 1;
        } else {
            int size = blosc2_schunk_decompress_chunk(s, chunk, restored, sizeof(restored));
            if (size != sizeof(original) || memcmp(original, restored, sizeof(original))) return 1;
        }
    }
    printf("%s: codec %d, %lld chunks, %lld input bytes, %lld bytes in compressed chunks\n",
           argv[1], (int)s->compcode, (long long)s->nchunks, (long long)s->nbytes, (long long)s->cbytes);
    if (s->nchunks != 6 || s->cbytes >= s->nbytes) return 1;
    blosc2_schunk_free(s);
    blosc2_destroy();
    return 0;
}
