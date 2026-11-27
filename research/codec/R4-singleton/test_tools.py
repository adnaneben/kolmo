"""Independent singleton-table/terminal-state and bounded parsing checks."""
import struct, unittest
from run import chunks, read_uv

def uv(n):
    out=bytearray()
    while n>=128: out.append((n&127)|128);n>>=7
    out.append(n);return bytes(out)
def one(symbol=0,count=100):
    table=(bytes((0,symbol-1)) if symbol else b'')+uv(65536)+(bytes((0,254-symbol)) if symbol<255 else b'')
    return uv(count)+table+uv(32)+struct.pack('<4Q',*((1<<31,)*4))
class ParseTests(unittest.TestCase):
    def test_all_symbols(self):
        for s in range(256):
            r=chunks(one(s));self.assertEqual(r[0]['singleton_symbol'],s);self.assertEqual(r[0]['raw_bytes'],100)
    def test_boundary_lengths(self):
        for n in (1,2,3,4,127,128,65535,65536,1048576):self.assertEqual(chunks(one(23,n))[0]['raw_bytes'],n)
    def test_multiple_chunks(self):self.assertEqual(len(chunks(one(0)+one(255))),2)
    def test_truncations(self):
        p=one(127)
        for cut in range(1,len(p)):
            with self.assertRaises(ValueError):chunks(p[:cut])
    def test_wrong_states(self):
        p=bytearray(one())
        for k in range(4):
            q=p[:];struct.pack_into('<Q',q,len(q)-32+k*8,(1<<31)+1)
            with self.assertRaises(ValueError):chunks(q)
    def test_extra_renormalization_word(self):
        p=bytearray(one());p[-33]=36;p.extend(b'\0'*4)
        with self.assertRaises(ValueError):chunks(p)
    def test_chunk_bounds(self):
        for n in (0,1048577):
            with self.assertRaises(ValueError):chunks(one(count=n))
    def test_bad_frequency_sum(self):
        with self.assertRaises(ValueError):chunks(uv(1)+uv(65535)+bytes((0,254))+uv(32)+struct.pack('<4Q',*((1<<31,)*4)))
    def test_varint_overflow(self):
        with self.assertRaises(ValueError):read_uv(b'\xff'*10,0)
    def test_varint_roundtrip(self):
        for v in (0,1,127,128,65536,2**63,2**64-1):self.assertEqual(read_uv(uv(v),0),(v,len(uv(v))))
    def test_state_identity_algebra(self):
        # At full-scale frequency the inverse leaves the state unchanged.
        for x in (1<<31,(1<<31)+1,2**48+65535,2**64-1):self.assertEqual(65536*(x>>16)+(x&65535),x)
if __name__=='__main__':unittest.main()
