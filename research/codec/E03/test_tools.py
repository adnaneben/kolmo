"""Synthetic tests of evidence checks; never reported as codec results."""
import copy
import unittest
from analyse import compare, index_rows
from run import expected_size, CHOICE

class Tests(unittest.TestCase):
    def test_expected_baseline(self):
        sizes = {f'{m}:{b}': 100 + m * 10 + i for m in range(5)
                 for i, b in enumerate(('stored', 'huffman', 'rans'))}
        sizes['0:huffman'] = 80; sizes['0:rans'] = 70; sizes['2:huffman'] = 60
        case = {'previous_sizes': sizes}
        self.assertEqual(expected_size(case, 0, 0), (70, 70))
        self.assertEqual(expected_size(case, 1, 2), (70, 60))
        self.assertEqual(expected_size(case, 2, 0), (80, 80))
        self.assertEqual(expected_size(case, 3, 0), (70, 70))
    def test_regression_rejected(self):
        case = {'previous_sizes': {f'{m}:{b}': 50 + m * 20 for m in (0, 3)
                                  for b in ('stored', 'huffman', 'rans')}}
        self.assertEqual(expected_size(case, 1, 3), (50, 50))
    def test_duplicates(self):
        row = {'id': 'x', 'policy': 'native_base'}
        with self.assertRaises(ValueError): index_rows([row, row], 2)
    def test_incomplete(self):
        with self.assertRaises(ValueError): index_rows([], 232)
    def test_unknown(self):
        with self.assertRaises(ValueError): index_rows([{'id': 'x', 'policy': 'unknown'}], 1)
    def test_confirmation(self):
        row = {x: 1 for x in (*CHOICE, 'input_sha256', 'frame_sha256', 'capture', 'order', 'gcd')}
        a = {('x', 'native_base'): row}; compare(a, copy.deepcopy(a))
        for field in row:
            b = copy.deepcopy(a); b[('x', 'native_base')][field] = 2
            with self.assertRaises(ValueError): compare(a, b)
        with self.assertRaises(ValueError): compare(a, {})

if __name__ == '__main__': unittest.main()
