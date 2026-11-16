/* Kolmo's build configuration for the vendored liblzma subset (xz-utils 5.8.1, LZMA2 raw encoder and decoder only).
 * Written for Kolmo; replaces the config.h that xz-utils' configure script would generate. It enables exactly what
 * the subset holds (the LZMA1/LZMA2 coders and every match finder that lzma_lzma_preset() can select) and no
 * container, check, BCJ or delta filter. No threads: Kolmo calls the single-threaded raw buffer API only.
 * Values follow what configure detects with GCC or Clang on x86-64 and AArch64 (Linux and macOS). */
#define HAVE_ENCODER_LZMA1 1
#define HAVE_DECODER_LZMA1 1
#define HAVE_ENCODER_LZMA2 1
#define HAVE_DECODER_LZMA2 1
#define HAVE_MF_HC3 1
#define HAVE_MF_HC4 1
#define HAVE_MF_BT2 1
#define HAVE_MF_BT3 1
#define HAVE_MF_BT4 1
#define HAVE_STDBOOL_H 1
#define HAVE__BOOL 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE___BUILTIN_BSWAPXX 1
#define HAVE___BUILTIN_ASSUME_ALIGNED 1
#define SIZEOF_SIZE_T __SIZEOF_SIZE_T__
#define ASSUME_RAM 128
#define PACKAGE "xz"
#define PACKAGE_NAME "XZ Utils"
#define PACKAGE_URL "https://tukaani.org/xz/"
#define PACKAGE_BUGREPORT "xz@tukaani.org"
#define TUKLIB_SYMBOL_PREFIX lzma_
#if defined(__x86_64__) || defined(__i386__) || defined(__aarch64__) || defined(__ARM_FEATURE_UNALIGNED) || defined(__powerpc64__) || defined(__s390x__)
#define TUKLIB_FAST_UNALIGNED_ACCESS 1
#endif
#if defined(__SSE2__) && (defined(__x86_64__) || defined(__i386__))
#define HAVE_IMMINTRIN_H 1
#define HAVE__MM_MOVEMASK_EPI8 1
#endif
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define WORDS_BIGENDIAN 1
#endif
#ifndef NDEBUG
#define NDEBUG 1      /* as configure does without --enable-debug */
#endif
