# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
import os, struct, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
import nga

def clip(cid, dur, counts): return struct.pack('<2HIfI', 12, cid, 0, dur, len(counts)) + b'\0\0' + struct.pack(f'<{len(counts)}H', *counts) + bytes(8)

class T(unittest.TestCase):
    def test_two_clips(self):
        a, b = clip(12, 120., [5, 2]), clip(24, 40., [9])
        d = struct.pack('<I2I2I', 0xc61798, 1, 20, 2, 20 + len(a)) + a + b
        m, C = nga.parse_nga(d); self.assertEqual((m, len(C)), (0xc61798, 2))
        self.assertEqual((C[0]['dur'], C[0]['counts'], C[1]['id'], C[1]['size']), (120., (5, 2), 24, len(b)))
    def test_continued_ids(self):
        a = clip(1, 1., [1]); d = struct.pack('<I2I', 7, 198, 12) + a
        self.assertEqual(nga.parse_nga(d)[1][0]['index'], 198)

if __name__ == '__main__': unittest.main()
