# kolmo — structure-aware compressor.  make -> build/kolmo, build/kolmobench, build/libkolmo.a
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
CC      ?= clang
ARCH    ?= -mcpu=native
CFLAGS  ?= -O3 $(ARCH)
BREW    := $(shell brew --prefix 2>/dev/null)
EXTRA_INC := -I$(BREW)/include
EXTRA_LIB := -L$(BREW)/lib
else
CC      ?= gcc
ARCH    ?= -march=native
CFLAGS  ?= -O3 $(ARCH) -fno-plt
endif
FPFLAGS := -ffp-contract=off
# No floating-point contraction anywhere: Kolmo, the vendored codecs, the tools and the research codecs.
# Brotli's encoder (-m eco) chooses block splits and clusters with float costs; with -mfma, -march=native or on arm64
# the compiler would otherwise fuse those into FMA and could pick other splits on different builds. Kolmo's own
# frames depend on kolmo.c (always built with FPFLAGS) and on these encoders, so both get it, whatever CFLAGS says.
override CFLAGS += $(FPFLAGS)
ZSTD    := vendor/zstd
LZ4     := vendor/lz4
BRO     := vendor/brotli
INC     := -Isrc -I$(ZSTD) -I$(LZ4) -I$(BRO)/include $(EXTRA_INC)
DEFS    := -DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0
ZSRC    := $(wildcard $(ZSTD)/common/*.c $(ZSTD)/compress/*.c $(ZSTD)/decompress/*.c $(ZSTD)/dictBuilder/*.c)
BROSRC  := $(wildcard $(BRO)/common/*.c $(BRO)/dec/*.c $(BRO)/enc/*.c)
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_M),x86_64)
ZASM    := $(ZSTD)/decompress/huf_decompress_amd64.S
else
ZASM    :=
DEFS    += -DZSTD_DISABLE_ASM
endif
OBJ     := $(patsubst %.c,build/obj/%.o,$(ZSRC) $(BROSRC) $(LZ4)/lz4.c $(LZ4)/lz4hc.c) $(patsubst %.S,build/obj/%.o,$(ZASM))
ifeq ($(NO_LZMA),1)
DEFS    += -DKOLMO_NO_LZMA
LZMALIB :=
else
LZMALIB := -llzma
endif
# macOS: link Homebrew's liblzma statically when its archive is present. A dylib outside the system cache adds
# start-up time to every run, which shows on small files at the speeds of the fast modes.
ifeq ($(UNAME_S),Darwin)
ifneq ($(NO_LZMA),1)
LZMA_A := $(firstword $(wildcard $(BREW)/lib/liblzma.a $(BREW)/opt/xz/lib/liblzma.a))
ifneq ($(LZMA_A),)
LZMALIB := $(LZMA_A)
endif
endif
endif
LIBS    := $(EXTRA_LIB) $(LZMALIB) -lpthread -lm
BLIBS   := $(LIBS)

# Library sources (docs/LAYOUT.md). src/kolmo.c includes the parts below into one translation unit, so they are listed
# by directory as its prerequisites. Compiled on their own and archived with it: arch/xxh3.c and the assembly kernels of
# this machine's ISA, src/arch/<isa>/*.S (NO_ASM=1 drops them and builds the C references only), and on macOS the GPU
# host code src/arch/metal/*.m (NO_GPU=1 drops it).
LIBDIRS := api core plan transforms codecs algo arch arch/x86_64 arch/aarch64
KOLMO_INCLUDES := src/kolmo.h $(foreach d,$(LIBDIRS),$(wildcard src/$(d)/*.c src/$(d)/*.h))
ifeq ($(UNAME_M),x86_64)
ASM_ISA := x86_64
else ifneq ($(filter arm64 aarch64,$(UNAME_M)),)
ASM_ISA := aarch64
endif
ifeq ($(NO_ASM),1)
DEFS    += -DKOLMO_NO_ASM
ASMSRC  :=
else
ASMSRC  := $(if $(ASM_ISA),$(wildcard src/arch/$(ASM_ISA)/*.S))
endif
ifeq ($(NO_GPU),1)
DEFS    += -DKOLMO_NO_GPU
GPUSRC  :=
else
GPUSRC  := $(if $(filter Darwin,$(UNAME_S)),$(wildcard src/arch/metal/*.m))
endif
ifneq ($(GPUSRC),)
DEFS    += -DKOLMO_HAVE_METAL
LIBS    += -framework Foundation -framework Metal
endif
# The block-sorting pipeline (LZP, QLFC, the libsais subset, the fork-join helper, the BWT stream formats) is compiled
# on its own too: these files have public kolmo_* entry points and are not included by src/kolmo.c.
BWTSRC  := src/algo/lzp.c src/algo/qlfc.c src/algo/par.c $(wildcard src/algo/sais/*.c) src/codecs/bwt_stream.c
BWTHDR  := $(wildcard src/algo/*.h src/algo/sais/*.h) src/codecs/bwt_stream.h
ARCHSRC := src/arch/xxh3.c $(BWTSRC) $(ASMSRC) $(GPUSRC)
ARCHOBJ := $(addprefix build/obj/,$(addsuffix .o,$(basename $(ARCHSRC))))
ARCHPIC := $(addprefix build/pic/,$(addsuffix .o,$(basename $(ARCHSRC))))

all: build/kolmo build/kolmobench build/kolmotest build/kolmoparallel build/kolmoresource

build/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEFS) $(INC) -c $< -o $@
build/obj/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEFS) $(INC) -c $< -o $@
build/obj/%.o: %.m
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEFS) $(INC) -fobjc-arc -c $< -o $@

build/libvendor.a: $(OBJ)
	ar rcs $@ $^
build/kolmo.o: src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES)
	@mkdir -p build
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) -c src/kolmo.c -o $@
$(patsubst %.c,build/obj/%.o,$(BWTSRC)) $(patsubst %.c,build/pic/%.o,$(BWTSRC)): $(BWTHDR)
build/libkolmoarch.a: $(ARCHOBJ)
	ar rcs $@ $^
build/libkolmo.a: build/kolmo.o $(ARCHOBJ)
	ar rcs $@ $^
build/kolmo: src/cli/cli.c build/libkolmo.a build/libvendor.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS)
build/kolmobench: bench/kolmobench.c build/libkolmo.a build/libvendor.a
	$(CC) $(CFLAGS) $(DEFS) $(INC) $^ -o $@ $(BLIBS)
build/kolmotest: tests/kolmotest.c build/libkolmo.a build/libvendor.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS)
build/kolmoparallel: tests/kolmoparallel.c build/libkolmo.a build/libvendor.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS)
build/kolmoresource: tests/kolmoresource.c tests/legacy_frames.h src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/kolmoresource.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
build/kolmo-profile: src/cli/cli.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) -DKOLMO_TIMING $(INC) src/cli/cli.c src/kolmo.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)

check: all
	python3 tests/test_fast_file.py
	./build/kolmoresource
	./build/kolmoparallel
	./build/kolmotest

SANFLAGS ?= -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
override SANFLAGS += $(FPFLAGS)
build/kolmoparallel-asan: tests/kolmoparallel.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/kolmoparallel.c src/kolmo.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
build/kolmoresource-asan: tests/kolmoresource.c tests/legacy_frames.h src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/kolmoresource.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
build/test-lines-asan: tests/test_lines.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_lines.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
check-lines: build/test-lines-asan
	./build/test-lines-asan
build/test-dec64-asan: tests/test_dec64.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_dec64.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
check-dec64: build/test-dec64-asan
	./build/test-dec64-asan
build/test-fastint-asan: tests/test_fastint.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_fastint.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
# The same test in the optimised build (the loops a release runs), and in the control build that sends every program
# to the generic inverse (-DKOLMO_FORCE_GENERIC_INT_INV).
build/test-fastint: tests/test_fastint.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_fastint.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
build/test-fastint-generic: tests/test_fastint.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) -DKOLMO_FORCE_GENERIC_INT_INV $(INC) tests/test_fastint.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
check-fastint: build/test-fastint-asan build/test-fastint build/test-fastint-generic
	./build/test-fastint-asan
	./build/test-fastint
	./build/test-fastint-generic
build/test-planner-ownership-asan: tests/test_planner_ownership.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_planner_ownership.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
check-planner-ownership: build/test-planner-ownership-asan
	./build/test-planner-ownership-asan
build/test-cmimg-asan: tests/test_cmimg.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_cmimg.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
check-cmimg: build/test-cmimg-asan
	./build/test-cmimg-asan
# Opt-in --typed-huf route (ported from the RC1 integration, PR #10): round trips, header parsing, and agreement of
# the typed route's predictor choice with the graph planner's.
build/test-rc-options: tests/test_rc_options.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
build/test-rc-options-asan: tests/test_rc_options.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
check-rc-options: build/test-rc-options build/test-rc-options-asan
	./build/test-rc-options
	./build/test-rc-options-asan
.PHONY: check-rc-options
# FASTQ family blocks (codec 16): container round trips at ultra, the modes that must not write it, damaged payloads
build/test-fqfam-asan: tests/test_fqfam.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_fqfam.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
check-fqfam: build/test-fqfam-asan
	./build/test-fqfam-asan
# Component tests of the features merged for the release candidate. `make check` does not run them; CI does.
# Runtime ISA dispatch (x86-64): CRC32C and XXH3 against scalar references, optimised and with sanitizers.
build/test-dispatch: tests/test_dispatch.c build/libkolmo.a build/libvendor.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS)
build/test-dispatch-asan: tests/test_dispatch.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_dispatch.c src/kolmo.c src/arch/xxh3.c $(BWTSRC) $(ASMSRC) build/libvendor.a -o $@ $(LIBS)
check-dispatch: build/test-dispatch build/test-dispatch-asan
	./build/test-dispatch
	./build/test-dispatch-asan
# Block-sorting codec: codec 5 bytes against libbsc's (golden hashes), round trips at 1 and 4 threads, damaged streams.
build/test-bwt: tests/test_bwt.c tests/test_bwt_golden.h $(BWTSRC) $(BWTHDR)
	$(CC) $(CFLAGS) -Isrc tests/test_bwt.c $(BWTSRC) -o $@ -lpthread
build/test-bwt-asan: tests/test_bwt.c tests/test_bwt_golden.h $(BWTSRC) $(BWTHDR)
	$(CC) $(SANFLAGS) -Isrc tests/test_bwt.c $(BWTSRC) -o $@ -lpthread
check-bwt: build/test-bwt build/test-bwt-asan
	./build/test-bwt
	./build/test-bwt-asan
.PHONY: check-bwt
check-rc: check-bwt check-lines check-dec64 check-fastint check-planner-ownership check-cmimg check-rc-options check-fqfam check-dispatch
.PHONY: check-rc check-lines check-dec64 check-fastint check-planner-ownership check-cmimg check-fqfam check-dispatch
check-sanitizers: build/kolmoparallel-asan build/kolmoresource-asan
	./build/kolmoresource-asan
	./build/kolmoparallel-asan

# Shared library for language bindings (Python, Rust): everything rebuilt -fPIC into build/pic.
SOEXT := $(if $(filter Darwin,$(UNAME_S)),dylib,so)
PICC  := $(patsubst %.c,build/pic/%.o,$(ZSRC) $(BROSRC) $(LZ4)/lz4.c $(LZ4)/lz4hc.c) $(patsubst %.S,build/pic/%.o,$(ZASM))
build/pic/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fPIC $(DEFS) $(INC) -c $< -o $@
build/pic/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fPIC $(DEFS) $(INC) -c $< -o $@
build/pic/%.o: %.m
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fPIC $(DEFS) $(INC) -fobjc-arc -c $< -o $@
lib: build/libkolmo.$(SOEXT)
build/libkolmo.$(SOEXT): src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) $(PICC) $(ARCHPIC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(FPFLAGS) -fPIC -shared $(DEFS) $(INC) src/kolmo.c $(PICC) $(ARCHPIC) -o $@ $(LIBS)

clean:
	rm -rf build
.PHONY: all clean lib check check-sanitizers

# Optional lower-memory archive encoder.
build/kolmo-cm20: src/cli/cli.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) -DCM_BB_MAX=20 $(DEFS) $(INC) src/cli/cli.c src/kolmo.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)

# The compact variant retains the standard Ultra bitstream and table sizes elsewhere.
build/kolmo-cm-compact: src/cli/cli.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) -DKOLMO_CM_COMPACT $(DEFS) $(INC) src/cli/cli.c src/kolmo.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)

build/cm-reference.o: tests/test_cm_compact.c src/codecs/cm.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/cm-compact-test: tests/test_cm_compact.c src/codecs/cm.c build/cm-reference.o
	$(CC) $(CFLAGS) -DKOLMO_CM_COMPACT $< build/cm-reference.o -lpthread -lm -o $@

check-cm-compact: build/cm-compact-test
	./build/cm-compact-test
.PHONY: check-cm-compact

# Research build. Experimental recipes emit v6 frames only when used.
FRONTIER_SRC := src/experimental/frontier_dna.c src/experimental/frontier_table.c src/experimental/frontier_prefix.c src/experimental/frontier_pcm.c src/experimental/frontier_lag.c src/experimental/frontier_shuffle.c src/experimental/frontier_runs.c
FRONTIER_HDR := src/experimental/frontier_dna.h src/experimental/frontier_table.h src/experimental/frontier_prefix.h src/experimental/frontier_pcm.h src/experimental/frontier_lag.h src/experimental/frontier_shuffle.h src/experimental/frontier_runs.h
build/kolmo-frontier: src/cli/cli.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) $(FRONTIER_SRC) $(FRONTIER_HDR) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) -DKOLMO_FRONTIER $(INC) src/cli/cli.c src/kolmo.c $(FRONTIER_SRC) build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)

build/kolmo-tar-file-test: tests/test_tar_file.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) $(FRONTIER_SRC) $(FRONTIER_HDR) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) -DKOLMO_FRONTIER $(INC) $< $(FRONTIER_SRC) build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)

check-tar-files: build/kolmo-tar-file-test
	./build/kolmo-tar-file-test
.PHONY: check-tar-files

check-frontier-dna: build/kolmo build/kolmo-frontier
	python3 tests/test_frontier_dna.py $^
.PHONY: check-frontier-dna

build/frontier-test: tests/frontier.c $(FRONTIER_SRC) $(FRONTIER_HDR)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SANFLAGS) tests/frontier.c $(FRONTIER_SRC) -o $@


build/libkolmo-frontier.$(SOEXT): src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) $(FRONTIER_SRC) $(FRONTIER_HDR) $(PICC) $(ARCHPIC)
	$(CC) $(CFLAGS) $(FPFLAGS) -fPIC -shared $(DEFS) -DKOLMO_FRONTIER $(INC) src/kolmo.c $(FRONTIER_SRC) $(PICC) $(ARCHPIC) -o $@ $(LIBS)

# Standalone research codecs (not part of kolmo): see experimental.mk
include experimental.mk
