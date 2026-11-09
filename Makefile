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
build/czn.o: src/czn.c src/czn.h src/tbl2.inc src/log.inc src/sci.inc src/word.inc src/cm.inc src/stream.inc
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

# Opt-in experiments; existing libraries, formats and default targets are unchanged.
build/krc.o: src/experimental/krc.c src/experimental/krc.h
	@mkdir -p build
	$(CC) $(CFLAGS) -std=c11 -Isrc/experimental -c $< -o $@
build/krctest: tests/krctest.c build/krc.o
	$(CC) $(CFLAGS) -Isrc/experimental $^ -lpthread -o $@
build/krctest-asan: tests/krctest.c src/experimental/krc.c src/experimental/krc.h
	$(CC) $(SANFLAGS) -Isrc/experimental tests/krctest.c src/experimental/krc.c -lpthread -o $@
build/krcbench-cpu: bench/krcbench.c build/krc.o build/libczn.a build/libvendor.a
	$(CC) $(CFLAGS) $(INC) -Isrc/experimental $^ -o $@ $(LIBS) $(CXXLIB)
ifeq ($(UNAME_S),Darwin)
build/krc_gpu.o: src/experimental/krc_gpu.m src/experimental/krc_gpu.h src/experimental/krc.h
	$(CC) $(CFLAGS) -fobjc-arc -Isrc/experimental -c $< -o $@
build/krcbench-gpu-main.o: bench/krcbench.c src/experimental/krc.h src/experimental/krc_gpu.h
	$(CC) $(CFLAGS) $(INC) -Isrc/experimental -DKRC_HAVE_METAL -c $< -o $@
build/krcbench-gpu: build/krcbench-gpu-main.o build/krc.o build/krc_gpu.o build/libczn.a build/libvendor.a
	$(CC) $^ -o $@ $(LIBS) $(CXXLIB) -framework Foundation -framework Metal -framework CoreGraphics
endif
experimental-check: build/krctest build/krctest-asan
	./build/krctest
	./build/krctest-asan
.PHONY: experimental-check

build/krc: src/experimental/krc_cli.c build/krc.o
	$(CC) $(CFLAGS) -Isrc/experimental $^ -lpthread -o $@
ifeq ($(UNAME_S),Darwin)
build/krc_hybrid.o: src/experimental/krc_hybrid.c src/experimental/krc_gpu.h src/experimental/krc.h
	$(CC) $(CFLAGS) -Isrc/experimental -c $< -o $@
build/krc-metal-main.o: src/experimental/krc_cli.c src/experimental/krc.h src/experimental/krc_gpu.h
	$(CC) $(CFLAGS) -Isrc/experimental -DKRC_HAVE_METAL -c $< -o $@
build/krc-metal: build/krc-metal-main.o build/krc.o build/krc_gpu.o build/krc_hybrid.o
	$(CC) $^ -lpthread -framework Foundation -framework Metal -framework CoreGraphics -o $@
build/krcbench-gpu: build/krc_hybrid.o
endif
ifeq ($(UNAME_S),Darwin)
build/krcgputest: tests/krcgputest.c build/krc.o build/krc_gpu.o build/krc_hybrid.o
	$(CC) $(CFLAGS) -Isrc/experimental $^ -lpthread -framework Foundation -framework Metal -framework CoreGraphics -o $@
endif

# Independent byte-context research; preserves all previous targets and formats.
build/kbc.o: src/experimental/kbc.c src/experimental/kbc.h src/experimental/krc.h
	@mkdir -p build
	$(CC) $(CFLAGS) -std=c11 -Isrc/experimental -c $< -o $@
build/kbctest: tests/kbctest.c build/kbc.o build/krc.o
	$(CC) $(CFLAGS) -Isrc/experimental $^ -lpthread -o $@
build/kbctest-asan: tests/kbctest.c src/experimental/kbc.c src/experimental/krc.c src/experimental/kbc.h src/experimental/krc.h
	$(CC) $(SANFLAGS) -Isrc/experimental tests/kbctest.c src/experimental/kbc.c src/experimental/krc.c -lpthread -o $@
byte-context-check: build/kbctest build/kbctest-asan
	./build/kbctest
	./build/kbctest-asan
.PHONY: byte-context-check
build/kbc: src/experimental/kbc_cli.c build/kbc.o build/krc.o
	$(CC) $(CFLAGS) -Isrc/experimental $^ -lpthread -o $@
