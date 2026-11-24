/* R07 experiment: call the existing kolmo_compress_file (fd in, fd out) in any mode, to see what the slow modes
 * would use if the CLI took the file path. usage: filepath MODE LEVEL THREADS in out */
#include "kolmo.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
int main(int c, char **v) {
    if (c < 6) return 2;
    kolmo_params p; memset(&p, 0, sizeof p);
    p.mode = atoi(v[1]); p.level = atoi(v[2]); p.threads = atoi(v[3]); p.transforms = 1;
    int in = open(v[4], O_RDONLY), out = open(v[5], O_RDWR | O_CREAT | O_TRUNC, 0644); struct stat st; fstat(in, &st);
    long long r = kolmo_compress_file(in, out, (size_t)st.st_size, &p, NULL, 0);
    fprintf(stderr, "r=%lld\n", r); if (r > 0 && ftruncate(out, r)) {}
    return r < 0;
}
