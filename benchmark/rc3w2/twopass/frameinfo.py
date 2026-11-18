"""Prints the version and the codec of each block of a Kolmo frame (CZN1): python3 -I frameinfo.py FILE.kol"""
import sys, struct
b = open(sys.argv[1], 'rb').read()
HDR, IDX = 32, 16
nb = struct.unpack_from('<I', b, 20)[0]
codecs = [b[HDR + i * IDX + 12] for i in range(nb)]
print(f'version={b[4]} mode={b[5]} blocks={nb} codecs={",".join(map(str, codecs))}')
