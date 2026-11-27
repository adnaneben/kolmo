"""Independent deterministic edge-property tests for experimental NPY wrapper.

This uses a deliberately labelled in-process zlib MOCK to check only frame,
metadata and exact-byte inverse contracts. It is not an OpenZL benchmark.
"""
import itertools
import random
import unittest
import zlib
from pathlib import Path
from unittest.mock import patch
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "N1-exact-npy"))
import npy_openzl as adapter

SEED = 20261113


def npy(payload, dtype, shape, major=1, *, order=0, extra=b''):
    meta = ("{'descr': %r, 'fortran_order': False, 'shape': %r, }"
            % (dtype, shape)).encode('ascii')
    if order:
        meta = ("{'shape': %r, 'fortran_order': False, 'descr': %r, }"
                % (shape, dtype)).encode('ascii')
    prefix = 10 if major == 1 else 12
    if major == 3:
        meta += b' ' + extra
    meta += b'\n'
    meta = meta[:-1] + b' ' * (-(prefix + len(meta)) % 64) + b'\n'
    return (b'\x93NUMPY' + bytes((major, 0))
            + len(meta).to_bytes(2 if major == 1 else 4, 'little')
            + meta + payload)


def mock_zli(_binary, action, source, destination, **kwargs):
    data = source.read_bytes()
    if action == 'compress':
        assert kwargs['profile'] in {
            f'{e}-{t}{width}' for e in ('le', 'be')
            for t in ('u', 'i') for width in (16, 32, 64)
        }
        assert kwargs['level'] in (6, 7, 9)
        result = zlib.compress(data, 1)
    else:
        result = zlib.decompress(data)
    destination.write_bytes(result)


class ExactNpyContractRegression(unittest.TestCase):
    def setUp(self):
        self.mock = patch.object(adapter, 'zli_action', side_effect=mock_zli)
        self.mock.start()
        self.addCleanup(self.mock.stop)
        self.zli = Path('/mock/zli-not-a-real-executable')

    def test_144_profile_endian_version_lexical_roundtrips(self):
        """144 byte-exact cases across every registered width/sign/endian."""
        rng = random.Random(SEED)
        count = 0
        for endian, kind, width, major, order, level in itertools.product(
                '<>', 'iu', (2, 4, 8), (1, 2, 3), (0, 1), (6, 7)):
            dtype = endian + kind + str(width)
            payload = rng.randbytes(width * 7)
            original = npy(payload, dtype, (7,), major, order=order)
            h, n, profile = adapter.npy_header(original)
            self.assertEqual((h + n, profile), (len(original),
                ('le-' if endian == '<' else 'be-') + kind + str(width * 8)))
            encoded = adapter.encode(original, self.zli, level)
            self.assertEqual(encoded[adapter.FRAME.size:adapter.FRAME.size+h], original[:h])
            self.assertEqual(adapter.decode(encoded, self.zli), original)
            count += 1
        self.assertEqual(count, 144)

    def test_64_shape_and_empty_exactness(self):
        rng = random.Random(SEED + 1)
        for trial in range(64):
            width = (2, 4, 8)[trial % 3]
            shape = [(), (0,), (1,), (3, 2), (1, 1, 1, 2), (2, 0, 999)][trial % 6]
            cells = 1
            for d in shape: cells *= d
            payload = rng.randbytes(cells * width)
            original = npy(payload, '<u' + str(width), shape, 1 + trial % 3)
            self.assertEqual(adapter.decode(adapter.encode(original, self.zli, 9), self.zli), original)

    def test_frame_field_mutations_are_rejected(self):
        original = npy(bytes(range(24)), '>i8', (3,), 2)
        encoded = adapter.encode(original, self.zli, 6)
        self.assertEqual(adapter.decode(encoded, self.zli), original)
        # Header identity / byte-exact original checks, including shape, frame lengths,
        # SHA and trailing data. No mutation is allowed to return output silently.
        positions = (0, 8, 16, 20, 24, 28, 32, 40, 50,
                     adapter.FRAME.size + 8, adapter.FRAME.size + 12,
                     adapter.FRAME.size + 30, len(encoded) - 1)
        for pos in positions:
            with self.subTest(position=pos):
                damaged = bytearray(encoded)
                damaged[pos] ^= 0x40
                with self.assertRaises((adapter.FormatError, zlib.error)):
                    adapter.decode(bytes(damaged), self.zli)
        for data in (encoded[:-1], encoded + b'\0', b'', b'\0' * adapter.FRAME.size):
            with self.assertRaises(adapter.FormatError):
                adapter.decode(data, self.zli)

    def test_shape_overflow_and_unapproved_kinds(self):
        for dtype,shape,payload in (
            ('<u4', (8_388_609,), b''),
            ('<u8', (2**40,), b''),
            ('<u4', (-1,), b''),
            ('<u4', (True,), b''),
            ('<f4', (1,), b'\0' * 4),
            ('|u1', (1,), b'\0'),
            ('<u4', (2,), b'\0' * 4),
        ):
            raw = npy(payload, dtype, shape)
            with self.subTest(dtype=dtype, shape=shape):
                with self.assertRaises(adapter.FormatError):
                    adapter.npy_header(raw)

    def test_no_extra_footer_or_hidden_payload_allowed(self):
        raw = npy(bytes(range(16)), '<u4', (4,))
        for mutated in (raw + b'\0', raw[:-1], raw + b'\xff' * 16):
            with self.assertRaises(adapter.FormatError):
                adapter.encode(mutated, self.zli, 7)


if __name__ == '__main__':
    unittest.main()
