# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
import os, struct, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import ngpgrid

class T(unittest.TestCase):
    def test_grid(self):
        n = struct.pack('<I2H5f', 15, 2, 2, 100., -10., -20., 190., 180.) + bytes(0x30 - 28) + struct.pack('<4I', 0, (3 << 10) | 2, (5 << 10) | 7, 0)
        g = ngpgrid.parse_grid(n, 0); self.assertEqual((g['w'], g['h'], g['size']), (2, 2, 0x40)); self.assertEqual(g['cells'][1:3], [(3, 2), (5, 7)])
    def test_probes(self):
        c = (16 << 11) | (8 << 6) | (4 << 1)
        n = struct.pack('<II3f', 45, 1, 0, 0, 0) + struct.pack('<f', 1.) + struct.pack('<3hH', -5, 6, 7, c)
        p = ngpgrid.parse_probes(n, 0); self.assertEqual(p['probes'][0], (-5, 6, 7, 0.25, 0.5, 1.0)); self.assertEqual(p['size'], len(n))
    def test_wrong_type(self):
        with self.assertRaises(ValueError): ngpgrid.parse_grid(struct.pack('<I2H', 1, 1, 1) + bytes(64), 0)

if __name__ == '__main__': unittest.main()
