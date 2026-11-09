# kolmo — structure-aware compressor.  make -> build/kolmo, build/czbench, build/libczn.a
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
OBJ     := $(patsubst %.c,build/obj/%.o,$(ZSRC) $(BROSRC) vendor/xxhash/czn_xxh3.c $(LZ4)/lz4.c $(LZ4)/lz4hc.c $(BSC)/bwt/libsais/libsais.c) $(patsubst %.S,build/obj/%.o,$(ZASM)) \
           $(patsubst %.cpp,build/obj/%.o,$(BSRC))
ifeq ($(NO_LZMA),1)
DEFS    += -DCZN_NO_LZMA
LZMALIB :=
else
LZMALIB := -llzma
endif
LIBS    := $(EXTRA_LIB) $(LZMALIB) -lpthread -lm
BLIBS   := $(LIBS)

all: build/kolmo build/czbench build/czntest build/cznparallel build/cznresource

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
build/czn.o: src/czn.c src/czn.h src/tbl2.inc src/log.inc src/sci.inc src/word.inc src/cm.inc src/stream.inc src/kgraph.inc
	@mkdir -p build
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) -c src/czn.c -o $@
build/libczn.a: build/czn.o
	ar rcs $@ $^
build/kolmo: src/cli.c build/libczn.a build/libvendor.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS) $(CXXLIB)
build/czbench: bench/czbench.c build/libczn.a build/libvendor.a
	$(CC) $(CFLAGS) $(DEFS) $(INC) $^ -o $@ $(BLIBS) $(CXXLIB)
build/czntest: tests/czntest.c build/libczn.a build/libvendor.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS) $(CXXLIB)
build/cznparallel: tests/cznparallel.c build/libczn.a build/libvendor.a
	$(CC) $(CFLAGS) $(INC) $^ -o $@ $(LIBS) $(CXXLIB)
build/cznresource: tests/cznresource.c tests/legacy_frames.h src/czn.c src/czn.h src/tbl2.inc src/log.inc src/sci.inc src/word.inc src/cm.inc src/stream.inc build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/cznresource.c build/libvendor.a -o $@ $(LIBS) $(CXXLIB)
build/kolmo-profile: src/cli.c src/czn.c src/czn.h src/tbl2.inc src/log.inc src/sci.inc src/word.inc src/cm.inc src/stream.inc build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) -DCZN_TIMING $(INC) src/cli.c src/czn.c build/libvendor.a -o $@ $(LIBS) $(CXXLIB)

check: all
	python3 tests/test_fast_file.py
	./build/cznresource
	./build/cznparallel
	./build/czntest

SANFLAGS ?= -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
build/cznparallel-asan: tests/cznparallel.c src/czn.c src/czn.h src/tbl2.inc src/log.inc src/sci.inc src/word.inc src/cm.inc src/stream.inc build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/cznparallel.c src/czn.c build/libvendor.a -o $@ $(LIBS) $(CXXLIB)
build/cznresource-asan: tests/cznresource.c tests/legacy_frames.h src/czn.c src/czn.h src/tbl2.inc src/log.inc src/sci.inc src/word.inc src/cm.inc src/stream.inc build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) tests/cznresource.c build/libvendor.a -o $@ $(LIBS) $(CXXLIB)
check-sanitizers: build/cznparallel-asan build/cznresource-asan
	./build/cznresource-asan
	./build/cznparallel-asan

# Shared library for language bindings (Python, Rust): everything rebuilt -fPIC into build/pic.
SOEXT := $(if $(filter Darwin,$(UNAME_S)),dylib,so)
PICC  := $(patsubst %.c,build/pic/%.o,$(ZSRC) $(BROSRC) vendor/xxhash/czn_xxh3.c $(LZ4)/lz4.c $(LZ4)/lz4hc.c $(BSC)/bwt/libsais/libsais.c) $(patsubst %.S,build/pic/%.o,$(ZASM))
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
build/libkolmo.$(SOEXT): src/czn.c src/czn.h src/tbl2.inc src/log.inc src/sci.inc src/word.inc src/cm.inc src/stream.inc $(PICC) $(PICPP)
	@mkdir -p build
	$(CC) $(CFLAGS) $(FPFLAGS) -fPIC -shared $(DEFS) $(INC) src/czn.c $(PICC) $(PICPP) -o $@ $(LIBS) $(CXXLIB)

clean:
	rm -rf build
.PHONY: all clean lib check check-sanitizers

# Optional lower-memory archive encoder.
build/kolmo-cm20: src/cli.c src/czn.c src/czn.h src/cm.inc src/tbl2.inc src/log.inc src/sci.inc src/word.inc src/stream.inc build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) -DCM_BB_MAX=20 $(DEFS) $(INC) src/cli.c src/czn.c build/libvendor.a -o $@ $(LIBS) $(CXXLIB)

# Research build. Experimental recipes emit v6 frames only when used.
FRONTIER_SRC := src/experimental/frontier_dna.c src/experimental/frontier_table.c src/experimental/frontier_prefix.c src/experimental/frontier_pcm.c src/experimental/frontier_lag.c src/experimental/frontier_shuffle.c src/experimental/frontier_runs.c
FRONTIER_HDR := src/experimental/frontier_dna.h src/experimental/frontier_table.h src/experimental/frontier_prefix.h src/experimental/frontier_pcm.h src/experimental/frontier_lag.h src/experimental/frontier_shuffle.h src/experimental/frontier_runs.h
build/kolmo-frontier: src/cli.c src/czn.c src/czn.h $(wildcard src/*.inc) $(FRONTIER_SRC) $(FRONTIER_HDR) build/libvendor.a
	$(CC) $(CFLAGS) $(FPFLAGS) $(DEFS) -DCZN_FRONTIER $(INC) src/cli.c src/czn.c $(FRONTIER_SRC) build/libvendor.a -o $@ $(LIBS) $(CXXLIB)

build/frontier-test: tests/frontier.c $(FRONTIER_SRC) $(FRONTIER_HDR)
	@mkdir -p build
	$(CC) $(CFLAGS) $(SANFLAGS) tests/frontier.c $(FRONTIER_SRC) -o $@


build/libkolmo-frontier.$(SOEXT): src/czn.c src/czn.h $(wildcard src/*.inc) $(FRONTIER_SRC) $(FRONTIER_HDR) $(PICC) $(PICPP)
	$(CC) $(CFLAGS) $(FPFLAGS) -fPIC -shared $(DEFS) -DCZN_FRONTIER $(INC) src/czn.c $(FRONTIER_SRC) $(PICC) $(PICPP) -o $@ $(LIBS) $(CXXLIB)
