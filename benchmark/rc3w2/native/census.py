"""Census tables from profile_modes.py output (native lane, rc3 wave 2).

usage: python3 benchmark/rc3w2/native/census.py results/libprof-X.tsv [...]
Per mode: output bytes, compress and decompress wall time, and per library the share of compress time, of decompress
time, and of the output bytes (compressed bytes read by that library's decoder calls, divided by the output size).
Libraries: zstd (frames; HUF calls inside zstd are part of zstd), HUF (Kolmo's direct Huffman leaves, codec 13),
LZ4, Brotli, LZMA2 (liblzma), XXH3 (block checksums), XXH64 (zstd's copy), ZDICT; a column is shown only when the
runs of that file reach the library."""
import sys, collections
LIB = {"ZSTD_compressCCtx": "zstd", "ZSTD_compress": "zstd", "ZSTD_compress2": "zstd", "ZSTD_compress_usingDict": "zstd",
       "ZSTD_decompressDCtx": "zstd", "ZSTD_decompress_usingDict": "zstd", "HUF_compress4X_repeat": "HUF",
       "HUF_decompress4X_hufOnly_wksp": "HUF", "LZ4_compress_fast": "LZ4", "LZ4_decompress_safe": "LZ4",
       "LZ4_compress_HC_extStateHC": "LZ4", "LZ4_compress_default": "LZ4", "BrotliEncoderCompress": "Brotli",
       "BrotliDecoderDecompress": "Brotli", "kolmo_lzma_raw_buffer_encode": "LZMA2", "kolmo_lzma_raw_buffer_decode": "LZMA2",
       "kolmo_xxh3": "XXH3", "ZSTD_XXH64": "XXH64", "ZDICT_trainFromBuffer": "ZDICT"}
ORDER = ["zstd", "HUF", "LZ4", "Brotli", "LZMA2", "XXH3", "XXH64", "ZDICT"]
for path in sys.argv[1:]:
    rows = [l.rstrip("\n").split("\t") for l in open(path)][1:]
    modes = collections.OrderedDict()
    for m, ph, e, calls, by, ms, outb, zb in rows:
        d = modes.setdefault(m, {"c": collections.defaultdict(float), "d": collections.defaultdict(float), "z": collections.defaultdict(int), "calls": collections.defaultdict(int)})
        if e == "_total_": d["wall" + ph] = float(ms); d["out"] = int(outb); d["in"] = int(by); continue
        lib = LIB.get(e)
        if not lib: continue
        d[ph][lib] += float(ms)
        if ph == "d": d["z"][lib] += int(zb or 0)
        if ph == "c": d["calls"][lib] += int(calls)
    used = [l for l in ORDER if any(d["c"][l] + d["d"][l] + d["z"][l] > 0 for d in modes.values())]   # libraries reached
    print(f"\n{path}\n")
    print("| mode | output bytes | compress s | decompress s | " + " | ".join(f"{l} c / d / bytes" for l in used) + " |")
    print("|---|---|---|---|" + "---|" * len(used))
    for m, d in modes.items():
        cells = []
        for l in used:
            c = 100 * d["c"][l] / d["wallc"]; dd = 100 * d["d"][l] / d["walld"]; z = 100 * d["z"][l] / d["out"]
            cells.append("—" if c < 0.05 and dd < 0.05 and z < 0.05 else f"{c:.1f}% / {dd:.1f}% / {z:.1f}%")
        print(f"| {m} | {d['out']:,} | {d['wallc'] / 1000:.2f} | {d['walld'] / 1000:.2f} | " + " | ".join(cells) + " |")
