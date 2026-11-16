"""Summarise prof.py output: per case, the share of Kolmo-owned code and its top functions (markdown table).

usage: python3 -I benchmark/rc3/simd-arm/prof_summary.py OUT_DIR/summary.txt
Vendored code (zstd, LZ4, Brotli, liblzma, libbsc, libsais, xxHash) and system libraries are told apart by name.
"""
import re
import sys

VENDOR = re.compile(r"^(ZSTD_|HUF_|FSE_|HIST_|LZ4_|XXH|lzma_|bt_|rc_|get_literal_price|move_pos|libsais_|bsc_|Brotli|"
                    r"CreateBackward|ProcessCommands|SafeProcessCommands|ContextBlockSplitter|StoreSymbol|ReadHuffmanCode|"
                    r"ExtendLastCommand|OUTLINED|DYLD|_platform|__bzero|_qsort|log2)")
cases, cur = [], None
for line in open(sys.argv[1]):
    m = re.match(r"== (\S+) (\S+) (\S)\s+\[(.*)\]\s+samples (\d+)", line)
    if m:
        cur = {"in": m.group(1), "mode": m.group(2), "dir": m.group(3), "rows": []}
        cases.append(cur)
        continue
    m = re.match(r"\s+([\d.]+)%\s+(.+?)\s+\((.+)\)\s*$", line)
    if m and cur is not None:
        cur["rows"].append((float(m.group(1)), m.group(2).strip(), m.group(3)))
print("| input | mode | dir | Kolmo-owned (of the top functions) | top Kolmo functions |")
print("|---|---|---|---:|---|")
for c in cases:
    own = [(p, f) for p, f, lib in c["rows"] if lib == "proploop" and not VENDOR.match(f)]
    tot = sum(p for p, _ in own)
    top = ", ".join(f"`{f}` {p:.0f}%" for p, f in own[:3] if p >= 2)
    print(f"| {c['in']} | {c['mode']} | {'compress' if c['dir'] == 'c' else 'decode'} | {tot:.0f}% | {top or '—'} |")
