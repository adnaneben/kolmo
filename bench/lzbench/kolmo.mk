# kolmo
CODECS += KOLMO
KOLMO_UPSTREAM := git-tags https://github.com/adnaneben/kolmo

# misc/kolmo/src is kolmo's src/ directory, unchanged from the release tag (see misc/kolmo/README.md).
# Every coder is kolmo's own; the one piece of third-party code, a libsais subset in src/algo/sais, has its symbols
# renamed to kolmo_sais_*, so it does not collide with the libsais inside bsc.
KOLMO_DIR := misc/kolmo/src

# Kolmo needs a 64-bit little-endian target; it is not built or tested on Windows.
ifeq ($(TARGET_BIG_ENDIAN),1)
    DONT_BUILD_KOLMO ?= 1
endif
ifneq ($(shell echo|$(CC) $(CODE_FLAGS) -dM -E - 2>/dev/null|grep -c '__SIZEOF_POINTER__ 8'), 1)
    DONT_BUILD_KOLMO ?= 1
endif
ifeq ($(TARGET_WINDOWS),1)
    DONT_BUILD_KOLMO ?= 1
endif

# src/kolmo.c includes the rest of the library into one translation unit; the kernels below have public kolmo_*
# entry points and are compiled on their own (the lists of kolmo's Makefile: KSRC, BWTSRC, NATSRC, ASMSRC).
KOLMO_OBJS := $(addprefix $(KOLMO_DIR)/, kolmo.o \
    arch/cpu.o algo/predict.o algo/planes.o algo/mixer.o algo/video.o \
    algo/lzp.o algo/qlfc.o algo/par.o algo/sais/sais.o codecs/bwt_stream.o \
    algo/khuf.o algo/krans.o algo/klz.o algo/klzma.o algo/khash.o)
ifneq (,$(filter x86_64% amd64%,$(TARGET_ARCH)))
    KOLMO_OBJS += $(addprefix $(KOLMO_DIR)/arch/x86_64/, med8_sse2.o planes_sse2.o mixer_avx2.o video_sse2.o \
        img16_sse2.o med8f_avx2.o hist_x86_64.o)
else ifneq (,$(filter arm64% aarch64%,$(TARGET_ARCH)))
    KOLMO_OBJS += $(KOLMO_DIR)/arch/aarch64/hist_aarch64.o
else
    KOLMO_ASM_FLAGS := -DKOLMO_NO_ASM
endif

# Kolmo's encoders decide some choices with floating-point cost estimates, and its frames are specified to be the
# same on every machine and compiler. -ffast-math (in lzbench's OPT_FLAGS) or FMA contraction could change those
# choices, so they are turned off for kolmo's objects: the output is then byte for byte what the kolmo command
# line writes. (The SIMD kernels pick their instruction set by CPUID at run time, so no -m flags are needed.)
KOLMO_FLAGS := -I$(SRC)$(KOLMO_DIR) -fno-fast-math -ffp-contract=off $(KOLMO_ASM_FLAGS)
LDFLAGS += -lm
