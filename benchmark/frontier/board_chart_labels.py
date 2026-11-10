LABELS={"m1_fast":"Kolmo fast · M1","kolmo_rt":"Kolmo rt","kolmo_fast":"Kolmo fast · PR #3",
        "kolmo_eco":"Kolmo eco","kolmo_hc":"Kolmo hc","kolmo_max":"Kolmo max","kolmo_ultra":"Kolmo ultra",
        "kbc":"Kolmo KBC1","krc":"Kolmo KRC1","lz4":"LZ4","zstd3":"Zstd −3","zstd19":"Zstd −19",
        "openzl":"OpenZL generic","brotli11":"Brotli −11","zpaq":"ZPAQ −5","bsc":"libbsc","lzma9":"LZMA −9",
        "kanzi9":"Kanzi −9","lzav2":"LZAV −2","bzip3":"bzip3 −5"}

LABELS.update(candidate="Kolmo fast · M1",baseline="Kolmo fast · PR #3",release="Kolmo fast · v1.0.0",
    zstd="Zstd −3",naf1="NAF −1",naf19="NAF −19",flac0="FLAC −0",flac5="FLAC −5",flac8="FLAC −8",
    alac="ALAC",ffv1="FFV1 v3",adaptive="Adaptive CPU + Metal",**{"x264-lossless":"x264 lossless",
    "av1-lossless":"AV1 lossless","jpegxl-e7":"JPEG XL · effort 7","webp-m4":"WebP · method 4","png-6":"PNG −6",
    "legacy-cpu1":"Original · one CPU","legacy-cpu10":"Original · ten CPUs","legacy-metal":"Original · Metal",
    "legacy-split50":"Original · fixed CPU/GPU split","parallel-cpu10":"Parallel pack · ten CPUs",
    "parallel-metal":"Parallel pack · Metal","parallel-split50":"Parallel pack · fixed split"})
