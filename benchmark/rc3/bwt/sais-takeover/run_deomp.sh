cd /tmp/claude-0/wip && cp sais_pre.c sais.c && python3 ../tools/deomp.py sais.c \
 "libsais_count_and_gather_lms_suffixes_8u_omp=m:sa_sint_t" \
 "libsais_count_and_gather_lms_suffixes_32s_4k_fs_omp=m:sa_sint_t" \
 "libsais_count_and_gather_lms_suffixes_32s_2k_fs_omp=m:sa_sint_t" \
 "libsais_count_and_gather_lms_suffixes_32s_4k_nofs_omp=m:sa_sint_t" \
 "libsais_count_and_gather_lms_suffixes_32s_2k_nofs_omp=m:sa_sint_t" \
 "libsais_count_and_gather_compacted_lms_suffixes_32s_2k_nofs_omp=m:sa_sint_t" \
 "libsais_partial_sorting_scan_left_to_right_8u_block_omp=block_threads:sa_sint_t;d:sa_sint_t" \
 "libsais_partial_sorting_scan_right_to_left_8u_block_omp=block_threads:sa_sint_t;d:sa_sint_t" \
 "libsais_final_bwt_aux_scan_left_to_right_8u_block_omp=block_threads:sa_sint_t" \
 "libsais_final_bwt_aux_scan_right_to_left_8u_block_omp=block_threads:sa_sint_t" \
 "libsais_renumber_lms_suffixes_8u_omp=name:sa_sint_t" \
 "libsais_renumber_distinct_lms_suffixes_32s_4k_omp=name:sa_sint_t" \
 "libsais_gather_nonunique_lms_suffixes_omp=groups:fast_sint_t" \
 "libsais_sort_nonunique_lms_suffixes_omp=SAm:sa_sint_t * RESTRICT;budget:fast_sint_t;capacity:fast_sint_t;deferred:fast_sint_t;scratch:sa_sint_t * RESTRICT;slots:fast_sint_t" \
 "libsais_renumber_unique_and_nonunique_lms_suffixes_32s_omp=f:sa_sint_t" \
 "libsais_unbwt_decode_omp=blocks:fast_sint_t;remainder:fast_uint_t" \
 "libsais_unbwt_decode_split#1=U_chunks:fast_uint_t;chains_taken:fast_uint_t;chunks_taken:fast_uint_t;count:fast_uint_t;first_shift:fast_uint_t;first_symbols:const uint8_t * RESTRICT;shift:fast_uint_t;symbol_ends:const sa_uint_t * RESTRICT" \
 "libsais_unbwt_decode_split#3=A:uint8_t * RESTRICT" \
 "libsais_find_block_end_left_to_right_omp=block_end:fast_sint_t;block_scan_end:fast_sint_t" "libsais_find_block_end_right_to_left_omp=block_end:fast_sint_t;block_scan_end:fast_sint_t" \
 "libsais_unbwt_init_parallel=bucket1:sa_uint_t[ALPHABET_SIZE];symbol_start:fast_uint_t;symbol_end:fast_uint_t;shift:fast_uint_t" \
 "libsais_partial_sorting_scan_left_to_right_32s_6k_block_omp=d:sa_sint_t" "libsais_partial_sorting_scan_left_to_right_32s_4k_block_omp=d:sa_sint_t" "libsais_partial_sorting_scan_right_to_left_32s_6k_block_omp=d:sa_sint_t" "libsais_partial_sorting_scan_right_to_left_32s_4k_block_omp=d:sa_sint_t" \
 "libsais_partial_sorting_shift_markers_8u_omp=temp_bucket:const sa_sint_t * RESTRICT;prefetch_distance:const fast_sint_t" "libsais_partial_sorting_shift_markers_32s_6k_omp=temp_bucket:const sa_sint_t * RESTRICT;prefetch_distance:const fast_sint_t" \
 "libsais_unbwt_decode_split#0=count:fast_uint_t" "libsais_unbwt_decode_split#2=count:fast_uint_t;cycle:fast_uint_t;A:uint8_t * RESTRICT;U_chunks:fast_uint_t" \
 && python3 ../tools/hdr.py sais.c
