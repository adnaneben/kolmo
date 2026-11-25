"""Tool configurations for the performance graph.

Every configuration runs on ONE thread. A configuration is a dict with:
  tool    - colour key in the plot (one colour per tool)
  config  - stable id, used as the checkpoint key
  label   - short text for the chart
  enc/dec - argv templates; {in} {out} {rt} {arch} {kolmo} {dir} {name} are substituted
  stdin   - (enc, dec): feed {in} / {out} on stdin instead of as an argument
  stdout  - (enc, dec): capture stdout into {out} / {rt}
  rt_path - where the decoder leaves the restored file (default {rt})
  kind    - "file": compressed output is the single file {out}
"""

KOLMO_LEVELS = [str(n) for n in range(1, 10)] + ["ultra"]


def kolmo_configs(snapshot, binary):
    out = []
    for lv in KOLMO_LEVELS:
        lvarg = ["--level", "ultra"] if lv == "ultra" else ["-" + lv]
        out.append(dict(
            tool="kolmo", snapshot=snapshot, binary=binary, level=lv,
            config=f"kolmo-{snapshot}-L{lv}", label=("U" if lv == "ultra" else lv),
            enc=["{kolmo}", "-q", "-T", "1", *lvarg, "-c", "{in}"],
            dec=["{kolmo}", "-q", "-d", "-T", "1", "-c", "{out}"],
            stdout=(True, True)))
    return out


def _pipe(tool, cid, label, enc, dec, level):
    return dict(tool=tool, config=cid, label=label, level=level, enc=enc, dec=dec, stdout=(True, True))


COMPETITORS = [
    _pipe("lz4", "lz4-1", "lz4", ["lz4", "-q", "-1", "-T1", "-c", "{in}"], ["lz4", "-q", "-d", "-c", "{out}"], "1"),
    *[_pipe("zstd", f"zstd-{lv}", f"zstd -{lv}",
            ["zstd", "-q", "-T1", *(["--ultra"] if lv == "22" else []), f"-{lv}", "-c", "{in}"],
            ["zstd", "-q", "-d", "-c", "{out}"], lv) for lv in ("1", "3", "9", "19", "22")],
    *[_pipe("brotli", f"brotli-{q}", f"brotli -{q}", ["brotli", "-q", q, "-c", "{in}"],
            ["brotli", "-d", "-c", "{out}"], q) for q in ("1", "5", "9", "11")],
    _pipe("xz", "xz-6", "xz -6", ["xz", "-6", "-T1", "-c", "{in}"], ["xz", "-d", "-T1", "-c", "{out}"], "6"),
    _pipe("xz", "xz-9e", "xz -9e", ["xz", "-9e", "-T1", "-c", "{in}"], ["xz", "-d", "-T1", "-c", "{out}"], "9e"),
    dict(tool="7z-lzma2", config="7z-lzma2-mx9", label="7z LZMA2", level="9",
         enc=["7zz", "a", "-t7z", "-m0=LZMA2", "-mx9", "-mmt1", "-bso0", "-bsp0", "{out}", "{in}"],
         dec=["7zz", "e", "-mmt1", "-so", "-bsp0", "{out}"], stdout=(False, True)),
    dict(tool="7z-ppmd", config="7z-ppmd-mx9", label="7z PPMd", level="9",
         enc=["7zz", "a", "-t7z", "-m0=PPMd", "-mx9", "-mmt1", "-bso0", "-bsp0", "{out}", "{in}"],
         dec=["7zz", "e", "-mmt1", "-so", "-bsp0", "{out}"], stdout=(False, True)),
    _pipe("bzip3", "bzip3", "bzip3", ["bzip3", "-e", "-j", "1", "-c", "{in}"], ["bzip3", "-d", "-j", "1", "-c", "{out}"], "-"),
    # bsc 3.4.0 from Homebrew is not linked with OpenMP: single-threaded by construction.
    dict(tool="bsc", config="bsc", label="bsc", level="-",
         enc=["bsc", "e", "{in}", "{out}"], dec=["bsc", "d", "{out}", "{rt}"], stdout=(False, False)),
    # zpaq stores the member name: archive the file by its bare name from its own directory.
    *[dict(tool="zpaq", config=f"zpaq-m{m}", label=f"zpaq -m{m}", level=m,
           enc=["zpaq", "a", "{out}", "{name}", f"-m{m}", "-t1"],
           dec=["zpaq", "x", "{out}", "-to", "{rtdir}", "-t1"],
           stdout=(False, False), enc_cwd="{dir}", rt_path="{rtdir}/{name}")
      for m in ("4", "5")],
]
