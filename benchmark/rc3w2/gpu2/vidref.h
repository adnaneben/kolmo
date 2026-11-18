/* kvid strip prediction on CPU threads (vidref.c). Apache-2.0, Kolmo contributors. */
#ifndef VIDREF_H
#define VIDREF_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    int np; size_t w[3], h[3], frames;
    size_t pstart[3], strip_base[3], strips_per_frame, frame_samples, plane_res[3], res_bytes;
} vid_t;
void vid_geom(vid_t *V);
size_t vid_jobs(const vid_t *V);
void vid_job(const vid_t *V, const uint8_t *file, const uint64_t *frame_at, size_t job, uint8_t *res, uint8_t *flags);
#endif
