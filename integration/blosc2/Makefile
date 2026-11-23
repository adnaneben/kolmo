# Blosc2 codec plugin for Kolmo.
#   make            build/plugin/libblosc2_kolmo.so (self-contained: libkolmo linked in) and
#                   build/libblosc2_kolmo.a (to link into an application that calls blosc2_kolmo_register)
#   make test       round trips through c-blosc2, super-chunk frame, dynamic plugin lookup
#   make bench      single-thread benchmark (see README.md); BENCH_ARGS="file:typesize ..." picks the inputs
# c-blosc2 is found with pkg-config (blosc2) or BLOSC2_DIR=/prefix.
ROOT    := ../..
UNAME_S := $(shell uname -s)
CC      ?= cc
comma   := ,
KOLMO_ARCH ?=
KMAKE    = $(MAKE) -s --no-print-directory -C $(ROOT) -f Makefile -f integration/common/kolmo-objs.mk ARCH="$(KOLMO_ARCH)"

ifdef BLOSC2_DIR
BLOSC2_CFLAGS ?= -I$(BLOSC2_DIR)/include
BLOSC2_LIBS   ?= -L$(BLOSC2_DIR)/lib -lblosc2 -Wl,-rpath,$(BLOSC2_DIR)/lib
else
BLOSC2_OK     := $(shell pkg-config --exists blosc2 2>/dev/null && echo 1)
BLOSC2_CFLAGS ?= $(if $(BLOSC2_OK),$(shell pkg-config --cflags blosc2))
BLOSC2_LIBDIR := $(if $(BLOSC2_OK),$(patsubst -L%,%,$(filter -L%,$(shell pkg-config --libs blosc2))))
BLOSC2_LIBS   ?= $(if $(BLOSC2_OK),$(shell pkg-config --libs blosc2) $(addprefix -Wl$(comma)-rpath$(comma),$(BLOSC2_LIBDIR)))
endif

ifeq ($(UNAME_S),Darwin)
# The plugin does not link libblosc2 (only blosc2_kolmo_register calls into it): it uses the Blosc2 already loaded.
SO_LDFLAGS  := -dynamiclib -undefined dynamic_lookup -Wl,-exported_symbol,_info -Wl,-exported_symbol,_blosc2_kolmo_encoder \
               -Wl,-exported_symbol,_blosc2_kolmo_decoder -Wl,-exported_symbol,_blosc2_kolmo_register
else
SO_LDFLAGS  := -shared -Wl,--version-script=exports.map
endif
# Named .so on every Unix, macOS included: c-blosc2 opens a registered plugin as dlopen("libblosc2_<name>.so").
PLUGIN := build/plugin/libblosc2_kolmo.so
STATIC := build/libblosc2_kolmo.a
KOLMO_SRC := $(ROOT)/src/kolmo.c $(ROOT)/src/kolmo.h $(wildcard $(ROOT)/src/*/*.[ch] $(ROOT)/src/*/*/*.[chS] $(ROOT)/src/*/*.inc)

all: $(PLUGIN) $(STATIC)

check-blosc2:
	@test -n "$(BLOSC2_CFLAGS)" || { echo "c-blosc2 not found: install it (brew install c-blosc2, apt install libblosc2-dev) or set BLOSC2_DIR=/prefix"; exit 1; }

build/kolmo_pic.o: $(KOLMO_SRC)
	@mkdir -p build
	@$(KMAKE) kolmo-pic-objs
	$(CC) $$($(KMAKE) kolmo-print-cflags) -fPIC -I$(ROOT)/src -c $(ROOT)/src/kolmo.c -o $@

build/blosc2_kolmo.o: blosc2_kolmo.c blosc2_kolmo.h | check-blosc2
	@mkdir -p build
	$(CC) -O2 -fPIC -Wall -Wextra -I$(ROOT)/src $(BLOSC2_CFLAGS) -c blosc2_kolmo.c -o $@

$(PLUGIN): build/blosc2_kolmo.o build/kolmo_pic.o exports.map
	@mkdir -p build/plugin
	$(CC) build/blosc2_kolmo.o build/kolmo_pic.o $(addprefix $(ROOT)/,$(shell $(KMAKE) kolmo-print-objs)) \
	    $(SO_LDFLAGS) -lpthread -lm -o $@

$(STATIC): build/blosc2_kolmo.o build/kolmo_pic.o
	rm -f $@
	ar rcs $@ build/blosc2_kolmo.o build/kolmo_pic.o $(addprefix $(ROOT)/,$(shell $(KMAKE) kolmo-print-objs))

build/test_blosc2_kolmo: tests/test_blosc2_kolmo.c $(STATIC)
	$(CC) -O2 -Wall -Wextra $(BLOSC2_CFLAGS) tests/test_blosc2_kolmo.c $(STATIC) $(BLOSC2_LIBS) -lpthread -lm -ldl -o $@

build/bench_blosc2: bench/bench_blosc2.c $(STATIC)
	$(CC) -O2 -Wall $(BLOSC2_CFLAGS) bench/bench_blosc2.c $(STATIC) $(BLOSC2_LIBS) -lpthread -lm -o $@

build/test_dynamic_load: tests/test_dynamic_load.c | check-blosc2
	@mkdir -p build
	$(CC) -O2 -Wall -Wextra $(BLOSC2_CFLAGS) tests/test_dynamic_load.c $(BLOSC2_LIBS) -lm -o $@

# LIBPATH_VAR: where dlopen looks for a bare library name.
LIBPATH_VAR := $(if $(filter Darwin,$(UNAME_S)),DYLD_LIBRARY_PATH,LD_LIBRARY_PATH)
test: build/test_blosc2_kolmo build/test_dynamic_load $(PLUGIN)
	./build/test_blosc2_kolmo $(PLUGIN)
	$(LIBPATH_VAR)=$(CURDIR)/build/plugin ./build/test_dynamic_load

bench: build/bench_blosc2
	./build/bench_blosc2 $(BENCH_ARGS)

clean:
	rm -rf build
.PHONY: all test bench clean check-blosc2
