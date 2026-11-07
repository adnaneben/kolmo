# kolmo — structure-aware lossless compressor (https://github.com/adnaneben/kolmo)
CODECS += KOLMO

# libkolmo_iso.a is kolmo's core, its vendored codecs and a static liblzma, partially linked into a
# single object whose only global symbols are czn_*; it is built by kolmo's
# benchmark/lzbench/setup.sh with the same -O3 -march=native flags lzbench uses here.
KOLMO_OBJS  := misc/kolmo/kolmo_lzbench.o misc/kolmo/libkolmo_iso.a
KOLMO_FLAGS := -I$(SRC)misc/kolmo
