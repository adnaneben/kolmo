# kolmo — structure-aware compressor.  make -> build/kolmo, build/kolmobench, build/libkolmo.a
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
CC      ?= clang
ARCH    ?= -mcpu=native
CFLAGS  ?= -O3 $(ARCH)
else
CC      ?= gcc
ARCH    ?= -march=native
CFLAGS  ?= -O3 $(ARCH) -fno-plt
endif
FPFLAGS := -ffp-contract=off
# No floating-point contraction anywhere: Kolmo's encoders decide some choices with float costs (context mixing,
# the planner's estimates); with -mfma, -march=native or on arm64 the compiler would otherwise fuse those into FMA and
# could choose differently on different builds, so the frames would differ. Whatever CFLAGS says.
override CFLAGS += $(FPFLAGS)
# Every coder is Kolmo's own: no vendored or system codec library is built or linked (zstd, Brotli, LZ4, liblzma and
# xxHash were removed with container version 12).
INC     := -Isrc
DEFS    :=
VSECT   :=
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_S),Darwin)
LDGC    := -Wl,-dead_strip
else
LDGC    := -Wl,--gc-sections
endif
LIBS    := $(LDGC) -lpthread -lm
BLIBS   := $(LIBS)

# Library sources (docs/LAYOUT.md). src/kolmo.c includes the parts below into one translation unit, so they are listed
# by directory as its prerequisites. Compiled on their own and archived with it: the assembly kernels of
# this machine's ISA, src/arch/<isa>/*.S (NO_ASM=1 drops them and builds the C references only). The GPU
# worker of the rc3 gpu lane is not built (see below).
LIBDIRS := api core plan transforms codecs algo arch arch/x86_64 arch/aarch64
KOLMO_INCLUDES := src/kolmo.h $(foreach d,$(LIBDIRS),$(wildcard src/$(d)/*.c src/$(d)/*.h)) src/plan/target_table.inc src/codecs/cm_prior.inc
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
# Kolmo's own LZ, Huffman, LZMA-class coders and block hash (codecs 24, 27-29, 32; kolmo_khash64), compiled on their
# own like the block-sorting files and listed with them so every build and test that links those links these.
NATSRC  := src/algo/khuf.c src/algo/krans.c src/algo/klz.c src/algo/klzma.c src/algo/khash.c
BWTSRC  += $(NATSRC)
BWTHDR  := $(wildcard src/algo/*.h src/algo/sais/*.h) src/codecs/bwt_stream.h
ARCHSRC := $(KSRC) $(BWTSRC) $(ASMSRC) $(GPUSRC)
ARCHOBJ := $(addprefix build/obj/,$(addsuffix .o,$(basename $(ARCHSRC))))
ARCHPIC := $(addprefix build/pic/,$(addsuffix .o,$(basename $(ARCHSRC))))
$(ARCHOBJ): VSECT := -ffunction-sections -fdata-sections   # Kolmo's coders: unused functions are dropped at link time

all: build/kolmo build/kolmobench build/kolmotest build/kolmoparallel build/kolmoresource build/test-levels

build/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(VSECT) $(DEFS) $(INC) -c $< -o $@
build/obj/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEFS) $(INC) -c $< -o $@
build/obj/%.o: %.m
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEFS) $(INC) -fobjc-arc -c $< -o $@

build/kolmo.o: src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES)
	@mkdir -p build
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) -c src/kolmo.c -o $@
$(ARCHOBJ) $(ARCHPIC): $(KHDR)
$(patsubst %.c,build/obj/%.o,$(BWTSRC)) $(patsubst %.c,build/pic/%.o,$(BWTSRC)): $(BWTHDR)
build/libkolmoarch.a: $(ARCHOBJ)
	ar rcs $@ $^
build/libkolmo.a: build/kolmo.o $(ARCHOBJ)
	ar rcs $@ $^
build/kolmo: src/cli/cli.c src/cli/train.c src/cli/features.c build/libkolmo.a
	$(CC) $(CFLAGS) $(INC) $(filter-out src/cli/train.c,$^) -o $@ $(LIBS)
build/kolmobench: bench/kolmobench.c build/libkolmo.a
	$(CC) $(CFLAGS) $(DEFS) $(INC) $^ -o $@ $(BLIBS)
build/kolmotest: tests/kolmotest.c build/libkolmo.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS)
build/test-levels: tests/test_levels.c build/libkolmo.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS)
build/test-levels-asan: tests/test_levels.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_levels.c src/kolmo.c build/libkolmoarch.a -o $@ $(LIBS)
build/kolmoparallel: tests/kolmoparallel.c build/libkolmo.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS)
build/kolmoresource: tests/kolmoresource.c tests/legacy_frames.h src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/kolmoresource.c build/libkolmoarch.a -o $@ $(LIBS)
# Research harness of the impact matrix (benchmark/impact/): the CLI with -DKOLMO_IMPACT (src/core/impact.c), which
# can force a codec or drop a transform per block through KOLMO_IMPACT_* variables. Not part of `all`; never shipped.
kolmo-impact: build/kolmo-impact
build/kolmo-impact: src/cli/cli.c src/cli/train.c src/core/impact.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) -DKOLMO_IMPACT $(INC) src/cli/cli.c src/cli/features.c src/kolmo.c build/libkolmoarch.a -o $@ $(LIBS)
build/kolmo-profile: src/cli/cli.c src/cli/train.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) -DKOLMO_TIMING $(INC) src/cli/cli.c src/cli/features.c src/kolmo.c build/libkolmoarch.a -o $@ $(LIBS)

check: all build/test-decoder-hardening build/test-harden build/test-v12-corpus
	./build/test-levels
	python3 tests/test_cli.py build/kolmo
	sh tests/test_filter.sh build/kolmo
	python3 tests/test_fast_file.py
	./build/test-v12-corpus tests/v12
	python3 tests/check_v12_frames.py build/kolmo tests/v12
	python3 tests/format_reference.py tests/v12
	python3 tests/coder_reference.py tests/v12
	python3 tests/bwt_reference.py tests/v12
	python3 tests/kimg_reference.py tests/v12
	./build/test-harden
	./build/kolmoresource
	./build/kolmoparallel
	./build/kolmotest
	./build/test-decoder-hardening

# decoder memory-safety and DoS regressions (review r1 of 0.0.1). The ASan build compiles the separately built coders
# (qlfc, khuf, klzma, ...) with the sanitizers instead of taking them from libkolmoarch.a.
build/test-decoder-hardening: tests/test_decoder_hardening.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_decoder_hardening.c build/libkolmoarch.a -o $@ $(LIBS)
build/test-decoder-hardening-asan: tests/test_decoder_hardening.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) $(BWTSRC) $(BWTHDR) build/libkolmoarch.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_decoder_hardening.c $(BWTSRC) build/libkolmoarch.a -o $@ $(LIBS)
check-decoder-hardening: build/test-decoder-hardening-asan
	./build/test-decoder-hardening-asan

build/test-harden: tests/test_harden.c build/libkolmo.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS)
build/test-v12-corpus: tests/test_v12_corpus.c build/libkolmo.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS)

SANFLAGS ?= -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
override SANFLAGS += $(FPFLAGS)
# The coders compiled on their own (ARCHSRC: KLZ, KLZH2, KLZMA, khuf, krans, the block-sorting pipeline, the kernels)
# are instrumented too in every sanitizer build: they are archived from objects built with SANFLAGS, not CFLAGS.
ARCHASAN := $(addprefix build/asan/,$(addsuffix .o,$(basename $(ARCHSRC))))
build/asan/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(SANFLAGS) $(DEFS) $(INC) -c $< -o $@
build/asan/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(SANFLAGS) $(DEFS) $(INC) -c $< -o $@
$(ARCHASAN): $(KHDR)
$(patsubst %.c,build/asan/%.o,$(BWTSRC)): $(BWTHDR)
build/libkolmoarch-asan.a: $(ARCHASAN)
	ar rcs $@ $^
build/kolmoparallel-asan: tests/kolmoparallel.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/kolmoparallel.c src/kolmo.c build/libkolmoarch-asan.a -o $@ $(LIBS)
build/kolmoresource-asan: tests/kolmoresource.c tests/legacy_frames.h src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/kolmoresource.c build/libkolmoarch-asan.a -o $@ $(LIBS)
build/test-lines-asan: tests/test_lines.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_lines.c build/libkolmoarch-asan.a -o $@ $(LIBS)
check-lines: build/test-lines-asan
	./build/test-lines-asan
build/test-dec64-asan: tests/test_dec64.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_dec64.c build/libkolmoarch-asan.a -o $@ $(LIBS)
check-dec64: build/test-dec64-asan
	./build/test-dec64-asan
build/test-fastint-asan: tests/test_fastint.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_fastint.c build/libkolmoarch-asan.a -o $@ $(LIBS)
# The same test in the optimised build (the loops a release runs), and in the control build that sends every program
# to the generic inverse (-DKOLMO_FORCE_GENERIC_INT_INV).
build/test-fastint: tests/test_fastint.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_fastint.c build/libkolmoarch.a -o $@ $(LIBS)
build/test-fastint-generic: tests/test_fastint.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) -DKOLMO_FORCE_GENERIC_INT_INV $(INC) tests/test_fastint.c build/libkolmoarch.a -o $@ $(LIBS)
check-fastint: build/test-fastint-asan build/test-fastint build/test-fastint-generic
	./build/test-fastint-asan
	./build/test-fastint
	./build/test-fastint-generic
build/test-planner-ownership-asan: tests/test_planner_ownership.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_planner_ownership.c build/libkolmoarch-asan.a -o $@ $(LIBS)
check-planner-ownership: build/test-planner-ownership-asan
	./build/test-planner-ownership-asan
build/test-cmimg-asan: tests/test_cmimg.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_cmimg.c build/libkolmoarch-asan.a -o $@ $(LIBS)
check-cmimg: build/test-cmimg-asan
	./build/test-cmimg-asan
build/test-cmpar-asan: tests/test_cmpar.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_cmpar.c build/libkolmoarch-asan.a -o $@ $(LIBS)
check-cmpar: build/test-cmpar-asan
	./build/test-cmpar-asan
build/test-cm4-asan: tests/test_cm4.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_cm4.c build/libkolmoarch-asan.a -o $@ $(LIBS)
check-cm4: build/test-cm4-asan
	./build/test-cm4-asan
# Opt-in --typed-huf route (ported from the RC1 integration, PR #10): round trips, header parsing, and agreement of
# the typed route's predictor choice with the graph planner's.
build/test-rc-options: tests/test_rc_options.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch.a -o $@ $(LIBS)
build/test-rc-options-asan: tests/test_rc_options.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch-asan.a -o $@ $(LIBS)
check-rc-options: build/test-rc-options build/test-rc-options-asan
	./build/test-rc-options
	./build/test-rc-options-asan
.PHONY: check-rc-options
# FASTQ family blocks (codec 16): container round trips at ultra, the modes that must not write it, damaged payloads
build/test-fqfam-asan: tests/test_fqfam.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_fqfam.c build/libkolmoarch-asan.a -o $@ $(LIBS)
check-fqfam: build/test-fqfam-asan
	./build/test-fqfam-asan
# Component tests of the features merged for the release candidate. `make check` does not run them; CI does.
# Runtime ISA dispatch (x86-64): CRC32C against a scalar reference, kolmo_khash64 independent of alignment; optimised
# and with sanitizers.
build/test-dispatch: tests/test_dispatch.c build/libkolmo.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS)
build/test-dispatch-asan: tests/test_dispatch.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) $(KSRC) $(KHDR)
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_dispatch.c src/kolmo.c $(KSRC) $(BWTSRC) $(ASMSRC) -o $@ $(LIBS)
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
build/test-scan-asan: tests/test_scan.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_scan.c build/libkolmoarch-asan.a -o $@ $(LIBS)
check-scan: build/test-scan-asan
	./build/test-scan-asan
.PHONY: check-kernels check-scan
# Block-sorting codec: codec 5 bytes against libbsc's (golden hashes), round trips at 1 and 4 threads, damaged streams.
build/test-bwt: tests/test_bwt.c tests/test_bwt_golden.h $(BWTSRC) $(BWTHDR)
	$(CC) $(CFLAGS) -Isrc tests/test_bwt.c $(BWTSRC) -o $@ -lpthread
build/test-bwt-asan: tests/test_bwt.c tests/test_bwt_golden.h $(BWTSRC) $(BWTHDR)
	$(CC) $(SANFLAGS) -Isrc tests/test_bwt.c $(BWTSRC) -o $@ -lpthread
# Kolmo's own coders (codecs 24, 27-29, 32, src/algo/k*.c): round trips, corrupt payloads under ASan/UBSan, pinned hash
# values; then every mode through the CLI: version 12, no retired codec id, 1 and 4 threads alike, KOLMO_NATIVE ignored.
build/test-native: tests/test_native.c $(NATSRC) $(wildcard src/algo/k*.h)
	$(CC) $(CFLAGS) -Isrc tests/test_native.c $(NATSRC) -o $@
build/test-native-asan: tests/test_native.c $(NATSRC) $(wildcard src/algo/k*.h)
	$(CC) $(SANFLAGS) -fno-sanitize-recover=undefined -Isrc tests/test_native.c $(NATSRC) -o $@
check-native: build/test-native build/test-native-asan build/kolmo
	./build/test-native 3
	./build/test-native-asan
	python3 tests/test_native_modes.py build/kolmo build/native-modes
.PHONY: check-native
check-bwt: build/test-bwt build/test-bwt-asan
	./build/test-bwt
	./build/test-bwt-asan
.PHONY: check-bwt
# Assembly kernels (src/arch/<isa>/*.S) against their C references, optimised and with sanitizers; with NO_ASM=1 the
# references only.
build/test-asm: tests/test_asm.c src/kolmo.c $(KOLMO_INCLUDES) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch.a -o $@ $(LIBS)
build/test-asm-asan: tests/test_asm.c src/kolmo.c $(KOLMO_INCLUDES) $(ARCHSRC)
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< $(ARCHSRC) -o $@ $(LIBS)
check-asm: build/test-asm build/test-asm-asan
	./build/test-asm
	./build/test-asm-asan
# CPU kernels (src/arch) against their C references, and whole frames with the kernels on and off; optimised and
# with sanitizers.
build/test-arch: tests/test_arch.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch.a -o $@ $(LIBS)
build/test-arch-asan: tests/test_arch.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch-asan.a -o $@ $(LIBS)
check-arch: build/test-arch build/test-arch-asan
	./build/test-arch
	./build/test-arch-asan
# Format 2 (codecs 19-21, 23; 22 refused): leaves, audio and video blocks, damaged payloads, file API
build/test-format2: tests/test_format2.c src/kolmo.c $(KOLMO_INCLUDES) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch.a -o $@ $(LIBS)
build/test-format2-asan: tests/test_format2.c src/kolmo.c $(KOLMO_INCLUDES) $(ARCHSRC)
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< $(ARCHSRC) -o $@ $(LIBS)
check-format2: build/test-format2 build/test-format2-asan
	./build/test-format2
	./build/test-format2-asan
# Metadata (plan/meta.c: header parsers on valid and mutated files) and the ARM64 branch transform
build/test-meta: tests/test_meta.c src/kolmo.c $(KOLMO_INCLUDES) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch.a -o $@ $(LIBS)
build/test-meta-asan: tests/test_meta.c src/kolmo.c $(KOLMO_INCLUDES) $(ARCHSRC)
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< $(ARCHSRC) -o $@ $(LIBS)
check-meta: build/test-meta build/test-meta-asan
	./build/test-meta
	./build/test-meta-asan
# --target (features-a): parsing, calibration, chunk merging; trained profiles and dictionaries report
# KOLMO_ERR_UNSUPPORTED (not available without library codecs).
build/test-profile: tests/test_profile.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch.a -o $@ $(LIBS)
build/test-profile-asan: tests/test_profile.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch-asan.a -o $@ $(LIBS)
check-profile: build/test-profile build/test-profile-asan
	./build/test-profile
	KOLMO_SKIP_CALIBRATE=1 ./build/test-profile-asan
.PHONY: check-profile
# Queryable archives (IDX1, column-split table blocks) and deduplicating archives (DDP1), with sanitizers and optimised
build/test-features-b-asan: tests/test_features_b.c src/kolmo.c $(KOLMO_INCLUDES) $(ARCHSRC)
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_features_b.c src/kolmo.c $(ARCHSRC) -o $@ $(LIBS)
build/test-features-b: tests/test_features_b.c build/libkolmo.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS)
check-features-b: build/test-features-b build/test-features-b-asan build/kolmo
	./build/test-features-b
	./build/test-features-b-asan 7
	sh tests/test_features_b_cli.sh build/kolmo
.PHONY: check-features-b
# The memory model (docs/MEMORY-MODEL.md): frame declaration, governor, budgets mid-run, forced budgets, forged frames
build/test-memdyn: tests/test_memdyn.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch.a -o $@ $(LIBS)
build/test-memdyn-asan: tests/test_memdyn.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libkolmoarch-asan.a -o $@ $(LIBS)
check-memdyn: build/test-memdyn build/test-memdyn-asan
	./build/test-memdyn
	./build/test-memdyn-asan --quick
.PHONY: check-memdyn
check-rc: check-decoder-hardening check-meta check-memdyn check-features-b check-native check-bwt check-kernels check-scan check-lines check-dec64 check-fastint check-planner-ownership check-cmimg check-cmpar check-cm4 check-rc-options check-fqfam check-dispatch check-asm check-arch check-format2 check-profile
.PHONY: check-meta check-rc check-features-b check-bwt check-kernels check-scan check-lines check-dec64 check-fastint check-planner-ownership check-cmimg check-cmpar check-cm4 check-fqfam check-dispatch check-asm check-arch check-format2
check-sanitizers: build/kolmoparallel-asan build/kolmoresource-asan build/test-levels-asan build/test-harden-asan build/test-v12-corpus-asan
	./build/test-levels-asan
	./build/test-v12-corpus-asan tests/v12
	./build/test-harden-asan
	./build/kolmoresource-asan
	./build/kolmoparallel-asan
build/test-harden-asan: tests/test_harden.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_harden.c src/kolmo.c build/libkolmoarch-asan.a -o $@ $(LIBS)
build/test-v12-corpus-asan: tests/test_v12_corpus.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch-asan.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_v12_corpus.c src/kolmo.c build/libkolmoarch-asan.a -o $@ $(LIBS)

# Shared library for language bindings (Python, Rust): everything rebuilt -fPIC into build/pic.
SOEXT := $(if $(filter Darwin,$(UNAME_S)),dylib,so)
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
build/libkolmo.$(SOEXT): src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) $(ARCHPIC)
	@mkdir -p build
	$(CC) $(CFLAGS) $(FPFLAGS) -fPIC -shared $(DEFS) $(INC) src/kolmo.c $(ARCHPIC) -o $@ $(LIBS)

clean:
	rm -rf build
.PHONY: all clean lib check check-sanitizers

# Optional lower-memory archive encoder.
build/kolmo-cm20: src/cli/cli.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) -DCM_BB_MAX=20 $(DEFS) $(INC) src/cli/cli.c src/cli/features.c src/kolmo.c build/libkolmoarch.a -o $@ $(LIBS)

# The compact variant retains the standard Ultra bitstream and table sizes elsewhere.
build/kolmo-cm-compact: src/cli/cli.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) -DKOLMO_CM_COMPACT $(DEFS) $(INC) src/cli/cli.c src/cli/features.c src/kolmo.c build/libkolmoarch.a -o $@ $(LIBS)

build/cm-reference.o: tests/test_cm_compact.c src/codecs/cm.c $(KHDR)
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/cm-compact-test: tests/test_cm_compact.c src/codecs/cm.c build/cm-reference.o build/libkolmoarch.a
	$(CC) $(CFLAGS) -DKOLMO_CM_COMPACT $< build/cm-reference.o build/libkolmoarch.a -lpthread -lm -o $@

check-cm-compact: build/cm-compact-test
	./build/cm-compact-test
.PHONY: check-cm-compact

# Research build (-DKOLMO_FRONTIER, not part of `make all`). Its experimental table/lag blocks are coded with R_LZH
# (KLZH2, codec 32) since version 12; nothing in it needs a codec library.
FRONTIER_SRC := src/experimental/frontier_dna.c src/experimental/frontier_table.c src/experimental/frontier_prefix.c src/experimental/frontier_pcm.c src/experimental/frontier_lag.c src/experimental/frontier_shuffle.c src/experimental/frontier_runs.c
FRONTIER_HDR := src/experimental/frontier_dna.h src/experimental/frontier_table.h src/experimental/frontier_prefix.h src/experimental/frontier_pcm.h src/experimental/frontier_lag.h src/experimental/frontier_shuffle.h src/experimental/frontier_runs.h
build/kolmo-frontier: src/cli/cli.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) $(FRONTIER_SRC) $(FRONTIER_HDR) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) -DKOLMO_FRONTIER $(INC) src/cli/cli.c src/cli/features.c src/kolmo.c $(FRONTIER_SRC) build/libkolmoarch.a -o $@ $(LIBS)

build/kolmo-tar-file-test: tests/test_tar_file.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) $(FRONTIER_SRC) $(FRONTIER_HDR) build/libkolmoarch.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) -DKOLMO_FRONTIER $(INC) $< $(FRONTIER_SRC) build/libkolmoarch.a -o $@ $(LIBS)

check-tar-files: build/kolmo-tar-file-test
	./build/kolmo-tar-file-test
.PHONY: check-tar-files

check-frontier-dna: build/kolmo build/kolmo-frontier
	python3 tests/test_frontier_dna.py $^
.PHONY: check-frontier-dna

build/frontier-test: tests/frontier.c $(FRONTIER_SRC) $(FRONTIER_HDR)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SANFLAGS) tests/frontier.c $(FRONTIER_SRC) -o $@


build/libkolmo-frontier.$(SOEXT): src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) $(FRONTIER_SRC) $(FRONTIER_HDR) $(ARCHPIC)
	$(CC) $(CFLAGS) $(FPFLAGS) -fPIC -shared $(DEFS) -DKOLMO_FRONTIER $(INC) src/kolmo.c $(FRONTIER_SRC) $(ARCHPIC) -o $@ $(LIBS)

# Standalone research codecs (not part of kolmo): see experimental.mk. None of them uses a codec library. Bench tools
# that compare against zstd, LZ4, Brotli or xz (benchmark/rc3w2/native/natbench.c and other benchmark/ sources) are
# not built by this Makefile and are left as they were; they need those libraries from the system to build.
include experimental.mk
