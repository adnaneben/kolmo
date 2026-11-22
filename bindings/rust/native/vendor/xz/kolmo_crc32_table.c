/* Written for Kolmo. The vendored liblzma subset builds its LZ match finders' hash from lzma_crc32_table[0]
 * (lz/lz_encoder_hash.h, CRC32_GENERIC). Upstream defines that table in check/crc32_fast.c, together with the CRC32
 * function and its CPU-specific variants, none of which Kolmo needs. This file defines only the table, from the
 * unmodified generated headers, exactly as crc32_fast.c does under CRC32_GENERIC. */
#include "check.h"
#include "crc_common.h"
#ifdef WORDS_BIGENDIAN
#	include "crc32_table_be.h"
#else
#	include "crc32_table_le.h"
#endif
