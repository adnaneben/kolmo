# E02C experimental build (research only; the production Makefile, library and CLI are unchanged).
# Run from the repository root:
#   make -f research/codec/E02C/Makefile            -> build/e02c (optimised, kernel/E2E measurements)
#   make -f research/codec/E02C/Makefile check      -> ASan/UBSan/LSan selftest + golden vectors + independent Python reference
CC      ?= gcc
E02C_CFLAGS ?= -O3 -march=native -fno-plt
SANFLAGS ?= -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined
FPFLAGS := -ffp-contract=off
DEFS    := -DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0
INC     := -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/libbsc -Ivendor/brotli/include
LIBS    := -llzma -lpthread -lm -lstdc++
H       := research/codec/E02C/harness
SRCS    := $(H)/e02c.c $(H)/selftest.inc src/kolmo.c src/kolmo.h $(wildcard src/*.inc)
VEC     ?= build/e02c-vectors

all: build/e02c
build/libvendor.a:
	$(MAKE) build/libvendor.a
build/e02c: $(SRCS) build/libvendor.a
	$(CC) $(E02C_CFLAGS) $(FPFLAGS) $(DEFS) $(INC) $(H)/e02c.c build/libvendor.a -o $@ $(LIBS)
build/e02c-asan: $(SRCS) build/libvendor.a
	$(CC) $(SANFLAGS) $(FPFLAGS) $(DEFS) $(INC) $(H)/e02c.c build/libvendor.a -o $@ $(LIBS)
check: build/e02c-asan build/e02c
	rm -rf $(VEC) && mkdir -p $(VEC)
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ./build/e02c-asan selftest $(VEC) $(H)/golden.txt
	./build/e02c selftest "" $(H)/golden.txt
	python3 -I $(H)/reference.py $(VEC)
.PHONY: all check
