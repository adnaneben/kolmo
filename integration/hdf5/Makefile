# HDF5 filter plugin for Kolmo.
#   make            build/plugin/libH5Zkolmo.{dylib,so}: one self-contained file (libkolmo linked in)
#   make test       C round-trip test, then the h5py test if h5py is installed (skipped otherwise)
#   make bench      single-thread benchmark (see README.md); BENCH_ARGS="file:dtype ..." picks the inputs
# HDF5 is found with pkg-config (hdf5, or hdf5-serial on Debian/Ubuntu) or HDF5_DIR=/prefix.
# The plugin does not link libhdf5: its HDF5 symbols resolve to the HDF5 already loaded in the process.
ROOT    := ../..
UNAME_S := $(shell uname -s)
CC      ?= cc
comma   := ,
# Kolmo is built portable (ARCH empty) by default, as for the Python wheels; KOLMO_ARCH=-mcpu=native for this CPU only.
KOLMO_ARCH ?=
KMAKE    = $(MAKE) -s -C $(ROOT) -f Makefile -f integration/common/kolmo-objs.mk ARCH="$(KOLMO_ARCH)"

ifdef HDF5_DIR
HDF5_CFLAGS ?= -I$(HDF5_DIR)/include
HDF5_LIBS   ?= -L$(HDF5_DIR)/lib -lhdf5 -Wl,-rpath,$(HDF5_DIR)/lib
else
HDF5_PC     := $(shell pkg-config --exists hdf5 2>/dev/null && echo hdf5 || (pkg-config --exists hdf5-serial 2>/dev/null && echo hdf5-serial))
HDF5_CFLAGS ?= $(if $(HDF5_PC),$(shell pkg-config --cflags $(HDF5_PC)))
HDF5_LIBDIR := $(if $(HDF5_PC),$(patsubst -L%,%,$(filter -L%,$(shell pkg-config --libs $(HDF5_PC)))))
HDF5_LIBS   ?= $(if $(HDF5_PC),$(shell pkg-config --libs $(HDF5_PC)) $(addprefix -Wl$(comma)-rpath$(comma),$(HDF5_LIBDIR)))
endif

ifeq ($(UNAME_S),Darwin)
SOEXT       := dylib
SO_LDFLAGS  := -dynamiclib -undefined dynamic_lookup -Wl,-exported_symbol,_H5PLget_plugin_type \
               -Wl,-exported_symbol,_H5PLget_plugin_info
PLUGIN_LDFLAGS := $(SO_LDFLAGS) -Wl,-exported_symbol,_H5Z_kolmo_register
else
SOEXT       := so
SO_LDFLAGS  := -shared
PLUGIN_LDFLAGS := -shared -Wl,--version-script=exports.map
endif
PLUGIN   := build/plugin/libH5Zkolmo.$(SOEXT)
ZSTDPLUG := build/bench-plugins/libH5Zzstd_bench.$(SOEXT)

all: $(PLUGIN)

check-hdf5:
	@test -n "$(HDF5_CFLAGS)" || { echo "HDF5 not found: install it (brew install hdf5, apt install libhdf5-dev) or set HDF5_DIR=/prefix"; exit 1; }

KOLMO_SRC := $(ROOT)/src/kolmo.c $(ROOT)/src/kolmo.h $(wildcard $(ROOT)/src/*/*.[ch] $(ROOT)/src/*/*/*.[chS] $(ROOT)/src/*/*.inc)
build/kolmo_pic.o: $(KOLMO_SRC)
	@mkdir -p build
	@$(KMAKE) kolmo-pic-objs
	$(CC) $$($(KMAKE) kolmo-print-cflags) -fPIC -I$(ROOT)/src -c $(ROOT)/src/kolmo.c -o $@

$(PLUGIN): H5Zkolmo.c H5Zkolmo.h build/kolmo_pic.o exports.map | check-hdf5
	@mkdir -p build/plugin
	$(CC) -O2 -fPIC -Wall -Wextra -I$(ROOT)/src $(HDF5_CFLAGS) H5Zkolmo.c build/kolmo_pic.o \
	    $(addprefix $(ROOT)/,$(shell $(KMAKE) kolmo-print-objs)) $(PLUGIN_LDFLAGS) -lpthread -lm -o $@

build/test_h5zkolmo: tests/test_h5zkolmo.c H5Zkolmo.h | check-hdf5
	@mkdir -p build
	$(CC) -O2 -Wall -Wextra $(HDF5_CFLAGS) tests/test_h5zkolmo.c -o $@ $(HDF5_LIBS)

# A minimal zstd filter (ID 32015, as hdf5plugin's) for the benchmark only.
$(ZSTDPLUG): bench/H5Zzstd_bench.c | check-hdf5
	@mkdir -p build/bench-plugins
	$(CC) -O2 -fPIC $(HDF5_CFLAGS) $$(pkg-config --cflags libzstd) bench/H5Zzstd_bench.c $(SO_LDFLAGS) \
	    $$(pkg-config --libs libzstd) -o $@

build/bench_hdf5: bench/bench_hdf5.c H5Zkolmo.h | check-hdf5
	@mkdir -p build
	$(CC) -O2 -Wall $(HDF5_CFLAGS) bench/bench_hdf5.c -o $@ $(HDF5_LIBS)

test: $(PLUGIN) build/test_h5zkolmo
	HDF5_PLUGIN_PATH=$(CURDIR)/build/plugin ./build/test_h5zkolmo
	@if python3 -c "import h5py, numpy" 2>/dev/null; then \
	    HDF5_PLUGIN_PATH=$(CURDIR)/build/plugin python3 tests/test_h5py.py; \
	else echo "SKIP h5py test: h5py and numpy are not installed (pip install h5py numpy)"; fi

bench: $(PLUGIN) $(ZSTDPLUG) build/bench_hdf5
	HDF5_PLUGIN_PATH=$(CURDIR)/build/plugin:$(CURDIR)/build/bench-plugins ./build/bench_hdf5 $(BENCH_ARGS)

clean:
	rm -rf build
.PHONY: all test bench clean check-hdf5
