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
XZ      := vendor/xz
INC     := -Isrc -I$(ZSTD) -I$(LZ4) -I$(BRO)/include -I$(XZ) $(EXTRA_INC)
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
# liblzma: a pinned, verified subset of xz-utils 5.8.1 (raw LZMA2 encoder and decoder only), vendored in vendor/xz
# (see vendor/xz/README.kolmo). No system liblzma is needed. Its symbols are renamed to kolmo_lzma_* by a forced
# include, so the shared library can share a process with the system liblzma. NO_LZMA=1 still builds without it.
XZSRC   := $(wildcard $(XZ)/*.c $(XZ)/liblzma/*/*.c)
XZFLAGS := -DHAVE_CONFIG_H -include $(XZ)/kolmo_lzma_namespace.h -I$(XZ) -I$(XZ)/liblzma/api -I$(XZ)/liblzma/common \
           -I$(XZ)/liblzma/check -I$(XZ)/liblzma/lz -I$(XZ)/liblzma/lzma -I$(XZ)/liblzma/rangecoder -I$(XZ)/liblzma/simple \
           -I$(XZ)/liblzma/delta -I$(XZ)/common
ifeq ($(NO_LZMA),1)
DEFS    += -DKOLMO_NO_LZMA
XZSRC   :=
endif
LZMALIB :=
OBJ     := $(patsubst %.c,build/obj/%.o,$(ZSRC) $(BROSRC) $(XZSRC) $(LZ4)/lz4.c $(LZ4)/lz4hc.c) $(patsubst %.S,build/obj/%.o,$(ZASM))
# Vendored code is compiled one function per section and the linker drops what Kolmo never calls (the trim measured in
# benchmark/rc3/deps/RESULTS.md: about 370 KiB of zstd, LZ4, xxHash, Brotli and liblzma code). Same bytes out.
VSECT   :=
$(OBJ): VSECT := -ffunction-sections -fdata-sections
ifeq ($(UNAME_S),Darwin)
LDGC    := -Wl,-dead_strip
else
LDGC    := -Wl,--gc-sections
endif
LIBS    := $(EXTRA_LIB) $(LZMALIB) $(LDGC) -lpthread -lm
BLIBS   := $(LIBS)

# Library sources (docs/LAYOUT.md). src/kolmo.c includes the parts below into one translation unit, so they are listed
# by directory as its prerequisites. Compiled on their own and archived with it: arch/xxh3.c and the assembly kernels of
# this machine's ISA, src/arch/<isa>/*.S (NO_ASM=1 drops them and builds the C references only). The GPU
# worker of the rc3 gpu lane is not built (see below).
LIBDIRS := api core plan transforms codecs algo arch arch/x86_64 arch/aarch64
KOLMO_INCLUDES := src/kolmo.h $(foreach d,$(LIBDIRS),$(wildcard src/$(d)/*.c src/$(d)/*.h)) src/plan/target_table.inc
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
# GPU worker (Metal) from the rc3 gpu lane: research code, not built. Its BWT path plugged into libbsc's GPU hook,
# which rc3 removed together with libbsc, so GPU=1 needs a port to src/algo/sais first. The lane measured no gain
# from running GPU and CPU together on the M5, and 0.8 ms more per process start from linking Metal
# (benchmark/rc3/gpu/RESULTS.md). The sources stay in src/arch/metal and src/core/hetero.c.
GPUSRC  :=
ifeq ($(GPU),1)
$(error GPU=1 is not available in rc3: the Metal BWT hook targeted libbsc, which rc3 removed; see benchmark/rc3/gpu/RESULTS.md)
endif
# Kernels called through the dispatch table of src/arch/cpu.c (arch/kernels.h), each object on its own: the C
# references in src/algo, and on x86-64 the SSE2/SSE4.1/AVX2 versions (intrinsics with target attributes, chosen by
# CPUID at first use). NO_ASM=1 keeps the references only.
KSRC    := src/arch/cpu.c src/algo/predict.c src/algo/planes.c src/algo/mixer.c src/algo/video.c
ifeq ($(ASM_ISA)$(NO_ASM),x86_64)
KSRC    += src/arch/x86_64/med8_sse2.c src/arch/x86_64/planes_sse2.c src/arch/x86_64/mixer_avx2.c src/arch/x86_64/video_sse2.c \
           src/arch/x86_64/img16_sse2.c src/arch/x86_64/med8f_avx2.c
endif
KHDR    := src/arch/kernels.h src/algo/predict.h src/algo/planes.h src/algo/mixer.h src/algo/video.h $(wildcard src/arch/x86_64/*.h)
# The block-sorting pipeline (LZP, QLFC, the libsais subset, the fork-join helper, the BWT stream formats) is compiled
# on its own too: these files have public kolmo_* entry points and are not included by src/kolmo.c.
BWTSRC  := src/algo/lzp.c src/algo/qlfc.c src/algo/par.c $(wildcard src/algo/sais/*.c) src/codecs/bwt_stream.c
BWTHDR  := $(wildcard src/algo/*.h src/algo/sais/*.h) src/codecs/bwt_stream.h
ARCHSRC := src/arch/xxh3.c $(KSRC) $(BWTSRC) $(ASMSRC) $(GPUSRC)
ARCHOBJ := $(addprefix build/obj/,$(addsuffix .o,$(basename $(ARCHSRC))))
ARCHPIC := $(addprefix build/pic/,$(addsuffix .o,$(basename $(ARCHSRC))))
$(ARCHOBJ): VSECT := -ffunction-sections -fdata-sections   # arch/xxh3.c carries xxHash (see VSECT above)

all: build/kolmo build/kolmobench build/kolmotest build/kolmoparallel build/kolmoresource

# liblzma first: GNU make 3.81 (macOS) takes the first matching pattern rule, make 4 the most specific one
build/obj/$(XZ)/%.o: $(XZ)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(VSECT) $(XZFLAGS) -c $< -o $@
build/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(VSECT) $(DEFS) $(INC) -c $< -o $@
build/obj/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEFS) $(INC) -c $< -o $@
build/obj/%.o: %.m
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEFS) $(INC) -fobjc-arc -c $< -o $@

# LZ4's decoder loop is sensitive to where the linker places it: on the Apple M5 the same lz4.o decoded 20% slower
# (-m speed, JSON and logs) after unrelated code grew the binary. 64-byte function and loop alignment makes the
# speed independent of placement (benchmark/rc3/simd-arm/RESULTS.md). Code placement only: same output.
LZ4ALIGN := -falign-functions=64 -falign-loops=64
build/obj/$(LZ4)/lz4.o build/pic/$(LZ4)/lz4.o: override CFLAGS += $(LZ4ALIGN)
build/libvendor.a: $(OBJ)
	ar rcs $@ $^
build/kolmo.o: src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES)
	@mkdir -p build
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) -c src/kolmo.c -o $@
$(ARCHOBJ) $(ARCHPIC): $(KHDR)
$(patsubst %.c,build/obj/%.o,$(BWTSRC)) $(patsubst %.c,build/pic/%.o,$(BWTSRC)): $(BWTHDR)
build/libkolmoarch.a: $(ARCHOBJ)
	ar rcs $@ $^
build/libkolmo.a: build/kolmo.o $(ARCHOBJ)
	ar rcs $@ $^
build/kolmo: src/cli/cli.c src/cli/train.c src/cli/features.c build/libkolmo.a build/libvendor.a
	$(CC) $(CFLAGS) $(INC) $(filter-out src/cli/train.c,$^) -o $@ $(LIBS)
build/kolmobench: bench/kolmobench.c build/libkolmo.a build/libvendor.a
	$(CC) $(CFLAGS) $(DEFS) $(INC) $^ -o $@ $(BLIBS)
# features-a benchmark: trained profiles against untrained Kolmo and zstd dictionaries (benchmark/rc3/features-a)
build/kpbench: benchmark/rc3/features-a/kpbench.c build/libkolmo.a build/libvendor.a
	$(CC) $(CFLAGS) $(DEFS) $(INC) $^ -o $@ $(LIBS)
build/tgbench: benchmark/rc3/features-a/tgbench.c build/libkolmo.a build/libvendor.a
	$(CC) $(CFLAGS) $(DEFS) $(INC) $^ -o $@ $(LIBS)
build/kolmotest: tests/kolmotest.c build/libkolmo.a build/libvendor.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS)
build/kolmoparallel: tests/kolmoparallel.c build/libkolmo.a build/libvendor.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS)
build/kolmoresource: tests/kolmoresource.c tests/legacy_frames.h src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/kolmoresource.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
build/kolmo-profile: src/cli/cli.c src/cli/train.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) -DKOLMO_TIMING $(INC) src/cli/cli.c src/cli/features.c src/kolmo.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)

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
build/test-cmpar-asan: tests/test_cmpar.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_cmpar.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
check-cmpar: build/test-cmpar-asan
	./build/test-cmpar-asan
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
build/test-dispatch-asan: tests/test_dispatch.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) $(KSRC) $(KHDR) build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_dispatch.c src/kolmo.c src/arch/xxh3.c $(KSRC) $(BWTSRC) $(ASMSRC) build/libvendor.a -o $@ $(LIBS)
check-dispatch: build/test-dispatch build/test-dispatch-asan
	./build/test-dispatch
	./build/test-dispatch-asan
# Dispatched kernels (src/arch) against their C references (src/algo), at every ISA level of this CPU; also with
# sanitizers.
build/test-kernels: tests/test_kernels.c build/libkolmoarch.a
	$(CC) $(CFLAGS) $(DEFS) $(INC) $^ -o $@ -lpthread -lm
build/test-kernels-asan: tests/test_kernels.c $(KSRC) $(KHDR)
	$(CC) $(SANFLAGS) $(DEFS) $(INC) tests/test_kernels.c $(KSRC) -o $@ -lpthread -lm
check-kernels: build/test-kernels build/test-kernels-asan
	./build/test-kernels
	./build/test-kernels-asan
# Scanner helpers of the table and log transforms (population counts, SWAR decimal parser) against references.
build/test-scan-asan: tests/test_scan.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_scan.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
check-scan: build/test-scan-asan
	./build/test-scan-asan
.PHONY: check-kernels check-scan
# Block-sorting codec: codec 5 bytes against libbsc's (golden hashes), round trips at 1 and 4 threads, damaged streams.
build/test-bwt: tests/test_bwt.c tests/test_bwt_golden.h $(BWTSRC) $(BWTHDR)
	$(CC) $(CFLAGS) -Isrc tests/test_bwt.c $(BWTSRC) -o $@ -lpthread
build/test-bwt-asan: tests/test_bwt.c tests/test_bwt_golden.h $(BWTSRC) $(BWTHDR)
	$(CC) $(SANFLAGS) -Isrc tests/test_bwt.c $(BWTSRC) -o $@ -lpthread
check-bwt: build/test-bwt build/test-bwt-asan
	./build/test-bwt
	./build/test-bwt-asan
.PHONY: check-bwt
# Assembly kernels (src/arch/<isa>/*.S) against their C references, optimised and with sanitizers; with NO_ASM=1 the
# references only.
build/test-asm: tests/test_asm.c src/kolmo.c $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
build/test-asm-asan: tests/test_asm.c src/kolmo.c $(KOLMO_INCLUDES) $(ARCHSRC) build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< $(ARCHSRC) build/libvendor.a -o $@ $(LIBS)
check-asm: build/test-asm build/test-asm-asan
	./build/test-asm
	./build/test-asm-asan
# CPU kernels (src/arch) against their C references, and whole frames with the kernels on and off; optimised and
# with sanitizers.
build/test-arch: tests/test_arch.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
build/test-arch-asan: tests/test_arch.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
check-arch: build/test-arch build/test-arch-asan
	./build/test-arch
	./build/test-arch-asan
# Format 2 (codecs 19-23, container version 11): leaves, audio and video blocks, damaged payloads, file API
build/test-format2: tests/test_format2.c src/kolmo.c $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
build/test-format2-asan: tests/test_format2.c src/kolmo.c $(KOLMO_INCLUDES) $(ARCHSRC) build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< $(ARCHSRC) build/libvendor.a -o $@ $(LIBS)
check-format2: build/test-format2 build/test-format2-asan
	./build/test-format2
	./build/test-format2-asan
# Metadata (plan/meta.c: header parsers on valid and mutated files) and the ARM64 branch transform
build/test-meta: tests/test_meta.c src/kolmo.c $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
build/test-meta-asan: tests/test_meta.c src/kolmo.c $(KOLMO_INCLUDES) $(ARCHSRC) build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< $(ARCHSRC) build/libvendor.a -o $@ $(LIBS)
check-meta: build/test-meta build/test-meta-asan
	./build/test-meta
	./build/test-meta-asan
# Trained profiles and --target (features-a): round trips, damaged profiles and frames, targets, merging.
build/test-profile: tests/test_profile.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
build/test-profile-asan: tests/test_profile.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)
check-profile: build/test-profile build/test-profile-asan
	./build/test-profile
	KOLMO_SKIP_CALIBRATE=1 ./build/test-profile-asan
.PHONY: check-profile
# Queryable archives (IDX1, column-split table blocks) and deduplicating archives (DDP1), with sanitizers and optimised
build/test-features-b-asan: tests/test_features_b.c src/kolmo.c $(KOLMO_INCLUDES) $(ARCHSRC) build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_features_b.c src/kolmo.c $(ARCHSRC) build/libvendor.a -o $@ $(LIBS)
build/test-features-b: tests/test_features_b.c build/libkolmo.a build/libvendor.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS)
check-features-b: build/test-features-b build/test-features-b-asan build/kolmo
	./build/test-features-b
	./build/test-features-b-asan 7
	sh tests/test_features_b_cli.sh build/kolmo
.PHONY: check-features-b
check-rc: check-meta check-features-b check-bwt check-kernels check-scan check-lines check-dec64 check-fastint check-planner-ownership check-cmimg check-cmpar check-rc-options check-fqfam check-dispatch check-asm check-arch check-format2 check-profile
.PHONY: check-meta check-rc check-features-b check-bwt check-kernels check-scan check-lines check-dec64 check-fastint check-planner-ownership check-cmimg check-cmpar check-fqfam check-dispatch check-asm check-arch check-format2
check-sanitizers: build/kolmoparallel-asan build/kolmoresource-asan
	./build/kolmoresource-asan
	./build/kolmoparallel-asan

# Shared library for language bindings (Python, Rust): everything rebuilt -fPIC into build/pic.
SOEXT := $(if $(filter Darwin,$(UNAME_S)),dylib,so)
PICC  := $(patsubst %.c,build/pic/%.o,$(ZSRC) $(BROSRC) $(XZSRC) $(LZ4)/lz4.c $(LZ4)/lz4hc.c) $(patsubst %.S,build/pic/%.o,$(ZASM))
build/pic/$(XZ)/%.o: $(XZ)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fPIC $(XZFLAGS) -c $< -o $@
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
	$(CC) $(CFLAGS) $(FPFLAGS) -DCM_BB_MAX=20 $(DEFS) $(INC) src/cli/cli.c src/cli/features.c src/kolmo.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)

# The compact variant retains the standard Ultra bitstream and table sizes elsewhere.
build/kolmo-cm-compact: src/cli/cli.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) -DKOLMO_CM_COMPACT $(DEFS) $(INC) src/cli/cli.c src/cli/features.c src/kolmo.c build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)

build/cm-reference.o: tests/test_cm_compact.c src/codecs/cm.c $(KHDR)
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/cm-compact-test: tests/test_cm_compact.c src/codecs/cm.c build/cm-reference.o build/libkolmoarch.a
	$(CC) $(CFLAGS) -DKOLMO_CM_COMPACT $< build/cm-reference.o build/libkolmoarch.a -lpthread -lm -o $@

check-cm-compact: build/cm-compact-test
	./build/cm-compact-test
.PHONY: check-cm-compact

# Research build. Experimental recipes emit v6 frames only when used.
FRONTIER_SRC := src/experimental/frontier_dna.c src/experimental/frontier_table.c src/experimental/frontier_prefix.c src/experimental/frontier_pcm.c src/experimental/frontier_lag.c src/experimental/frontier_shuffle.c src/experimental/frontier_runs.c
FRONTIER_HDR := src/experimental/frontier_dna.h src/experimental/frontier_table.h src/experimental/frontier_prefix.h src/experimental/frontier_pcm.h src/experimental/frontier_lag.h src/experimental/frontier_shuffle.h src/experimental/frontier_runs.h
build/kolmo-frontier: src/cli/cli.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) $(FRONTIER_SRC) $(FRONTIER_HDR) build/libkolmoarch.a build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) -DKOLMO_FRONTIER $(INC) src/cli/cli.c src/cli/features.c src/kolmo.c $(FRONTIER_SRC) build/libkolmoarch.a build/libvendor.a -o $@ $(LIBS)

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
