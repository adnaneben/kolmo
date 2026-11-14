#include "../P2-E11/p2e11.c"
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
__attribute__((constructor)) static void iv(int argc, char **argv, char **envp) {
    (void)envp; if (argc < 3 || strcmp(argv[1], "iv")) return;
    for (int c = 0; c < NC; c++) CONF[c] = -1;
    double tp = 0, tf = 0; int nf = 0;
    for (int a = 2; a < argc; a++) {
        size_t n; uint8_t *src = readfile(argv[a], &n); int w; size_t h; fiprog P;
        if (type_of(src, n, &w, &h) || fi_forward(src, n, w, h, &P)) { free(src); continue; }
        kbuf o1 = {0}, o2 = {0};
        if (kg_int_inv(P.par.p, P.par.n, &P.res, &P.rest, n, &o1) || fi_inverse(P.par.p, P.par.n, &P.res, &P.rest, n, &o2) || o1.n != n || o2.n != n || memcmp(o1.p, src, n) || memcmp(o2.p, src, n)) DIE("mismatch %s", argv[a]);
        double t0 = now(); for (int i = 0; i < 50; i++) { o1.n = 0; kg_int_inv(P.par.p, P.par.n, &P.res, &P.rest, n, &o1); }
        double t1 = now(); for (int i = 0; i < 50; i++) { o2.n = 0; fi_inverse(P.par.p, P.par.n, &P.res, &P.rest, n, &o2); }
        double t2 = now(); tp += (t1 - t0) / 50; tf += (t2 - t1) / 50; nf++;
        free(o1.p); free(o2.p); fi_free(&P); free(src);
    }
    printf("files %d  production kg_int_inv %.3f ms/file  fi_inverse %.3f ms/file  speedup %.1fx\n", nf, tp / nf * 1e3, tf / nf * 1e3, tp / tf);
    exit(0);
}
