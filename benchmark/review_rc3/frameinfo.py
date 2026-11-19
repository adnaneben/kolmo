#!/usr/bin/env python3
"""Read-only Kolmo frame inspector (review harness). Reports what a frame actually uses: container version, mode,
per-block codec / tflag / plan type, and for graph blocks (codec 12) the leaf codecs. Follows SPEC.md at RC3
(CZN1 one-shot, CZNS stream of CZN1 frames). Unknown magics are reported, not guessed.
usage: frameinfo.py FILE [--json]"""
import struct, sys, json, collections
NAMES = {0: "STORE", 1: "LZ4", 2: "LZ4HC", 3: "ZSTD", 4: "BROTLI", 5: "BSC", 6: "LZMA2", 7: "CM", 12: "GRAPH", 13: "HUF",
         14: "RANS", 15: "NPLANE", 16: "FQFAM", 17: "CMI", 18: "BWT", 19: "AUD", 20: "VID", 21: "PLANES", 22: "HZ",
         23: "ZRUN", 24: "KLZMA", 26: "GR6", 27: "KHUF", 28: "KLZ", 29: "KLZH", 30: "C30"}
NATIVE = {24, 27, 28, 29}
def uv(b, i):
    r = s = 0
    while True:
        c = b[i]; i += 1; r |= (c & 127) << s; s += 7
        if c < 128: return r, i
def graph_leaves(p):
    """p = block payload after the 4-byte plan prefix; returns list of leaf codecs or an error string"""
    try:
        if p[0:1] != b"G": return "not-G"
        i = 2; nodes, i = uv(p, i); ops = []
        for _ in range(nodes):
            op = p[i]; i += 1; ops.append(op); nin, i = uv(p, i)
            for _ in range(nin): _, i = uv(p, i)
            nout, i = uv(p, i); pl, i = uv(p, i); i += pl
        nl, i = uv(p, i); leaves = []
        for _ in range(nl):
            _, i = uv(p, i); c = p[i]; i += 1; _, i = uv(p, i); cl, i = uv(p, i); i += cl; leaves.append(c)
        return {"ops": ops, "leaves": leaves}
    except IndexError:
        return "truncated-graph"
def container(b, off=0):
    if b[off:off + 4] != b"CZN1": return {"error": "magic %r" % b[off:off + 4]}
    ver, mode, _, _, _, _, flags, memd = b[off + 4:off + 12]
    orig, nb, blk, crc = struct.unpack_from("<QIII", b, off + 12)
    pos = off + 32 + 16 * nb; blocks = []
    for k in range(nb):
        raw, comp, ck, codec, tflag = struct.unpack_from("<IIIBB", b, off + 32 + 16 * k)
        pay = b[pos:pos + comp]; plan = pay[0] if comp else None
        e = {"raw": raw, "comp": comp, "codec": codec, "tflag": tflag, "plan": plan}
        if codec == 12 and comp > 4: e["graph"] = graph_leaves(pay[4:])
        blocks.append(e); pos += comp
    return {"version": ver, "mode": mode, "flags": flags, "memd": memd, "orig": orig, "nblocks": nb, "block": blk,
            "blocks": blocks, "end": pos}
def inspect(b):
    if b[:4] == b"CZNS":
        frames, i = [], 12
        while i + 8 <= len(b):
            (n,) = struct.unpack_from("<Q", b, i); i += 8
            if n == 0: break
            frames.append(container(b, i)); i += n
        return {"stream": True, "frames": frames}
    return {"stream": False, "frames": [container(b)]}
def summary(info):
    bc, lc = collections.Counter(), collections.Counter(); vers = set()
    for f in info["frames"]:
        if "error" in f: bc[f["error"]] += 1; continue
        vers.add(f["version"])
        for e in f["blocks"]:
            bc[NAMES.get(e["codec"], str(e["codec"]))] += 1
            g = e.get("graph")
            if isinstance(g, dict):
                for c in g["leaves"]: lc[NAMES.get(c, str(c))] += 1
            elif g: lc[g] += 1
    used = set(bc) | set(lc)
    return {"stream": info["stream"], "versions": sorted(vers), "block_codecs": dict(bc), "leaf_codecs": dict(lc),
            "native_used": sorted(n for n in used if n in {NAMES[c] for c in NATIVE})}
if __name__ == "__main__":
    info = inspect(open(sys.argv[1], "rb").read())
    print(json.dumps(info if "--json" in sys.argv else summary(info)))
