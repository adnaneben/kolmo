# Standalone research codecs. None of them is used by the `kolmo` binary or library, none writes a Kolmo
# frame, and none changes the Kolmo format. Each has its own command-line tool and its own frame.
#
#   build/krc-cpu      KRC1 residual codec, CPU only, threaded encode and decode       (Codex, 7-10 Nov 2026)
#   build/krc-auto     KRC1 with the Metal GPU path on macOS and a CPU fallback       (Codex, 7-10 Nov 2026)
#   build/kbc          KBC1 bitwise context-mixing codec built on KRC1                 (Codex, 7 Nov 2026)
#   build/kbx          KBX3: KBC1 with longer hashed contexts (profiles 0, 1, 2)      (Codex, 7 Nov 2026)
#   build/kolmo-fastq  four-line FASTQ prototype (names, bases, qualities as streams) (Codex, 7 Nov 2026)
#   build/kvid         temporal-prediction front end for raw video frames             (new, 15 Nov 2026)
#   build/kbin         KBN1 numeric-array coder after the pcodec paper (KBIN.md)    (new, 16 Nov 2026)
#
# `make experimental-check` builds them and runs their correctness tests, with and without sanitizers.

XD := src/experimental
KRC_CORE := $(XD)/krc.c $(XD)/krc.h $(XD)/krc_parallel.inc

build/krc-cpu: $(XD)/krc_cli.c $(KRC_CORE)
	@mkdir -p build
	$(CC) $(CFLAGS) -I$(XD) $(XD)/krc_cli.c $(XD)/krc.c -lpthread -o $@

build/krc-frontier-test: tests/krc_frontier.c $(KRC_CORE)
	@mkdir -p build
	$(CC) $(SANFLAGS) -I$(XD) tests/krc_frontier.c $(XD)/krc.c -lpthread -o $@

build/krctest: tests/krctest.c $(KRC_CORE)
	@mkdir -p build
	$(CC) $(CFLAGS) -I$(XD) tests/krctest.c $(XD)/krc.c -lpthread -o $@

build/krctest-asan: tests/krctest.c $(KRC_CORE)
	@mkdir -p build
	$(CC) $(SANFLAGS) -I$(XD) tests/krctest.c $(XD)/krc.c -lpthread -o $@

ifeq ($(UNAME_S),Darwin)
KRC_METAL := $(XD)/krc_hybrid.c $(XD)/krc_dispatch.c
build/krc_kernel.inc: $(XD)/krc_simd.metal
	@mkdir -p build
	xxd -i < $< > $@
build/krc-gpu.o: $(XD)/krc_gpu.m $(XD)/krc_gpu.h $(XD)/krc.h build/krc_kernel.inc
	@mkdir -p build
	$(CC) $(CFLAGS) -Ibuild -fobjc-arc -c $< -o $@
build/krc-auto: $(XD)/krc_cli.c $(KRC_CORE) $(KRC_METAL) build/krc-gpu.o
	$(CC) $(CFLAGS) -DKRC_HAVE_METAL -I$(XD) $< $(XD)/krc.c $(KRC_METAL) build/krc-gpu.o -framework Foundation -framework Metal -lpthread -o $@
build/krcgputest: tests/krcgputest.c $(KRC_CORE) $(KRC_METAL) build/krc-gpu.o
	$(CC) $(CFLAGS) -DKRC_HAVE_METAL -I$(XD) $< $(XD)/krc.c $(KRC_METAL) build/krc-gpu.o -framework Foundation -framework Metal -lpthread -o $@
GPU_CHECK := build/krcgputest
else
build/krc-auto: build/krc-cpu
	cp $< $@
GPU_CHECK :=
endif

build/kbc: $(XD)/kbc_cli.c $(XD)/kbc.c $(XD)/kbc.h $(KRC_CORE)
	@mkdir -p build
	$(CC) $(CFLAGS) -I$(XD) $(XD)/kbc_cli.c $(XD)/kbc.c $(XD)/krc.c -lpthread -o $@

build/kbctest-asan: tests/kbctest.c $(XD)/kbc.c $(XD)/kbc.h $(KRC_CORE)
	@mkdir -p build
	$(CC) $(SANFLAGS) -I$(XD) tests/kbctest.c $(XD)/kbc.c $(XD)/krc.c -lpthread -o $@

build/kbx: $(XD)/kbx_cli.c $(XD)/kbx.c $(XD)/kbx.h $(KRC_CORE)
	@mkdir -p build
	$(CC) $(CFLAGS) -I$(XD) $(XD)/kbx_cli.c $(XD)/kbx.c $(XD)/krc.c -lpthread -o $@

build/kbxtest-asan: tests/kbxtest.c $(XD)/kbx.c $(XD)/kbc.c $(XD)/kbx.h $(XD)/kbc.h $(KRC_CORE)
	@mkdir -p build
	$(CC) $(SANFLAGS) -I$(XD) tests/kbxtest.c $(XD)/kbx.c $(XD)/kbc.c $(XD)/krc.c -lpthread -o $@

FASTQ_SRC := $(XD)/frontier_fastq.c $(XD)/frontier_entropy.c $(XD)/frontier_runs.c
FASTQ_HDR := $(XD)/frontier_fastq.h $(XD)/frontier_entropy.h $(XD)/frontier_runs.h
build/kolmo-fastq: $(XD)/fastq_cli.c $(FASTQ_SRC) $(FASTQ_HDR)
	@mkdir -p build
	$(CC) $(CFLAGS) -I$(XD) $(XD)/fastq_cli.c $(FASTQ_SRC) -o $@

build/frontier-reads-test: tests/frontier_reads.c $(FASTQ_SRC) $(FASTQ_HDR)
	@mkdir -p build
	$(CC) $(SANFLAGS) -I$(XD) tests/frontier_reads.c $(FASTQ_SRC) -o $@

build/kfam: $(XD)/kfam.c src/codecs/fastq.c
	@mkdir -p build
	$(CC) $(CFLAGS) -ffp-contract=off $(XD)/kfam.c -lpthread -o $@

build/kfam-asan: $(XD)/kfam.c src/codecs/fastq.c
	@mkdir -p build
	$(CC) $(SANFLAGS) -ffp-contract=off $(XD)/kfam.c -lpthread -o $@

# kfam round trips (FASTQ, CSV and one-stream families) under ASan and UBSan, on inputs made from the test files, and
# damaged frames (no hang, no sanitizer report)
kfam-check: build/kfam-asan
	printf '@r1 x/1\nACGTNACGTA\n+\nIIIIIHHHGG\n@r2 x/1\nTTGCA\n+\n#####\n@r3 x/1\nACGTACGTAC\n+r3 x/1\nIIIIIIIIII\n' > build/kfam-t.fastq
	printf 'country,year,co2,share\nA,2000,1.5,\nA,2001,1.75,0.1\nB,2000,,-0.25\nB,2001,12,3\n' > build/kfam-t.csv
	for f in build/kfam-t.fastq build/kfam-t.csv tests/*.c; do ./build/kfam-asan -T 2 -b $$f > /dev/null || exit 1; ./build/kfam-asan -f one -b $$f > /dev/null || exit 1; done
	awk 'BEGIN { srand(7); g = ""; for (i = 0; i < 3000; i++) g = g substr("ACGT", int(rand() * 4) + 1, 1); \
	  for (r = 0; r < 3000; r++) { L = r % 97 == 0 ? 0 : 20 + int(rand() * 130); o = int(rand() * (3000 - L)); s = substr(g, o + 1, L); \
	    if (r % 5 == 0) { t = ""; for (i = 1; i <= L; i++) t = t substr(s, L - i + 1, 1); gsub(/A/, "t", t); gsub(/T/, "a", t); gsub(/C/, "g", t); gsub(/G/, "c", t); s = toupper(t) } \
	    if (r % 7 == 0 && L > 10) s = substr(s, 1, 5) "N" substr(s, 7); q = ""; for (i = 0; i < L; i++) q = q sprintf("%c", 35 + int(rand() * 40)); \
	    printf "@x%d:%d %d/1\n%s\n+%s\n%s\n", r, r * 3 + 1, o, s, (r % 11 == 0 ? "x" r ":" (r * 3 + 1) " " o "/1" : ""), q } }' > build/kfam-t2.fastq
	./build/kfam-asan -T 3 -C 997 -B 5000 -b build/kfam-t2.fastq > /dev/null
	./build/kfam-asan -T 1 -C 0 -B 0 -b build/kfam-t2.fastq > /dev/null
	python3 -I tests/kfam_damage.py ./build/kfam-asan build 60
	@echo "KFAM TESTS PASSED"
.PHONY: kfam-check

build/kvid: $(XD)/kvid.c src/kolmo.h build/libkolmo.a build/libvendor.a build/gpu.cfg
	@mkdir -p build
	$(CC) $(CFLAGS) $(if $(GPUSRC),-DKVID_GPU) -Isrc $(XD)/kvid.c build/libkolmo.a build/libvendor.a $(LIBS) $(CXXLIB) -o $@

experimental-check: build/krc-frontier-test build/krctest build/krctest-asan build/kbctest-asan build/kbxtest-asan \
		build/frontier-reads-test build/krc-cpu build/krc-auto build/kbc build/kbx build/kolmo-fastq $(GPU_CHECK) \
		build/kbin build/kbin-asan
	$(if $(GPU_CHECK),./build/krcgputest)
	./build/krc-frontier-test
	./build/krctest
	./build/krctest-asan
	./build/kbctest-asan
	./build/kbxtest-asan
	./build/frontier-reads-test
	./build/kbin-asan --selftest
.PHONY: experimental-check

# kbin: numeric-array coder after the pcodec paper (binning, rANS bin indices, raw offset bits); KBIN.md
build/kbin: $(XD)/kbin.c
	@mkdir -p build
	$(CC) $(CFLAGS) -ffp-contract=off $(XD)/kbin.c -lpthread -lm -o $@

build/kbin-asan: $(XD)/kbin.c
	@mkdir -p build
	$(CC) $(SANFLAGS) -ffp-contract=off $(XD)/kbin.c -lpthread -lm -o $@

kbin-check: build/kbin-asan build/kbin
	./build/kbin-asan --selftest
	./build/kbin --selftest
.PHONY: kbin-check
