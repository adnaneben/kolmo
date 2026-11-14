"""Synthetic arithmetic tests, not extra empirical compression cases."""
import unittest
from analyse import interaction
class Interactions(unittest.TestCase):
 def test_additive(self):self.assertEqual(interaction({0:100,1:90,2:80,3:70},1,2)['additive_extra_saving_bytes'],0)
 def test_synergy(self):self.assertEqual(interaction({0:100,1:95,2:98,3:85},1,2)['additive_extra_saving_bytes'],8)
 def test_overlap(self):self.assertEqual(interaction({0:100,1:90,2:80,3:80},1,2)['additive_extra_saving_bytes'],-10)
 def test_multiplicative(self):self.assertEqual(interaction({0:100,1:90,2:80,3:72},1,2)['extra_vs_multiplicative_bytes'],0)
 def test_enablement(self):self.assertEqual(interaction({0:100,2:120,4:100,6:80},2,4)['additive_extra_saving_bytes'],40)
 def test_conditioning(self):self.assertEqual(interaction({4:100,5:90,6:80,7:60},1,2,4)['additive_extra_saving_bytes'],10)
 def test_invalid(self):
  with self.assertRaises(ValueError):interaction({0:0,1:1,2:1,3:1},1,2)
if __name__=='__main__':unittest.main()
