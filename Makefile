# kolmo — structure-aware compressor.  make -> build/kolmo, build/kolmobench, build/libkolmo.a
UNAME_S := $(shell uname -s)
CXX     ?= g++
ifeq ($(UNAME_S),Darwin)
CC      ?= clang
ARCH    ?= -mcpu=native
CFLAGS  ?= -O3 $(ARCH)
BREW    := $(shell brew --prefix 2>/dev/null)
EXTRA_INC := -I$(BREW)/include
EXTRA_LIB := -L$(BREW)/lib
CXXLIB  := -lc++
else
CXXLIB  := -lstdc++
CC      ?= gcc
ARCH    ?= -march=native
CFLAGS  ?= -O3 $(ARCH) -fno-plt
endif
FPFLAGS := -ffp-contract=off
ZSTD    := vendor/zstd
LZ4     := vendor/lz4
BRO     := vendor/brotli
INC     := -Isrc -I$(ZSTD) -I$(LZ4) -Ivendor/libbsc -I$(BRO)/include $(EXTRA_INC)
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
BSC     := vendor/libbsc/libbsc
BSRC    := $(shell find $(BSC) -name '*.cpp')
OBJ     := $(patsubst %.c,build/obj/%.o,$(ZSRC) $(BROSRC) vendor/xxhash/kolmo_xxh3.c $(LZ4)/lz4.c $(LZ4)/lz4hc.c $(BSC)/bwt/libsais/libsais.c) $(patsubst %.S,build/obj/%.o,$(ZASM)) \
           $(patsubst %.cpp,build/obj/%.o,$(BSRC))
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

KOLMO_INCLUDES := src/tbl2.inc src/log.inc src/lines.inc src/sci.inc src/word.inc src/cm.inc src/stream.inc src/kgraph.inc src/nplane.inc src/kg_int_fast.inc src/typed_int_encoder.inc src/csv_archive_encoder.inc

all: build/kolmo build/kolmobench build/kolmotest build/kolmoparallel build/kolmoresource

build/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEFS) $(INC) -c $< -o $@
build/obj/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) -I$(BSC) -c $< -o $@
build/obj/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEFS) $(INC) -c $< -o $@

build/libvendor.a: $(OBJ)
	ar rcs $@ $^
build/kolmo.o: src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES)
	@mkdir -p build
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) -c src/kolmo.c -o $@
build/libkolmo.a: build/kolmo.o
	ar rcs $@ $^
build/kolmo: src/cli.c build/libkolmo.a build/libvendor.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS) $(CXXLIB)
build/kolmobench: bench/kolmobench.c build/libkolmo.a build/libvendor.a
	$(CC) $(CFLAGS) $(DEFS) $(INC) $^ -o $@ $(BLIBS) $(CXXLIB)
build/kolmotest: tests/kolmotest.c build/libkolmo.a build/libvendor.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS) $(CXXLIB)
build/kolmoparallel: tests/kolmoparallel.c build/libkolmo.a build/libvendor.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS) $(CXXLIB)
build/kolmoresource: tests/kolmoresource.c tests/legacy_frames.h src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/kolmoresource.c build/libvendor.a -o $@ $(LIBS) $(CXXLIB)
build/kolmo-profile: src/cli.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) -DKOLMO_TIMING $(INC) src/cli.c src/kolmo.c build/libvendor.a -o $@ $(LIBS) $(CXXLIB)

check: all
	python3 tests/test_fast_file.py
	./build/kolmoresource
	./build/kolmoparallel
	./build/kolmotest

SANFLAGS ?= -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
build/kolmoparallel-asan: tests/kolmoparallel.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/kolmoparallel.c src/kolmo.c build/libvendor.a -o $@ $(LIBS) $(CXXLIB)
build/kolmoresource-asan: tests/kolmoresource.c tests/legacy_frames.h src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/kolmoresource.c build/libvendor.a -o $@ $(LIBS) $(CXXLIB)
build/test-lines-asan: tests/test_lines.c src/kolmo.c src/kolmo.h src/lines.inc build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/test_lines.c build/libvendor.a -o $@ $(LIBS) $(CXXLIB)
check-lines: build/test-lines-asan
	./build/test-lines-asan
check-sanitizers: build/kolmoparallel-asan build/kolmoresource-asan
	./build/kolmoresource-asan
	./build/kolmoparallel-asan

# Shared library for language bindings (Python, Rust): everything rebuilt -fPIC into build/pic.
SOEXT := $(if $(filter Darwin,$(UNAME_S)),dylib,so)
PICC  := $(patsubst %.c,build/pic/%.o,$(ZSRC) $(BROSRC) vendor/xxhash/kolmo_xxh3.c $(LZ4)/lz4.c $(LZ4)/lz4hc.c $(BSC)/bwt/libsais/libsais.c) $(patsubst %.S,build/pic/%.o,$(ZASM))
PICPP := $(patsubst %.cpp,build/pic/%.o,$(BSRC))
build/pic/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fPIC $(DEFS) $(INC) -c $< -o $@
build/pic/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CFLAGS) -fPIC -I$(BSC) -c $< -o $@
build/pic/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -fPIC $(DEFS) $(INC) -c $< -o $@
lib: build/libkolmo.$(SOEXT)
build/libkolmo.$(SOEXT): src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) $(PICC) $(PICPP)
	@mkdir -p build
	$(CC) $(CFLAGS) $(FPFLAGS) -fPIC -shared $(DEFS) $(INC) src/kolmo.c $(PICC) $(PICPP) -o $@ $(LIBS) $(CXXLIB)

clean:
	rm -rf build
.PHONY: all clean lib check check-sanitizers

# Optional lower-memory archive encoder.
build/kolmo-cm20: src/cli.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) -DCM_BB_MAX=20 $(DEFS) $(INC) src/cli.c src/kolmo.c build/libvendor.a -o $@ $(LIBS) $(CXXLIB)

# The compact variant retains the standard Ultra bitstream and table sizes elsewhere.
build/kolmo-cm-compact: src/cli.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) -DKOLMO_CM_COMPACT $(DEFS) $(INC) src/cli.c src/kolmo.c build/libvendor.a -o $@ $(LIBS) $(CXXLIB)

build/cm-reference.o: tests/test_cm_compact.c src/cm.inc
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

build/cm-compact-test: tests/test_cm_compact.c src/cm.inc build/cm-reference.o
	$(CC) $(CFLAGS) -DKOLMO_CM_COMPACT $< build/cm-reference.o -lpthread -o $@

check-cm-compact: build/cm-compact-test
	./build/cm-compact-test
.PHONY: check-cm-compact

# Research build. Experimental recipes emit v6 frames only when used.
FRONTIER_SRC := src/experimental/frontier_dna.c src/experimental/frontier_table.c src/experimental/frontier_prefix.c src/experimental/frontier_pcm.c src/experimental/frontier_lag.c src/experimental/frontier_shuffle.c src/experimental/frontier_runs.c
FRONTIER_HDR := src/experimental/frontier_dna.h src/experimental/frontier_table.h src/experimental/frontier_prefix.h src/experimental/frontier_pcm.h src/experimental/frontier_lag.h src/experimental/frontier_shuffle.h src/experimental/frontier_runs.h
build/kolmo-frontier: src/cli.c src/kolmo.c src/kolmo.h $(wildcard src/*.inc) $(FRONTIER_SRC) $(FRONTIER_HDR) build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) -DKOLMO_FRONTIER $(INC) src/cli.c src/kolmo.c $(FRONTIER_SRC) build/libvendor.a -o $@ $(LIBS) $(CXXLIB)

build/kolmo-tar-file-test: tests/test_tar_file.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) $(FRONTIER_SRC) $(FRONTIER_HDR) build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) -DKOLMO_FRONTIER $(INC) $< $(FRONTIER_SRC) build/libvendor.a -o $@ $(LIBS) $(CXXLIB)

check-tar-files: build/kolmo-tar-file-test
	./build/kolmo-tar-file-test
.PHONY: check-tar-files

check-frontier-dna: build/kolmo build/kolmo-frontier
	python3 tests/test_frontier_dna.py $^
.PHONY: check-frontier-dna

build/frontier-test: tests/frontier.c $(FRONTIER_SRC) $(FRONTIER_HDR)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SANFLAGS) tests/frontier.c $(FRONTIER_SRC) -o $@


build/libkolmo-frontier.$(SOEXT): src/kolmo.c src/kolmo.h $(wildcard src/*.inc) $(FRONTIER_SRC) $(FRONTIER_HDR) $(PICC) $(PICPP)
	$(CC) $(CFLAGS) $(FPFLAGS) -fPIC -shared $(DEFS) -DKOLMO_FRONTIER $(INC) src/kolmo.c $(FRONTIER_SRC) $(PICC) $(PICPP) -o $@ $(LIBS) $(CXXLIB)

# Combined release-candidate inverse differential gate (generic fallback retained).
build/test-int-inverse-fast: tests/test_int_inverse_fast.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libvendor.a -o $@ $(LIBS) $(CXXLIB)
build/test-int-inverse-fast-asan: tests/test_int_inverse_fast.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libvendor.a -o $@ $(LIBS) $(CXXLIB)
check-int-inverse-fast: build/test-int-inverse-fast
	./build/test-int-inverse-fast
.PHONY: check-int-inverse-fast

build/test-rc-options: tests/test_rc_options.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libvendor.a -o $@ $(LIBS) $(CXXLIB)
build/test-rc-options-asan: tests/test_rc_options.c src/kolmo.c src/kolmo.h $(KOLMO_INCLUDES) build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) $< build/libvendor.a -o $@ $(LIBS) $(CXXLIB)
check-rc-options: build/test-rc-options
	./build/test-rc-options
.PHONY: check-rc-options
