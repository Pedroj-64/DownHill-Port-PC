# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
import os, struct, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import ptsext

def blob(n, stride=32): return struct.pack('<II', ptsext.MAGIC, n) + b''.join(struct.pack('<4f', i, 2 * i, 3 * i, 0) + bytes(stride - 16) for i in range(n))

class T(unittest.TestCase):
    def test_points(self):
        P = ptsext.parse_points(blob(3), 'APT'); self.assertEqual(len(P), 3); self.assertEqual(P[2]['pos'], (2, 4, 6))
    def test_rpl_stride(self): self.assertEqual(len(ptsext.parse_points(blob(2, 48), 'RPL')), 2)
    def test_bad_size(self):
        with self.assertRaises(ValueError): ptsext.parse_points(blob(3) + b'x', 'APT')
    def test_bad_magic(self):
        with self.assertRaises(ValueError): ptsext.parse_points(b'\0' * 8, 'APT')
    def test_hdt(self):
        d = struct.pack('<I', 2) + struct.pack('<IHH', 1, 0, 4) + struct.pack('<IHH', 1, 4, 2) + bytes(16)
        g, rest = ptsext.parse_hdt(d); self.assertEqual(g[1], (1, 4, 2)); self.assertEqual(len(rest), 16)
    def test_hdt_noncontiguous(self):
        with self.assertRaises(ValueError): ptsext.parse_hdt(struct.pack('<I', 2) + struct.pack('<IHH', 1, 0, 4) + struct.pack('<IHH', 1, 5, 2))

if __name__ == '__main__': unittest.main()
