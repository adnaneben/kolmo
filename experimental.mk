# Standalone research codecs. None of them is used by the `kolmo` binary or library, none writes a Kolmo
# frame, and none changes the Kolmo format. Each has its own command-line tool and its own frame.
#
#   build/krc-cpu      KRC1 residual codec, CPU only, threaded encode and decode       (Codex, 7-10 Nov 2026)
#   build/krc-auto     KRC1 with the Metal GPU path on macOS and a CPU fallback       (Codex, 7-10 Nov 2026)
#   build/kbc          KBC1 bitwise context-mixing codec built on KRC1                 (Codex, 7 Nov 2026)
#   build/kbx          KBX3: KBC1 with longer hashed contexts (profiles 0, 1, 2)      (Codex, 7 Nov 2026)
#   build/kolmo-fastq  four-line FASTQ prototype (names, bases, qualities as streams) (Codex, 7 Nov 2026)
#   build/kvid         temporal-prediction front end for raw video frames             (new, 15 Nov 2026)
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

build/kvid: $(XD)/kvid.c src/kolmo.h build/libkolmo.a
	@mkdir -p build
	$(CC) $(CFLAGS) -Isrc $(XD)/kvid.c build/libkolmo.a $(LIBS) $(CXXLIB) -o $@

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
