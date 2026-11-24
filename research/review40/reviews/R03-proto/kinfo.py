# R03: list the block index of a Kolmo CZN1 frame (SPEC.md: header 32 B, 16 B per block, 4-byte plan prefix)
import sys, struct
b = open(sys.argv[1], 'rb').read()
assert b[:4] == b'CZN1', b[:4]
ver, mode = b[4], b[5]; orig, = struct.unpack_from('<Q', b, 12); nb, blk = struct.unpack_from('<II', b, 20)
print(f"version {ver} mode {mode} orig {orig} nblocks {nb} block {blk}")
off = 32 + 16 * nb
for i in range(nb):
    raw, comp, ck, codec, tflag = struct.unpack_from('<IIIBB', b, 32 + 16 * i)
    plan = tuple(b[off:off + 4])
    print(f"  block {i}: raw {raw} comp {comp} codec {codec} tflag {tflag} plan {plan} ratio {comp/raw:.4f}")
    off += comp
