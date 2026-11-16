# Standalone research codecs. None of them is used by the `kolmo` binary or library, none writes a Kolmo
# frame, and none changes the Kolmo format. Each has its own command-line tool and its own frame.
#
#   build/krc-cpu      KRC1 residual codec, CPU only, threaded encode and decode       (Codex, 7-10 Nov 2026)
#   build/krc-auto     KRC1 with the Metal GPU path on macOS and a CPU fallback       (Codex, 7-10 Nov 2026)
#   build/kbc          KBC1 bitwise context-mixing codec built on KRC1                 (Codex, 7 Nov 2026)
#   build/kbx          KBX3: KBC1 with longer hashed contexts (profiles 0, 1, 2)      (Codex, 7 Nov 2026)
#   build/kolmo-fastq  four-line FASTQ prototype (names, bases, qualities as streams) (Codex, 7 Nov 2026)
#   build/kvid         temporal-prediction front end for raw video frames             (new, 15 Nov 2026)
#   build/krans        static interleaved rANS, order 0 and 1, AVX2/NEON decoders     (new, 16 Nov 2026)
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

build/kfam: $(XD)/kfam.c
	@mkdir -p build
	$(CC) $(CFLAGS) -ffp-contract=off $(XD)/kfam.c -lpthread -o $@

build/kfam-asan: $(XD)/kfam.c
	@mkdir -p build
	$(CC) $(SANFLAGS) -ffp-contract=off $(XD)/kfam.c -lpthread -o $@

# kfam round trips (FASTQ, CSV and one-stream families) under ASan and UBSan, on inputs made from the test files
kfam-check: build/kfam-asan
	printf '@r1 x/1\nACGTNACGTA\n+\nIIIIIHHHGG\n@r2 x/1\nTTGCA\n+\n#####\n@r3 x/1\nACGTACGTAC\n+r3 x/1\nIIIIIIIIII\n' > build/kfam-t.fastq
	printf 'country,year,co2,share\nA,2000,1.5,\nA,2001,1.75,0.1\nB,2000,,-0.25\nB,2001,12,3\n' > build/kfam-t.csv
	for f in build/kfam-t.fastq build/kfam-t.csv tests/*.c; do ./build/kfam-asan -T 2 -b $$f > /dev/null || exit 1; ./build/kfam-asan -f one -b $$f > /dev/null || exit 1; done
	@echo "KFAM TESTS PASSED"
.PHONY: kfam-check

build/kvid: $(XD)/kvid.c src/kolmo.h build/libkolmo.a build/libvendor.a
	@mkdir -p build
	$(CC) $(CFLAGS) -Isrc $(XD)/kvid.c build/libkolmo.a build/libvendor.a $(LIBS) $(CXXLIB) -o $@

experimental-check: build/krc-frontier-test build/krctest build/krctest-asan build/kbctest-asan build/kbxtest-asan \
		build/frontier-reads-test build/krc-cpu build/krc-auto build/kbc build/kbx build/kolmo-fastq $(GPU_CHECK)
	$(if $(GPU_CHECK),./build/krcgputest)
	./build/krc-frontier-test
	./build/krctest
	./build/krctest-asan
	./build/kbctest-asan
	./build/kbxtest-asan
	./build/frontier-reads-test
.PHONY: experimental-check

# KRANS: static interleaved rANS (src/experimental/KRANS.md). Built for the baseline ISA on purpose: the AVX2 decoder
# is chosen at run time (with -march=native gcc puts AVX-512 encodings into the AVX2 functions, which is slower here).
KRANS_CFLAGS ?= -O3 -fno-plt
KRANS_SRC := $(XD)/krans.c $(XD)/krans.h
build/krans: $(XD)/krans_cli.c $(KRANS_SRC)
	@mkdir -p build
	$(CC) $(KRANS_CFLAGS) -I$(XD) $(XD)/krans_cli.c $(XD)/krans.c -lpthread -lm -o $@
build/kranstest: tests/kranstest.c $(KRANS_SRC)
	@mkdir -p build
	$(CC) $(KRANS_CFLAGS) -I$(XD) tests/kranstest.c $(XD)/krans.c -lpthread -lm -o $@
build/kranstest-asan: tests/kranstest.c $(KRANS_SRC)
	@mkdir -p build
	$(CC) $(SANFLAGS) -fno-sanitize-recover=undefined -I$(XD) tests/kranstest.c $(XD)/krans.c -lpthread -lm -o $@
# round trips (random, skewed, one-symbol, empty, 1-byte, 8 MiB inputs; every order/ways/precision/decoder) and
# corrupted frames, optimised and under ASan + UBSan
krans-check: build/krans build/kranstest build/kranstest-asan
	./build/kranstest
	./build/kranstest-asan
.PHONY: krans-check
# NEON decoder: cross-compiled and run under qemu-user (apt: gcc-aarch64-linux-gnu qemu-user), with UBSan
KRANS_ARM64_CC ?= aarch64-linux-gnu-gcc
krans-check-arm64: tests/kranstest.c $(KRANS_SRC)
	@mkdir -p build
	$(KRANS_ARM64_CC) -O1 -g -static -fsanitize=undefined -fno-sanitize-recover=undefined -I$(XD) tests/kranstest.c $(XD)/krans.c -lpthread -lm -o build/kranstest-arm64
	qemu-aarch64 ./build/kranstest-arm64
.PHONY: krans-check-arm64
# Rival coders on the same streams (zstd's HUF and FSE called directly, lz4, zstd -1/-3). Needs a zstd source tree
# built outside this repository: ZSTD_DIR=<zstd>/lib containing libzstd.a (bench/krans/build_zstd.sh makes one).
build/krans-rivals: $(XD)/krans_rivals.c
	@test -n "$(ZSTD_DIR)" || { echo "set ZSTD_DIR to a built zstd lib/ directory (bench/krans/build_zstd.sh)"; exit 1; }
	@mkdir -p build
	$(CC) $(KRANS_CFLAGS) -I$(ZSTD_DIR) -I$(ZSTD_DIR)/common -I$(ZSTD_DIR)/compress $< $(ZSTD_DIR)/libzstd.a -llz4 -o $@
