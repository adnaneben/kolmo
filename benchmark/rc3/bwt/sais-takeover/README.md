Scripts that turned libsais 2.12.1 (as vendored by libbsc 3.4.0) into src/algo/sais/sais.c. Kept to make the
takeover reproducible and reviewable; they are not part of the build.

1. `prune.py FILE NAME...` deletes function definitions; run on the public APIs Kolmo does not call, then repeatedly
   on every static function `gcc -Wunused-function` reports (with -fopenmp -DLIBSAIS_OPENMP, so threaded paths count).
   The GSA and cyclic branches were removed by hand before the last pruning rounds (see sais.c's header).
2. `run_deomp.sh` runs `deomp.py` (each `#pragma omp parallel` region becomes a function plus an argument struct;
   locals shared with the region are listed per function as name:type) and then `hdr.py` (the crew runtime at the top).
3. `post.py` turns the one critical section into an atomic add and drops declarations the conversion left unused.
4. `unif.py` resolves `#if defined(LIBSAIS_OPENMP)` as true (the OpenMP-less serial branches are dead code now).
