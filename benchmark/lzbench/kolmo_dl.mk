# kolmo — structure-aware lossless compressor (https://github.com/adnaneben/kolmo)
# macOS variant: installed as mk/kolmo.mk by kolmo's benchmark/lzbench/setup_macos.sh.
CODECS += KOLMO

# Only the wrapper is compiled here. It loads misc/kolmo/libkolmo.dylib (the release build) and
# misc/kolmo/libkolmo_ctl.dylib (the control build) at run time; both export only kolmo_*, so kolmo's
# vendored codecs stay apart from the copies lzbench builds for its own entries.
KOLMO_OBJS  := misc/kolmo/kolmo_lzbench_dl.o
KOLMO_FLAGS := -I$(SRC)misc/kolmo
